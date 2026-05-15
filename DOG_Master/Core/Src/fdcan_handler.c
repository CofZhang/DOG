/**
 * @file fdcan_handler.c
 * @brief FDCAN接收处理模块实现
 */

#include "fdcan_handler.h"
#include "led_indicator.h"
#include <string.h>

/* ==================== 私有变量 ==================== */
/* 电机反馈数据缓存（12个电机） */
static MotorFeedback g_motor_feedback[12];

/* 反馈数据更新标志 */
static volatile uint8_t g_feedback_updated[12] = {0};

/* 外部FDCAN句柄 */
extern FDCAN_HandleTypeDef hfdcan1;

/* ==================== FDCAN初始化 ==================== */
void FDCAN_Handler_Init(void)
{
    /* 清空反馈数据 */
    memset(g_motor_feedback, 0, sizeof(g_motor_feedback));
    memset((void*)g_feedback_updated, 0, sizeof(g_feedback_updated));

    /* 配置FDCAN1过滤器 - 接收所有标准ID */
    FDCAN_FilterTypeDef filter_config;
    filter_config.IdType = FDCAN_STANDARD_ID;
    filter_config.FilterIndex = 0;
    filter_config.FilterType = FDCAN_FILTER_RANGE;
    filter_config.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter_config.FilterID1 = 0x000;
    filter_config.FilterID2 = 0x7FF;

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter_config) != HAL_OK) {
        Error_Handler();
    }

    /* 启动FDCAN */ 
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }

    /* 激活FIFO0新消息通知 */
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Error_Handler();
    }
}

/* ==================== FDCAN启动接收 ==================== */
HAL_StatusTypeDef FDCAN_StartReceive(FDCAN_HandleTypeDef *hfdcan)
{
    return HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
}

/* ==================== 获取电机反馈数据 ==================== */
uint8_t FDCAN_GetMotorFeedback(uint8_t motor_id, MotorFeedback *feedback)
{
    if (motor_id < 1 || motor_id > 12 || feedback == NULL) {
        return 0;
    }

    uint8_t index = motor_id - 1;

    /* 复制反馈数据 */
    memcpy(feedback, &g_motor_feedback[index], sizeof(MotorFeedback));

    return g_feedback_updated[index];
}

void FDCAN_GetAllMotorFeedback(MotorFeedback feedback_array[12])
{
    if (feedback_array == NULL) {
        return;
    }

    /* 复制所有反馈数据 */
    memcpy(feedback_array, g_motor_feedback, sizeof(g_motor_feedback));
}

/* ==================== FDCAN接收回调 ==================== */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0) {
        FDCAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];

        /* 从FIFO0读取消息 */
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
            /* 检查是否为标准ID */
            if (rx_header.IdType == FDCAN_STANDARD_ID) {
                /* 解析电机ID */
                uint32_t motor_id = rx_header.Identifier;

                if (motor_id >= 0x01 && motor_id <= 0x0C) {
                    uint8_t index = motor_id - 1;

                    /* 解析反馈数据 */
                    Motor_UnpackFeedbackData(rx_data, &g_motor_feedback[index]);

                    /* 更新时间戳 */
                    g_motor_feedback[index].timestamp = HAL_GetTick();

                    /* 标记数据已更新 */
                    g_feedback_updated[index] = 1;

                    /* LED指示：收到电机反馈 */
                    LED_Indicator_Trigger(LED_IND_MOTOR_FEEDBACK);

                    /* 检查电机错误码 */
                    if (g_motor_feedback[index].error_code != 0) {
                        /* 电机报错，设置错误指示 */
                        LED_Indicator_SetError(LED_IND_FDCAN_ERROR, ERROR_LEVEL_WARNING);
                    }
                }
            }
        }
    }
}

/* ==================== 全局指令函数 ==================== */
/**
 * @brief 发送全局指令
 * @param data 指令数据
 * @param len 数据长度
 * @return HAL状态
 */
static HAL_StatusTypeDef FDCAN_SendGlobalCommand(const uint8_t *data, uint8_t len)
{
    FDCAN_TxHeaderTypeDef tx_header;

    /* 配置全局指令消息头 */
    tx_header.Identifier = CAN_ID_GLOBAL;
    tx_header.IdType = FDCAN_STANDARD_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.DataLength = FDCAN_DLC_BYTES_8;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0;

    /* 准备8字节数据（不足补0） */
    uint8_t tx_data[8] = {0};
    memcpy(tx_data, data, (len > 8) ? 8 : len);

    /* 发送消息 */
    HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, tx_data);

    /* 延迟500ms（全局指令要求） */
    if (status == HAL_OK) {
        HAL_Delay(500);
    }

    return status;
}

HAL_StatusTypeDef FDCAN_SetZeroPosition(uint8_t motor_id)
{
    uint8_t cmd_data[4] = {0x00, motor_id, 0x00, 0x03};
    return FDCAN_SendGlobalCommand(cmd_data, 4);
}

HAL_StatusTypeDef FDCAN_QueryMotorID(void)
{
    uint8_t cmd_data[4] = {0xFF, 0xFF, 0x00, 0x82};
    return FDCAN_SendGlobalCommand(cmd_data, 4);
}

HAL_StatusTypeDef FDCAN_ResetMotorID(void)
{
    uint8_t cmd_data[6] = {0x7F, 0x7F, 0x00, 0x05, 0x7F, 0x7F};
    return FDCAN_SendGlobalCommand(cmd_data, 6);
}

HAL_StatusTypeDef FDCAN_ConfigTimeout(uint16_t timeout_ms)
{
    uint8_t cmd_data[4] = {
        0xC0,
        0x0B,
        (uint8_t)((timeout_ms >> 8) & 0xFF),
        (uint8_t)(timeout_ms & 0xFF)
    };
    return FDCAN_SendGlobalCommand(cmd_data, 4);
}
