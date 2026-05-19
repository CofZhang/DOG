/**
 * @file system_control.c
 * @brief 系统控制模块实现 - 整合USB接收、SPI发送、FDCAN控制
 * @note 主控制流程：USB接收 → 解析 → FDCAN1直控(电机1-3) + SPI转发(电机4-12)
 */

#include "system_control.h"
#include "fdcan_handler.h"
#include "led_indicator.h"
#include "u_spi.h"
#include "tim.h"
#include <string.h>

/* ==================== 私有变量 ==================== */
/* 系统状态 */
static volatile SystemState g_system_state = SYS_STATE_INIT;

/* 电机控制参数缓存 */
 MotorControlParam g_motor_params[USB_PKG_MOTOR_CNT];

/* 电机反馈状态缓存 */
static MotorFeedback g_motor_feedback[USB_PKG_MOTOR_CNT];

/* 序列号 */
static uint8_t g_sequence = 0;

/* 反馈发送计数器 */
static uint32_t g_feedback_counter = 0;

/* 调试计数器：统计每个电机CAN发送成功/失败次数 */
volatile uint32_t g_can_tx_ok[3] = {0};
volatile uint32_t g_can_tx_fail[3] = {0};

/* TIM6驱动的CAN发送状态 */
static volatile uint8_t g_can_tx_index = 0;    /* 当前待发电机索引 0~2 */
static volatile uint8_t g_can_tx_pending = 0;  /* 是否有待发任务 */
static volatile uint8_t g_can_tx_error = 0;    /* 本轮发送是否有错误 */
static uint8_t g_can_tx_data[3][8];            /* 预打包的3帧CAN数据 */

/* 外部FDCAN句柄（在fdcan.c中定义） */
extern FDCAN_HandleTypeDef hfdcan1;

/* ==================== 系统初始化 ==================== */
void System_Control_Init(void)
{
    /* 初始化USB协议模块 */
    Protocol_Init();

    /* 初始化SPI协议模块 */
    SPI_Protocol_Init();

    /* 初始化FDCAN处理模块 */
    FDCAN_Handler_Init();

    /* 清空电机参数和反馈缓存 */
    memset(g_motor_params, 0, sizeof(g_motor_params));
    memset(g_motor_feedback, 0, sizeof(g_motor_feedback));

    /* 启动TIM6，用于CAN帧间1ms延迟 */
    HAL_TIM_Base_Start_IT(&htim6);

    /* 设置系统状态为空闲 */
    g_system_state = SYS_STATE_IDLE;
}

/* ==================== 系统主循环处理 ==================== */
void System_Control_Process(void)
{
    /* 检查是否有完整的USB数据包 */
    if (Protocol_HasCompletePacket()) {
        /* 处理电机控制数据 */
        System_ProcessMotorControl();
    }

    /* 定期发送反馈数据（每10次控制指令发送一次反馈） */
    if (g_feedback_counter >= 10) {
        g_feedback_counter = 0;
        System_SendFeedback();
    }
}

/* ==================== 处理电机控制数据 ==================== */
void System_ProcessMotorControl(void)
{
    /* 读取并解析USB数据包 */
    ProtocolState state = Protocol_ReadPacket(g_motor_params, &g_sequence);

    if (state != PROTOCOL_STATE_COMPLETE) {
        LED_Indicator_SetError(LED_IND_USB_ERROR, ERROR_LEVEL_WARNING);
        return;
    }

    /* 设置系统状态为运行中 */
    g_system_state = SYS_STATE_RUNNING;

    /* ==================== 步骤1：通过SPI发送电机4-12的数据到从控 ==================== */
    if (SPI_SendMotorControl(g_motor_params, g_sequence) != 1) {
        LED_Indicator_SetError(LED_IND_SPI_ERROR, ERROR_LEVEL_ERROR);
    } else {
        LED_Indicator_Trigger(LED_IND_SPI_TX);
        LED_Indicator_ClearError(LED_IND_SPI_ERROR);
    }

    /* ==================== 步骤2：预打包电机1-3的CAN数据，交由TIM6中断逐帧发送 ==================== */
    /* 等待上一轮发送完成（通常已完成，此处仅防止极端情况下覆盖） */
    while (g_can_tx_pending);

    for (int i = 0; i < 3; i++) {
        Motor_PackControlData(&g_motor_params[i], g_can_tx_data[i]);
    }

    /* 触发TIM6驱动的发送序列：从电机1开始 */
    g_can_tx_index = 0;
    g_can_tx_error = 0;
    g_can_tx_pending = 1;

    /* 增加反馈计数器 */
    g_feedback_counter++;
}

/* ==================== TIM6中断回调：每1ms发送一帧CAN ==================== */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM6) return;
    if (!g_can_tx_pending) return;

    uint8_t i = g_can_tx_index;

    FDCAN_TxHeaderTypeDef tx_header;
    tx_header.Identifier = Motor_GetMasterID(MOTOR_ID_1 + i);
    tx_header.IdType = FDCAN_STANDARD_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.DataLength = FDCAN_DLC_BYTES_8;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, g_can_tx_data[i]) != HAL_OK) {
        g_can_tx_fail[i]++;
        g_can_tx_error = 1;
    } else {
        g_can_tx_ok[i]++;
    }

    g_can_tx_index++;

    if (g_can_tx_index >= 3) {
        /* 3帧全部发完 */
        g_can_tx_pending = 0;

        if (!g_can_tx_error) {
            LED_Indicator_Trigger(LED_IND_FDCAN_TX);
            LED_Indicator_ClearError(LED_IND_FDCAN_ERROR);
        } else {
            LED_Indicator_SetError(LED_IND_FDCAN_ERROR, ERROR_LEVEL_ERROR);
        }
    }
}

/* ==================== 发送反馈数据 ==================== */
void System_SendFeedback(void)
{
    /* 获取主控 FDCAN1 收到的电机1-3反馈 */
    FDCAN_GetAllMotorFeedback(g_motor_feedback);

    /* 获取从机 SPI 收到的电机4-12反馈，合并到索引3-11 */
    SPI_GetSlaveFeedback(&g_motor_feedback[3]);

    /* 通过USB发送12个电机的完整反馈数据 */
    Protocol_SendFeedback(g_motor_feedback, g_sequence);
}

/* ==================== 系统状态管理 ==================== */
SystemState System_GetState(void)
{
    return g_system_state;
}
