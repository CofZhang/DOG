/**
 * @file system_control.h
 * @brief 系统控制模块 - 整合USB接收、SPI发送、FDCAN控制
 * @note 主控制流程：USB接收 → 解析 → FDCAN1直控(电机1-3) + SPI转发(电机4-12)
 */

#ifndef __SYSTEM_CONTROL_H
#define __SYSTEM_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "motor.h"
#include "usbcol.h"
#include "u_spi.h"

/* ==================== 系统状态定义 ==================== */
typedef enum {
    SYS_STATE_INIT = 0,       // 初始化
    SYS_STATE_IDLE,           // 空闲
    SYS_STATE_RUNNING,        // 运行中
    SYS_STATE_ERROR           // 错误
} SystemState;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化系统控制模块
 */
void System_Control_Init(void);

/**
 * @brief 系统主循环处理函数
 * @note 在main.c的while(1)循环中调用
 */
void System_Control_Process(void);

/**
 * @brief 处理USB接收到的电机控制数据
 * @note 解析USB数据包，分发到FDCAN1和SPI
 */
void System_ProcessMotorControl(void);

/**
 * @brief 发送电机反馈数据到USB
 * @note 收集所有电机反馈并打包发送
 */
void System_SendFeedback(void);

/**
 * @brief 获取系统状态
 * @return 当前系统状态
 */
SystemState System_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_CONTROL_H */
