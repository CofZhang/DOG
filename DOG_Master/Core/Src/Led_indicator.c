#include "Led_indicator.h"
#include <string.h>

/* ==================== 私有变量 ==================== */
/* LED闪烁计数器 */
static uint32_t g_led_blink_counter[10] = {0};

/* LED闪烁持续时间（毫秒） */
static uint32_t g_led_blink_duration[10] = {0};

/* 错误状态 */
static ErrorLevel g_error_level[10] = {ERROR_LEVEL_NONE};

/* 错误计数 */
static uint32_t g_error_count[10] = {0};

/* 蜂鸣器状态 */
static uint32_t g_beep_start_time = 0;
static uint32_t g_beep_duration = 0;
static uint8_t g_beep_active = 0;

/* 系统心跳计数器 */
static uint32_t g_heartbeat_counter = 0;

/* ==================== LED指示初始化 ==================== */
void LED_Indicator_Init(void)
{
    /* 清空所有状态 */ 
    memset(g_led_blink_counter, 0, sizeof(g_led_blink_counter));
    memset(g_led_blink_duration, 0, sizeof(g_led_blink_duration));
    memset(g_error_level, 0, sizeof(g_error_level));
    memset(g_error_count, 0, sizeof(g_error_count));

    /* 关闭所有LED和蜂鸣器 */
    LED1_Off();
    LED2_Off();
    LED3_Off();
    LED5_Off();
    LED6_Off();
    LED7_Off();
    LED8_Off();
    LED4_Off();
    BEEP_Off();

    g_beep_active = 0;
    g_heartbeat_counter = 0;
}

/* ==================== LED指示主循环处理 ==================== */
void LED_Indicator_Process(void)
{
    uint32_t current_time = HAL_GetTick();

    /* ==================== 系统心跳LED（LED1，1Hz闪烁） ==================== */
    static uint32_t last_heartbeat_time = 0;
    if (current_time - last_heartbeat_time >= 500) {  // 500ms切换一次
        last_heartbeat_time = current_time;
        static uint8_t heartbeat_state = 0;
        heartbeat_state = !heartbeat_state;
        if (heartbeat_state) {
            LED1_On();
        } else {
            LED1_Off();
        }
    }

    /* ==================== USB接收指示（LED2） ==================== */
    if (g_led_blink_duration[LED_IND_USB_RX] > 0) {
        if (current_time - g_led_blink_counter[LED_IND_USB_RX] < g_led_blink_duration[LED_IND_USB_RX]) {
            LED2_On();
        } else {
            LED2_Off();
            g_led_blink_duration[LED_IND_USB_RX] = 0;
        }
    }

    /* ==================== SPI发送指示（LED3） ==================== */
    if (g_led_blink_duration[LED_IND_SPI_TX] > 0) {
        if (current_time - g_led_blink_counter[LED_IND_SPI_TX] < g_led_blink_duration[LED_IND_SPI_TX]) {
            LED3_On();
        } else {
            LED3_Off();
            g_led_blink_duration[LED_IND_SPI_TX] = 0;
        }
    }

    /* ==================== FDCAN发送指示（LED5） ==================== */
    if (g_led_blink_duration[LED_IND_FDCAN_TX] > 0) {
        if (current_time - g_led_blink_counter[LED_IND_FDCAN_TX] < g_led_blink_duration[LED_IND_FDCAN_TX]) {
            LED4_On();
        } else {
            LED4_Off();
            g_led_blink_duration[LED_IND_FDCAN_TX] = 0;
        }
    }

    /* ==================== 电机反馈指示（LED6） ==================== */
    if (g_led_blink_duration[LED_IND_MOTOR_FEEDBACK] > 0) {
        if (current_time - g_led_blink_counter[LED_IND_MOTOR_FEEDBACK] < g_led_blink_duration[LED_IND_MOTOR_FEEDBACK]) {
            LED5_On();
        } else {
            LED5_Off();
            g_led_blink_duration[LED_IND_MOTOR_FEEDBACK] = 0;
        }
    }

    /* ==================== USB错误指示（LED7） ==================== */
    if (g_error_level[LED_IND_USB_ERROR] >= ERROR_LEVEL_ERROR) {
        LED6_On();
    } else {
        LED6_Off();
    }

    /* ==================== SPI错误指示（LED8） ==================== */
    if (g_error_level[LED_IND_SPI_ERROR] >= ERROR_LEVEL_ERROR) {
        LED7_On();
    } else {
        LED7_Off();
    }

    /* ==================== FDCAN错误指示（LED9） ==================== */
    if (g_error_level[LED_IND_FDCAN_ERROR] >= ERROR_LEVEL_ERROR) {
        LED8_On();
    } else {
        LED8_Off();
    }

    /* ==================== 蜂鸣器处理 ==================== */
    if (g_beep_active) {
        if (g_beep_duration > 0) {
            // 有限时长鸣叫
            if (current_time - g_beep_start_time >= g_beep_duration) {
                BEEP_Off();
                g_beep_active = 0;
            } else {
                // 间歇鸣叫（200ms开，200ms关）
                uint32_t elapsed = current_time - g_beep_start_time;
                if ((elapsed % 400) < 200) {
                    BEEP_On();
                } else {
                    BEEP_Off();
                }
            }
        } else {
            // 持续鸣叫（严重错误）
            BEEP_On();
        }
    }

    /* ==================== 严重错误检测 ==================== */
    if (g_error_level[LED_IND_CRITICAL_ERROR] >= ERROR_LEVEL_CRITICAL) {
        if (!g_beep_active) {
            LED_Indicator_BeepAlarm(0);  // 持续鸣叫
        }
    }
}

/* ==================== 触发LED指示 ==================== */
void LED_Indicator_Trigger(LED_IndicatorType type)
{
    uint32_t current_time = HAL_GetTick();

    switch (type) {
        case LED_IND_USB_RX:
            g_led_blink_counter[LED_IND_USB_RX] = current_time;
            g_led_blink_duration[LED_IND_USB_RX] = 50;  // 闪烁50ms
            break;

        case LED_IND_USB_TX:
            g_led_blink_counter[LED_IND_USB_TX] = current_time;
            g_led_blink_duration[LED_IND_USB_TX] = 50;  // 闪烁50ms
            break;

        case LED_IND_SPI_TX:
            g_led_blink_counter[LED_IND_SPI_TX] = current_time;
            g_led_blink_duration[LED_IND_SPI_TX] = 30;  // 闪烁30ms
            break;

        case LED_IND_FDCAN_TX:
            g_led_blink_counter[LED_IND_FDCAN_TX] = current_time;
            g_led_blink_duration[LED_IND_FDCAN_TX] = 30;  // 闪烁30ms
            break;

        case LED_IND_MOTOR_FEEDBACK:
            g_led_blink_counter[LED_IND_MOTOR_FEEDBACK] = current_time;
            g_led_blink_duration[LED_IND_MOTOR_FEEDBACK] = 50;  // 闪烁50ms
            break;

        default:
            break;
    }
}

/* ==================== 设置错误状态 ==================== */
void LED_Indicator_SetError(LED_IndicatorType type, ErrorLevel level)
{
    g_error_level[type] = level;
    g_error_count[type]++;

    /* 如果是严重错误，启动蜂鸣器 */
    if (level >= ERROR_LEVEL_CRITICAL) {
        LED_Indicator_BeepAlarm(0);  // 持续鸣叫
    }
}

/* ==================== 清除错误状态 ==================== */
void LED_Indicator_ClearError(LED_IndicatorType type)
{
    g_error_level[type] = ERROR_LEVEL_NONE;

    /* 检查是否还有其他严重错误 */
    uint8_t has_critical_error = 0;
    for (int i = 0; i < 10; i++) {
        if (g_error_level[i] >= ERROR_LEVEL_CRITICAL) {
            has_critical_error = 1;
            break;
        }
    }

    /* 如果没有严重错误，停止蜂鸣器 */
    if (!has_critical_error) {
        LED_Indicator_BeepStop();
    }
}

/* ==================== 获取错误计数 ==================== */
uint32_t LED_Indicator_GetErrorCount(LED_IndicatorType type)
{
    return g_error_count[type];
}

/* ==================== 启动蜂鸣器报警 ==================== */
void LED_Indicator_BeepAlarm(uint32_t duration_ms)
{
    g_beep_start_time = HAL_GetTick();
    g_beep_duration = duration_ms;
    g_beep_active = 1;
}

/* ==================== 停止蜂鸣器报警 ==================== */
void LED_Indicator_BeepStop(void)
{
    BEEP_Off();
    g_beep_active = 0;
}

void LED_Indicator_TestMode(void)
{

	LED1_On();
	HAL_Delay(200);
	LED1_Off();
	LED2_On();
	HAL_Delay(200);
	LED2_Off();
	LED3_On();
	HAL_Delay(200);
	LED3_Off();
	LED4_On();
	HAL_Delay(200);
	LED4_Off();
	LED5_On();
	HAL_Delay(200);
	LED5_Off();
	LED6_On();
	HAL_Delay(200);
	LED6_Off();
	LED7_On();
	HAL_Delay(200);
	LED7_Off();
	LED8_On();
	HAL_Delay(200);
	LED8_Off();

    Beep_Init();
	BEEP_On();
	HAL_Delay(100);
	BEEP_Off();

	LED1_On();
	LED2_On();
	LED3_On();
	LED5_On();
	LED6_On();
	LED7_On();
	LED8_On();
	LED4_On();
	HAL_Delay(500);

	LED1_Off();
	LED2_Off();
	LED3_Off();
	LED5_Off();
	LED6_Off();
	LED7_Off();
	LED8_Off();
	LED4_Off();
}
