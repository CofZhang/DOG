/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    slave_controller.c
  * @brief   从控制器实现（STM32G474RET6）
  *          负责接收主控通过SPI发送的电机控制指令，
  *          通过FDCAN1/2/3控制9个电机，并反馈电机状态
  ******************************************************************************
  */
/* USER CODE END Header */

#include "slave_controller.h"
#include <string.h>

// 全局从控制器状态
static SlaveController_t g_slaveCtrl = {0};

// 电机ID映射表（电机4-12）
static const uint8_t MOTOR_ID_MAP[9] = {
    MOTOR_ID_4,  MOTOR_ID_5,  MOTOR_ID_6,     // FDCAN1
    MOTOR_ID_7,  MOTOR_ID_8,  MOTOR_ID_9,     // FDCAN2
    MOTOR_ID_10, MOTOR_ID_11, MOTOR_ID_12     // FDCAN3
};

// FDCAN句柄映射表
static FDCAN_HandleTypeDef* FDCAN_HANDLE_MAP[9] = {
    &hfdcan1, &hfdcan1, &hfdcan1,   // 电机4-6 -> FDCAN1
    &hfdcan2, &hfdcan2, &hfdcan2,   // 电机7-9 -> FDCAN2
    &hfdcan3, &hfdcan3, &hfdcan3    // 电机10-12 -> FDCAN3
};

/**
 * @brief 配置FDCAN过滤器（接收所有标准帧）
 * @param hfdcan FDCAN句柄
 */
static void ConfigureFDCANFilter(FDCAN_HandleTypeDef *hfdcan) {
    FDCAN_FilterTypeDef sFilterConfig;

    // 配置过滤器：接收所有标准帧
    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1 = 0x000;
    sFilterConfig.FilterID2 = 0x7FF;

    if (HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig) != HAL_OK) {
        Error_Handler();
    }

    // 配置全局过滤器：拒绝不匹配的帧
    if (HAL_FDCAN_ConfigGlobalFilter(hfdcan,
                                     FDCAN_REJECT,
                                     FDCAN_REJECT,
                                     FDCAN_FILTER_REMOTE,
                                     FDCAN_FILTER_REMOTE) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief 初始化从控制器
 */
void SlaveController_Init(void) {
    // 清空状态结构体
    memset(&g_slaveCtrl, 0, sizeof(SlaveController_t));

    // ==================== FDCAN初始化 ====================
    // 配置FDCAN1过滤器
    ConfigureFDCANFilter(&hfdcan1);

    // 配置FDCAN2过滤器
    ConfigureFDCANFilter(&hfdcan2);

    // 配置FDCAN3过滤器
    ConfigureFDCANFilter(&hfdcan3);

    // 启动FDCAN1
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }

    // 启动FDCAN2
    if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK) {
        Error_Handler();
    }

    // 启动FDCAN3
    if (HAL_FDCAN_Start(&hfdcan3) != HAL_OK) {
        Error_Handler();
    }

    // 使能FDCAN1接收中断
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Error_Handler();
    }

    // 使能FDCAN2接收中断
    if (HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Error_Handler();
    }

    // 使能FDCAN3接收中断
    if (HAL_FDCAN_ActivateNotification(&hfdcan3, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Error_Handler();
    }

    // ==================== SPI初始化 ====================
    // 启动SPI DMA接收（等待主控发送数据）
    if (HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket, sizeof(SPIPacket_t)) != HAL_OK) {
        Error_Handler();
    }

    // 标记初始化完成
    g_slaveCtrl.initialized = 1;
    g_slaveCtrl.lastRxTime = HAL_GetTick();
}

/**
 * @brief 发送电机控制指令到FDCAN
 * @param motorIndex 电机索引（0-8对应电机4-12）
 * @param canData 8字节CAN数据
 */
void SlaveController_SendMotorCommand(uint8_t motorIndex, const uint8_t* canData) {
    if (motorIndex >= SLAVE_MOTOR_COUNT) {
        return;
    }

    FDCAN_TxHeaderTypeDef txHeader;

    // 配置发送头
    txHeader.Identifier = MOTOR_ID_MAP[motorIndex];
    txHeader.IdType = FDCAN_STANDARD_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.DataLength = FDCAN_DLC_BYTES_8;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch = FDCAN_BRS_OFF;
    txHeader.FDFormat = FDCAN_CLASSIC_CAN;
    txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker = 0;

    // 发送数据
    FDCAN_HandleTypeDef* hfdcan = FDCAN_HANDLE_MAP[motorIndex];
    HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &txHeader, (uint8_t*)canData);
}

/**
 * @brief 处理接收到的SPI数据包
 */
void SlaveController_ProcessSPIPacket(void) {
    SPIPacket_t* packet = &g_slaveCtrl.spiRxPacket;

    // 验证帧头和帧尾
    if (packet->headerH != SPI_HEADER_H || packet->headerL != SPI_HEADER_L ||
        packet->footerH != SPI_FOOTER_H || packet->footerL != SPI_FOOTER_L) {
        return;  // 数据包格式错误
    }

    // 验证校验和
    uint16_t calculatedChecksum = CalculateChecksum16((uint8_t*)packet, sizeof(SPIPacket_t) - 4);
    uint16_t receivedChecksum = ((uint16_t)packet->checksumH << 8) | packet->checksumL;

    if (calculatedChecksum != receivedChecksum) {
        return;  // 校验和错误
    }

    // 更新序列号
    g_slaveCtrl.sequence = packet->sequence;

    // 发送9个电机控制指令到FDCAN
    for (uint8_t i = 0; i < 9; i++) {
        SlaveController_SendMotorCommand(i, packet->motorData[i]);
    }

    // 更新最后接收时间
    g_slaveCtrl.lastRxTime = HAL_GetTick();
}

/**
 * @brief 准备SPI反馈数据包
 */
void SlaveController_PrepareFeedbackPacket(void) {
    SPIFeedbackPacket_t* feedback = &g_slaveCtrl.spiFeedbackPacket;

    // 设置帧头和帧尾
    feedback->headerH = SPI_HEADER_H;
    feedback->headerL = SPI_HEADER_L;
    feedback->footerH = SPI_FOOTER_H;
    feedback->footerL = SPI_FOOTER_L;

    // 设置状态和序列号
    feedback->status = 0x00;  // 正常状态
    feedback->sequence = g_slaveCtrl.sequence;

    // 复制电机反馈数据（这里使用最新接收到的CAN反馈）
    // 注意：实际反馈数据在FDCAN接收中断中更新
    memcpy(feedback->motorFeedback, g_slaveCtrl.motorFeedbackRaw, sizeof(feedback->motorFeedback));

    // 清空保留字节
    memset(feedback->reservedBytes, 0, sizeof(feedback->reservedBytes));

    // 计算校验和
    uint16_t checksum = CalculateChecksum16((uint8_t*)feedback, sizeof(SPIFeedbackPacket_t) - 4);
    feedback->checksumH = (checksum >> 8) & 0xFF;
    feedback->checksumL = checksum & 0xFF;
}

/**
 * @brief 主循环处理
 */
void SlaveController_Process(void) {
    // 检查SPI接收完成
    if (g_slaveCtrl.spiRxComplete) {
        g_slaveCtrl.spiRxComplete = 0;

        // 处理接收到的SPI数据包
        SlaveController_ProcessSPIPacket();

        // 准备反馈数据包
        SlaveController_PrepareFeedbackPacket();

        // 发送反馈数据包
        if (HAL_SPI_Transmit_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiFeedbackPacket,
                                 sizeof(SPIFeedbackPacket_t)) != HAL_OK) {
            Error_Handler();
        }
    }

    // 检查SPI发送完成
    if (g_slaveCtrl.spiTxComplete) {
        g_slaveCtrl.spiTxComplete = 0;

        // 重新启动SPI接收
        if (HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket,
                                sizeof(SPIPacket_t)) != HAL_OK) {
            Error_Handler();
        }
    }

    // 超时保护：如果超过1秒没有收到数据，停止所有电机
    if ((HAL_GetTick() - g_slaveCtrl.lastRxTime) > 1000) {
        // 发送零扭矩指令停止所有电机
        uint8_t stopCmd[8] = {0};
        MotorCommand_t zeroCmd = {0};
        MotorCommand_Pack(&zeroCmd, stopCmd);

        for (uint8_t i = 0; i < 9; i++) {
            SlaveController_SendMotorCommand(i, stopCmd);
        }

        g_slaveCtrl.lastRxTime = HAL_GetTick();  // 更新时间避免重复发送
    }
}

/**
 * @brief SPI接收完成回调
 */
void SlaveController_SPI_RxCpltCallback(void) {
    g_slaveCtrl.spiRxComplete = 1;
}

/**
 * @brief SPI发送完成回调
 */
void SlaveController_SPI_TxCpltCallback(void) {
    g_slaveCtrl.spiTxComplete = 1;
}

/**
 * @brief FDCAN接收回调
 * @param hfdcan FDCAN句柄
 * @param RxFifo0ITs 中断标志
 */
void SlaveController_FDCAN_RxCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0) {
        FDCAN_RxHeaderTypeDef rxHeader;
        uint8_t rxData[8];

        // 从FIFO0读取消息
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
            // 判断是哪个电机的反馈
            uint8_t motorId = rxHeader.Identifier;

            // 查找电机索引
            for (uint8_t i = 0; i < 9; i++) {
                if (MOTOR_ID_MAP[i] == motorId) {
                    // 保存原始反馈数据（8字节）
                    memcpy(g_slaveCtrl.motorFeedbackRaw[i], rxData, 8);
                    break;
                }
            }
        }
    }
}

// ==================== HAL回调函数重定向 ====================
/**
 * @brief SPI接收完成回调（HAL库调用）
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        SlaveController_SPI_RxCpltCallback();
    }
}

/**
 * @brief SPI发送完成回调（HAL库调用）
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        SlaveController_SPI_TxCpltCallback();
    }
}

/**
 * @brief FDCAN接收FIFO0回调（HAL库调用）
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
    SlaveController_FDCAN_RxCallback(hfdcan, RxFifo0ITs);
}
