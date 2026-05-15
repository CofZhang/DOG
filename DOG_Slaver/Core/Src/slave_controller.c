/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    slave_controller.c
  * @brief   从控制器实现（STM32G474RET6）- 修复版
  *          负责接收主控通过SPI发送的电机控制指令，
  *          通过FDCAN1/2/3控制9个电机，并实现电机状态反馈
  *
  * 重要修复：
  * 1. SPI配置为16位模式，需要使用16位数据传输
  * 2. 数据包大小必须是偶数字节（16位对齐）
  * 3. DMA传输长度单位为半字（16位）
  ******************************************************************************
  */
/* USER CODE END Header */

#include "slave_controller.h"
#include <string.h>

// 全局从控制器状态
static SlaveController_t g_slaveCtrl = {0};
volatile uint32_t err =100;

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
    // 注意：FDCAN已在main.c中通过MX_FDCANx_Init()完成初始化
    // 此处只需配置过滤器、启动FDCAN并使能接收中断

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
    // STM32G4 HAL Slave 模式：HAL_SPI_Receive_DMA 内部会同时启动 TX DMA（发送 dummy 数据）
    // 和 RX DMA（接收主控数据）。我们在调用前先把反馈包填入 TX 缓冲区，
    // 然后手动将 TX DMA 的源地址指向反馈包，这样 MISO 上发出的就是真实反馈数据。
    uint16_t spi_length = sizeof(SPIPacket_t);  // 80 字节（8bit模式）

    // 预先准备初始反馈包
    SlaveController_PrepareFeedbackPacket();

    // 启动 RX DMA，清除OVR等错误标志确保HAL状态为READY
    __HAL_SPI_CLEAR_OVRFLAG(&hspi1);
    hspi1.ErrorCode = HAL_SPI_ERROR_NONE;
    if (HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket, spi_length) != HAL_OK) {
        Error_Handler();
    }

    // 然后重定向 TX DMA 源地址到反馈包（覆盖 HAL 设置的 dummy 地址）
    // 这样 MISO 线上发出的是真实反馈数据而不是 0xFF
    hspi1.hdmatx->Instance->CMAR = (uint32_t)&g_slaveCtrl.spiFeedbackPacket;
    // 确保 TX DMA 内存地址自增（HAL_SPI_Receive_DMA 会关闭 MINC，需要重新打开）
    hspi1.hdmatx->Instance->CCR |= DMA_CCR_MINC;

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

    // 配置发送头（CAN ID = MotorID）
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
    HAL_StatusTypeDef ret = HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &txHeader, (uint8_t*)canData);
  if (ret != HAL_OK) {
      // 发送失败，记录错误
      err = HAL_FDCAN_GetError(hfdcan);
		uint32_t err2 = HAL_FDCAN_GetError(hfdcan);
		  
      
  }
}

/**
 * @brief 处理接收到的SPI数据包
 */
void SlaveController_ProcessSPIPacket(void) {
    SPIPacket_t* packet = &g_slaveCtrl.spiRxPacket;

    // 验证帧头和帧尾
    if (packet->header != SPI_HEADER || packet->footer != SPI_FOOTER) {
        return;  // 数据包格式错误
    }

    // 验证校验和（对 Byte 0~77 共78字节异或，与 Byte 78 比较）
    uint8_t calculatedChecksum = CalculateChecksum((uint8_t*)packet, sizeof(SPIPacket_t) - 2);

    if (calculatedChecksum != packet->checksum) {
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
    feedback->header = SPI_HEADER;
    feedback->footer = SPI_FOOTER;

    // 设置状态和序列号
    feedback->status = 0x00;  // 正常状态
    feedback->motorCnt = 9;
    feedback->sequence = g_slaveCtrl.sequence;
    feedback->reserved[0] = 0x00;
    feedback->reserved[1] = 0x00;

    // 复制电机反馈数据（在FDCAN接收中断中更新）
    memcpy(feedback->motorFeedback, g_slaveCtrl.motorFeedbackRaw, sizeof(feedback->motorFeedback));

    // 计算校验和（对 Byte 0~77 共78字节异或）
    feedback->checksum = CalculateChecksum((uint8_t*)feedback, sizeof(SPIFeedbackPacket_t) - 2);
}

/**
 * @brief 主循环处理
 */
void SlaveController_Process(void) {
    // SPI错误恢复：HAL因OVR等错误锁死时，主动清除并重启DMA
    if (hspi1.ErrorCode != HAL_SPI_ERROR_NONE ||
        hspi1.State == HAL_SPI_STATE_ERROR) {
        __HAL_SPI_CLEAR_OVRFLAG(&hspi1);
        hspi1.ErrorCode = HAL_SPI_ERROR_NONE;
        hspi1.State = HAL_SPI_STATE_READY;
        g_slaveCtrl.spiRxComplete = 0;
        uint16_t spi_length = sizeof(SPIPacket_t);
        HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket, spi_length);
        hspi1.hdmatx->Instance->CMAR = (uint32_t)&g_slaveCtrl.spiFeedbackPacket;
        hspi1.hdmatx->Instance->CCR |= DMA_CCR_MINC;
    }

    // 检查SPI接收完成
    if (g_slaveCtrl.spiRxComplete) {
        g_slaveCtrl.spiRxComplete = 0;
        g_slaveCtrl.spiTxComplete = 0;

        // 处理接收到的SPI数据包（此时主控的控制指令已经接收完毕）
        SlaveController_ProcessSPIPacket();

        // 基于最新的电机反馈准备下一轮要回传的反馈包
        SlaveController_PrepareFeedbackPacket();

        // 重新启动 RX DMA，清除OVR错误确保HAL状态为READY
        __HAL_SPI_CLEAR_OVRFLAG(&hspi1);
        hspi1.ErrorCode = HAL_SPI_ERROR_NONE;
        hspi1.State = HAL_SPI_STATE_READY;
        uint16_t spi_length = sizeof(SPIPacket_t);  // 80 字节（8bit模式）
        if (HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket, spi_length) != HAL_OK) {
            Error_Handler();
        }
        // 重定向 TX DMA 源地址到反馈包，并开启内存地址自增
        hspi1.hdmatx->Instance->CMAR = (uint32_t)&g_slaveCtrl.spiFeedbackPacket;
        hspi1.hdmatx->Instance->CCR |= DMA_CCR_MINC;
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
 * @brief SPI 接收完成回调（HAL_SPI_Receive_DMA 完成时触发）
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        SlaveController_SPI_RxCpltCallback();
    }
}

/**
 * @brief FDCAN接收FIFO0回调（HAL库调用）
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
    SlaveController_FDCAN_RxCallback(hfdcan, RxFifo0ITs);
}
