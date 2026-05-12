/**
 * @file fdcan_handler.h
 * @brief FDCAN接收处理模块 - 处理电机反馈数据
 * @note 本文件实现FDCAN接收中断处理和反馈数据收集
 */

#ifndef __FDCAN_HANDLER_H
#define __FDCAN_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "fdcan.h"
#include "motor.h"

/* ==================== FDCAN配置参数 ==================== */
/*
 * 波特率修改位置：在CubeMX中修改FDCAN配置
 * 或在fdcan.c的MX_FDCAN1_Init()函数中修改以下参数：
 *
 * hfdcan1.Init.NominalPrescaler = 4;      // 仲裁域预分频
 * hfdcan1.Init.NominalTimeSeg1 = 13;      // 仲裁域时间段1
 * hfdcan1.Init.NominalTimeSeg2 = 2;       // 仲裁域时间段2
 * hfdcan1.Init.DataPrescaler = 4;         // 数据域预分频
 * hfdcan1.Init.DataTimeSeg1 = 13;         // 数据域时间段1
 * hfdcan1.Init.DataTimeSeg2 = 2;          // 数据域时间段2
 *
 * 当前配置：1Mbps（仲裁域和数据域）
 * 计算公式：波特率 = CAN时钟 / (预分频 × (时间段1 + 时间段2 + 1))
 */

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化FDCAN接收处理模块
 * @note 配置FDCAN过滤器和启动接收
 */
void FDCAN_Handler_Init(void);

/**
 * @brief 启动FDCAN接收
 * @param hfdcan FDCAN句柄
 * @return HAL状态
 */
HAL_StatusTypeDef FDCAN_StartReceive(FDCAN_HandleTypeDef *hfdcan);

/**
 * @brief 获取指定电机的最新反馈数据
 * @param motor_id 电机ID (1-12)
 * @param feedback 输出反馈数据
 * @return 1=成功，0=失败
 */
uint8_t FDCAN_GetMotorFeedback(uint8_t motor_id, MotorFeedback *feedback);

/**
 * @brief 获取所有电机的反馈数据
 * @param feedback_array 输出反馈数据数组（12个电机）
 */
void FDCAN_GetAllMotorFeedback(MotorFeedback feedback_array[12]);

/**
 * @brief FDCAN接收FIFO0回调函数
 * @param hfdcan FDCAN句柄
 * @param RxFifo0ITs 中断标志
 * @note 此函数会被HAL库自动调用
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);

/**
 * @brief 发送全局指令 - 设置当前位置为零点
 * @param motor_id 电机ID (1-12)
 * @return HAL状态
 */
HAL_StatusTypeDef FDCAN_SetZeroPosition(uint8_t motor_id);

/**
 * @brief 发送全局指令 - 查询电机CAN ID
 * @return HAL状态
 */
HAL_StatusTypeDef FDCAN_QueryMotorID(void);

/**
 * @brief 发送全局指令 - 重置电机CAN ID为1
 * @return HAL状态
 */
HAL_StatusTypeDef FDCAN_ResetMotorID(void);

/**
 * @brief 发送全局指令 - 配置CAN超时时间
 * @param timeout_ms 超时时间（毫秒），0=关闭超时保护
 * @return HAL状态
 */
HAL_StatusTypeDef FDCAN_ConfigTimeout(uint16_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __FDCAN_HANDLER_H */
