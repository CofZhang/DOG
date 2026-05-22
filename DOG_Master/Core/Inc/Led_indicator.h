/**
 * @file led_indicator.h
 * @brief LED和蜂鸣器指示系统
 * @note 提供系统状态的可视化和声音指示
 *
 * LED指示定义：
 * - LED1: 系统心跳（1Hz闪烁，系统正常运行）
 * - LED2: USB数据接收指示（每收到一包闪一次）
 * - LED3: CAN发送指示（每发送成功一次闪一次）
 * - LED4: SPI发送指示（每发送成功一次闪一次）
 * - LED5: 电机1-3掉线报错（常亮）
 * - LED6: 电机4-6掉线报错（常亮）
 * - LED7: 电机7-9掉线报错（常亮）
 * - LED8: 电机10-12掉线报错（常亮）
 * - BEEP: 电机掉线报警（0.5s间歇鸣叫）
 */

#ifndef __LED_INDICATOR_H
#define __LED_INDICATOR_H

#include "main.h"
#include "led.h"

/* ==================== LED指示类型定义 ==================== */
typedef enum {
    LED_IND_SYSTEM_HEARTBEAT = 0,   // LED1：系统心跳
    LED_IND_USB_RX,                 // LED2：USB数据接收
    LED_IND_FDCAN_TX,               // LED3：CAN发送
    LED_IND_SPI_TX,                 // LED4：SPI发送
    LED_IND_COUNT                   // 枚举计数（保持在末尾）
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
