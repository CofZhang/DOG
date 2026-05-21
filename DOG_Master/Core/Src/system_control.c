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
#include "motor_calib.h"
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

/* 电机掉线检测：连续未收到反馈的次数（每次 SendFeedback 检查一次） */
#define MOTOR_OFFLINE_THRESHOLD  20     /* 连续失败超过此次数认为掉线 */
static uint8_t  g_motor_offline_cnt[USB_PKG_MOTOR_CNT] = {0};  /* 各电机连续无反馈计数 */
static uint8_t  g_motor_offline[USB_PKG_MOTOR_CNT] = {0};      /* 各电机掉线标志 */
static uint32_t g_last_feedback_ts[USB_PKG_MOTOR_CNT] = {0};   /* 上次检测时的时间戳快照 */

/* TIM6驱动的CAN发送状态 */
static volatile uint8_t g_can_tx_index = 0;    /* 当前待发电机索引 0~2 */
static volatile uint8_t g_can_tx_pending = 0;  /* 是否有待发任务 */
static volatile uint8_t g_can_tx_error = 0;    /* 本轮发送是否有错误 */
static volatile uint8_t g_can_tx_delay_cnt = 0; /* ID3发送前的延迟计数（0.1ms×5=0.5ms） */
static uint8_t g_can_tx_data[3][8];            /* 预打包的3帧CAN数据 */

/* 外部FDCAN句柄（在fdcan.c中定义） */
extern FDCAN_HandleTypeDef hfdcan1;

/* 私有函数前向声明 */
static void CAN_SendFrame(uint8_t motor_index);
static void System_BootCalibSample(void);
static void System_CheckMotorOffline(void);

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

    /* 初始化电机校准模块（从Flash加载偏移量，开始上电位置采样） */
    Motor_Calib_Init();

    /* 启动TIM6，用于CAN帧间1ms延迟 */
    HAL_TIM_Base_Start_IT(&htim6);

    /* 设置系统状态为空闲 */
    g_system_state = SYS_STATE_IDLE;

    /* 主动发控制帧触发电机反馈，完成上电位置采样（独立于USB数据流） */
    System_BootCalibSample();
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
        MotorControlParam param = g_motor_params[i];
        Motor_Calib_ApplyTransform(i, &param);
        Motor_PackControlData(&param, g_can_tx_data[i]);
    }

    /* 触发TIM6驱动的发送序列：从电机1开始 */
    g_can_tx_index = 0;
    g_can_tx_error = 0;
    g_can_tx_pending = 1;

    /* 增加反馈计数器 */
    g_feedback_counter++;
}

/* ==================== 发送单帧CAN辅助函数 ==================== */
static void CAN_SendFrame(uint8_t motor_index)
{
    FDCAN_TxHeaderTypeDef tx_header;
    tx_header.Identifier = Motor_GetMasterID(MOTOR_ID_1 + motor_index);
    tx_header.IdType = FDCAN_STANDARD_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.DataLength = FDCAN_DLC_BYTES_8;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, g_can_tx_data[motor_index]) != HAL_OK) {
        g_can_tx_fail[motor_index]++;
        g_can_tx_error = 1;
    } else {
        g_can_tx_ok[motor_index]++;
    }
}

/* ==================== 上电启动采样：主动发控制帧触发电机反馈 ==================== */
/* 采样超时：最多等待 BOOT_SAMPLE_TIMEOUT_MS 毫秒，防止电机未连接时卡死 */
#define BOOT_SAMPLE_TIMEOUT_MS  5000U
#define BOOT_SAMPLE_INTERVAL_MS 20U    /* 每轮发帧间隔，给电机足够时间回复 */

static void System_BootCalibSample(void)
{
    /* 构造零扭矩查询帧：kp/velocity/torque=0，kd取最小非零值避免电机拒帧 */
    MotorControlParam query_param;
    query_param.kp       = 0.0f;
    query_param.kd       = 0.1f;
    query_param.position = 0.0f;
    query_param.velocity = 0.0f;
    query_param.torque   = 0.0f;

    /* 预打包电机1-3的CAN数据 */
    for (int i = 0; i < 3; i++) {
        Motor_PackControlData(&query_param, g_can_tx_data[i]);
    }

    /* 构造12个电机的零扭矩参数（用于SPI发送电机4-12） */
    MotorControlParam query_params[USB_PKG_MOTOR_CNT];
    for (int i = 0; i < USB_PKG_MOTOR_CNT; i++) {
        query_params[i] = query_param;
    }

    uint32_t start_tick = HAL_GetTick();

    while (Motor_Calib_GetState() == CALIB_STATE_COLLECTING ||
           Motor_Calib_GetState() == CALIB_STATE_CALIBRATING)
    {
        /* 超时保护：电机未连接时不卡死，直接跳过采样进入READY */
        if (HAL_GetTick() - start_tick > BOOT_SAMPLE_TIMEOUT_MS) {
            break;
        }

        /* 发送电机1-3控制帧（直接写FDCAN FIFO，不经过TIM6延迟机制） */
        CAN_SendFrame(0);
        CAN_SendFrame(1);
        HAL_Delay(1);   /* 给ID3留出0.5ms以上间隔，与正常发送逻辑一致 */
        CAN_SendFrame(2);

        /* 发送电机4-12控制帧（SPI全双工，同时接收上一轮从机反馈） */
        SPI_SendMotorControl(query_params, 0);

        /* 等待电机回复反馈帧（FDCAN中断会自动更新g_motor_feedback） */
        HAL_Delay(BOOT_SAMPLE_INTERVAL_MS);

        /* 收集本轮反馈 */
        MotorFeedback feedback[USB_PKG_MOTOR_CNT];
        FDCAN_GetAllMotorFeedback(feedback);
        SPI_GetSlaveFeedback(&feedback[3]);

        Motor_Calib_FeedSample(feedback);
    }
}

/* ==================== TIM6中断回调：ID1/ID2立即连续发送，ID3延迟0.5ms后发送 ==================== */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM6) return;
    if (!g_can_tx_pending) return;

    if (g_can_tx_index == 0) {
        /* 第一次触发：立即发送ID1和ID2，重置延迟计数，等待0.5ms后发ID3 */
        CAN_SendFrame(0);
        CAN_SendFrame(1);
        g_can_tx_delay_cnt = 0;
        g_can_tx_index = 2;
    } else if (g_can_tx_index == 2) {
        /* 累计0.1ms×5=0.5ms后发送ID3 */
        g_can_tx_delay_cnt++;
        if (g_can_tx_delay_cnt < 5) return;

        CAN_SendFrame(2);
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

    /* 电机掉线检测 */
    System_CheckMotorOffline();

    /* 通过USB发送12个电机的完整反馈数据 */
    Protocol_SendFeedback(g_motor_feedback, g_sequence);
}

/* ==================== 电机掉线检测 ==================== */
static void System_CheckMotorOffline(void)
{
    uint8_t any_offline = 0;

    for (int i = 0; i < USB_PKG_MOTOR_CNT; i++) {
        uint32_t ts = g_motor_feedback[i].timestamp;

        if (ts == g_last_feedback_ts[i] || ts == 0) {
            /* 时间戳未更新：本轮未收到该电机反馈 */
            if (g_motor_offline_cnt[i] < MOTOR_OFFLINE_THRESHOLD) {
                g_motor_offline_cnt[i]++;
            }
            if (g_motor_offline_cnt[i] >= MOTOR_OFFLINE_THRESHOLD) {
                g_motor_offline[i] = 1;
            }
        } else {
            /* 时间戳有更新：通信正常，清除计数 */
            g_motor_offline_cnt[i] = 0;
            g_motor_offline[i] = 0;
        }

        g_last_feedback_ts[i] = ts;

        if (g_motor_offline[i]) {
            any_offline = 1;
        }
    }

    if (any_offline) {
        LED_Indicator_BeepAlarm(0);   /* 持续鸣叫，直到所有电机恢复 */
    } else {
        LED_Indicator_BeepStop();
    }
}

/* ==================== 系统状态管理 ==================== */
SystemState System_GetState(void)
{
    return g_system_state;
}

uint8_t System_IsMotorOffline(uint8_t motor_idx)
{
    if (motor_idx >= USB_PKG_MOTOR_CNT) return 0;
    return g_motor_offline[motor_idx];
}
