/**
 * @file led_indicator.h
 * @brief LED和蜂鸣器指示系统
 * @note 提供系统状态的可视化和声音指示
 *
 * LED指示定义：
 * - LED1: 系统运行指示（心跳，1Hz闪烁）
 * - LED2: USB通信指示（接收数据时闪烁）
 * - LED3: SPI通信指示（发送数据时闪烁）
 * - LED4: FDCAN1通信指示（发送数据时闪烁）
 * - LED5: 电机反馈正常指示（收到反馈时闪烁）
 * - LED6: USB错误指示（常亮表示错误）
 * - LED7: SPI错误指示（常亮表示错误）
 * - LED8: FDCAN错误指示（常亮表示错误）
 * - BEEP: 严重错误报警（连续鸣叫）
 */

#ifndef __LED_INDICATOR_H
#define __LED_INDICATOR_H

#include "main.h"
#include "led.h"

/* ==================== LED指示类型定义 ==================== */
typedef enum {
    LED_IND_SYSTEM_HEARTBEAT = 0,   // 系统心跳
    LED_IND_USB_RX,                 // USB接收
    LED_IND_USB_TX,                 // USB发送
    LED_IND_SPI_TX,                 // SPI发送
    LED_IND_FDCAN_TX,               // FDCAN发送
    LED_IND_MOTOR_FEEDBACK,         // 电机反馈
    LED_IND_USB_ERROR,              // USB错误
    LED_IND_SPI_ERROR,              // SPI错误
    LED_IND_FDCAN_ERROR,            // FDCAN错误
    LED_IND_CRITICAL_ERROR          // 严重错误（蜂鸣器）
} LED_IndicatorType;

/* ==================== 错误级别定义 ==================== */
typedef enum {
    ERROR_LEVEL_NONE = 0,           // 无错误
    ERROR_LEVEL_WARNING,            // 警告
    ERROR_LEVEL_ERROR,              // 错误
    ERROR_LEVEL_CRITICAL            // 严重错误
} ErrorLevel;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化LED指示系统
 */
void LED_Indicator_Init(void);

/**
 * @brief LED指示系统主循环处理
 * @note 在main循环中调用，处理LED闪烁和蜂鸣器
 */
void LED_Indicator_Process(void);

/**
 * @brief 触发LED指示
 * @param type 指示类型
 */
void LED_Indicator_Trigger(LED_IndicatorType type);

/**
 * @brief 设置错误状态
 * @param type 指示类型
 * @param level 错误级别
 */
void LED_Indicator_SetError(LED_IndicatorType type, ErrorLevel level);

/**
 * @brief 清除错误状态
 * @param type 指示类型
 */
void LED_Indicator_ClearError(LED_IndicatorType type);

/**
 * @brief 获取错误计数
 * @param type 指示类型
 * @return 错误计数
 */
uint32_t LED_Indicator_GetErrorCount(LED_IndicatorType type);

/**
 * @brief 启动蜂鸣器报警
 * @param duration_ms 持续时间（毫秒），0表示持续鸣叫
 */
void LED_Indicator_BeepAlarm(uint32_t duration_ms);

/**
 * @brief 停止蜂鸣器报警
 */
void LED_Indicator_BeepStop(void);


void LED_Indicator_TestMode(void);

#endif
