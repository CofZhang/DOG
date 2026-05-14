/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    slave_controller.h
  * @brief   从控制器头文件（STM32G474RET6）
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SLAVE_CONTROLLER_H__
#define __SLAVE_CONTROLLER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "motor_protocol.h"
#include "fdcan.h"
#include "spi.h"

// ==================== 从控制器配置 ====================
#define SLAVE_MOTOR_COUNT       9       // 从控制器管理9个电机

// 电机到FDCAN映射
typedef enum {
    MOTOR_4_FDCAN1 = 0,     // 电机4 -> FDCAN1
    MOTOR_5_FDCAN1 = 1,     // 电机5 -> FDCAN1
    MOTOR_6_FDCAN1 = 2,     // 电机6 -> FDCAN1
    MOTOR_7_FDCAN2 = 3,     // 电机7 -> FDCAN2
    MOTOR_8_FDCAN2 = 4,     // 电机8 -> FDCAN2
    MOTOR_9_FDCAN2 = 5,     // 电机9 -> FDCAN2
    MOTOR_10_FDCAN3 = 6,    // 电机10 -> FDCAN3
    MOTOR_11_FDCAN3 = 7,    // 电机11 -> FDCAN3
    MOTOR_12_FDCAN3 = 8     // 电机12 -> FDCAN3
} SlaveMotorIndex_e;

// 从控制器状态
typedef struct {
    uint8_t initialized;                        // 初始化标志
    volatile uint8_t spiRxComplete;                      // SPI接收完成标志
    volatile uint8_t spiTxComplete;                      // SPI发送完成标志
    SPIPacket_t spiRxPacket;                    // SPI接收缓冲区
    SPIFeedbackPacket_t spiFeedbackPacket;      // SPI反馈缓冲区
    uint8_t motorFeedbackRaw[9][8];             // 9个电机原始反馈数据（8字节）
    uint32_t lastRxTime;                        // 最后接收时间
    uint8_t sequence;                           // 序列号
} SlaveController_t;

// ==================== 函数声明 ====================
// 初始化从控制器
void SlaveController_Init(void);

// 主循环处理
void SlaveController_Process(void);

// SPI接收完成回调
void SlaveController_SPI_RxCpltCallback(void);

// SPI发送完成回调
void SlaveController_SPI_TxCpltCallback(void);

// FDCAN接收回调
void SlaveController_FDCAN_RxCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);

// 发送电机控制指令到FDCAN
void SlaveController_SendMotorCommand(uint8_t motorIndex, const uint8_t* canData);

// 处理接收到的SPI数据包
void SlaveController_ProcessSPIPacket(void);

// 准备SPI反馈数据包
void SlaveController_PrepareFeedbackPacket(void);

#ifdef __cplusplus
}
#endif

#endif /* __SLAVE_CONTROLLER_H__ */
