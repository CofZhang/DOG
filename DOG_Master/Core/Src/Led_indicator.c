#include "Led_indicator.h"
#include <string.h>

/* ==================== 私有变量 ==================== */
/* 各闪烁指示灯的触发时间戳和持续时间 */
static uint32_t g_blink_start[LED_IND_COUNT];
static uint32_t g_blink_duration[LED_IND_COUNT];

/* ==================== LED指示初始化 ==================== */
void LED_Indicator_Init(void)
{
    memset(g_blink_start,    0, sizeof(g_blink_start));
    memset(g_blink_duration, 0, sizeof(g_blink_duration));

    LED1_Off();
    LED2_Off();
    LED3_Off();
    LED4_Off();
    LED5_Off();
    LED6_Off();
    LED7_Off();
    LED8_Off();
    BEEP_Off();
}

/* ==================== LED指示主循环处理 ==================== */
void LED_Indicator_Process(void)
{
    uint32_t now = HAL_GetTick();

    /* LED1：系统心跳，500ms切换一次（1Hz） */
    static uint32_t last_hb = 0;
    static uint8_t  hb_state = 0;
    if (now - last_hb >= 500) {
        last_hb = now;
        hb_state = !hb_state;
        if (hb_state) { LED1_On(); } else { LED1_Off(); }
    }

    /* LED2：USB接收，触发后亮50ms */
    if (g_blink_duration[LED_IND_USB_RX] > 0) {
        if (now - g_blink_start[LED_IND_USB_RX] < g_blink_duration[LED_IND_USB_RX]) {
            LED2_On();
        } else {
            LED2_Off();
            g_blink_duration[LED_IND_USB_RX] = 0;
        }
    }

    /* LED3：CAN发送，触发后亮30ms */
    if (g_blink_duration[LED_IND_FDCAN_TX] > 0) {
        if (now - g_blink_start[LED_IND_FDCAN_TX] < g_blink_duration[LED_IND_FDCAN_TX]) {
            LED3_On();
        } else {
            LED3_Off();
            g_blink_duration[LED_IND_FDCAN_TX] = 0;
        }
    }

    /* LED4：SPI发送，触发后亮30ms */
    if (g_blink_duration[LED_IND_SPI_TX] > 0) {
        if (now - g_blink_start[LED_IND_SPI_TX] < g_blink_duration[LED_IND_SPI_TX]) {
            LED4_On();
        } else {
            LED4_Off();
            g_blink_duration[LED_IND_SPI_TX] = 0;
        }
    }

    /* LED5~LED8：电机掉线，由 system_control.c 直接控制，此处不处理 */
}

/* ==================== 触发LED闪烁 ==================== */
void LED_Indicator_Trigger(LED_IndicatorType type)
{
    uint32_t now = HAL_GetTick();

    switch (type) {
        case LED_IND_USB_RX:
            g_blink_start[LED_IND_USB_RX]    = now;
            g_blink_duration[LED_IND_USB_RX] = 50;
            break;

        case LED_IND_FDCAN_TX:
            g_blink_start[LED_IND_FDCAN_TX]    = now;
            g_blink_duration[LED_IND_FDCAN_TX] = 30;
            break;

        case LED_IND_SPI_TX:
            g_blink_start[LED_IND_SPI_TX]    = now;
            g_blink_duration[LED_IND_SPI_TX] = 30;
            break;

        default:
            break;
    }
}

/* ==================== 以下函数保留接口兼容，内部已简化 ==================== */
void LED_Indicator_SetError(LED_IndicatorType type, ErrorLevel level)
{
    (void)type;
    (void)level;
}

void LED_Indicator_ClearError(LED_IndicatorType type)
{
    (void)type;
}

uint32_t LED_Indicator_GetErrorCount(LED_IndicatorType type)
{
    (void)type;
    return 0;
}

void LED_Indicator_BeepAlarm(uint32_t duration_ms)
{
    /* 初始化阶段的短鸣请直接用 BEEP_On() + HAL_Delay() + BEEP_Off()
     * 此函数保留接口兼容，不做任何操作 */
    (void)duration_ms;
}

void LED_Indicator_BeepStop(void)
{
    BEEP_Off();
}

/* ==================== 上电自检：依次点亮所有LED和蜂鸣器 ==================== */
void LED_Indicator_TestMode(void)
{
    LED1_On();  HAL_Delay(200); LED1_Off();
    LED2_On();  HAL_Delay(200); LED2_Off();
    LED3_On();  HAL_Delay(200); LED3_Off();
    LED4_On();  HAL_Delay(200); LED4_Off();
    LED5_On();  HAL_Delay(200); LED5_Off();
    LED6_On();  HAL_Delay(200); LED6_Off();
    LED7_On();  HAL_Delay(200); LED7_Off();
    LED8_On();  HAL_Delay(200); LED8_Off();

    Beep_Init();
    BEEP_On();  HAL_Delay(100); BEEP_Off();

    LED1_On(); LED2_On(); LED3_On(); LED4_On();
    LED5_On(); LED6_On(); LED7_On(); LED8_On();
    HAL_Delay(500);
    LED1_Off(); LED2_Off(); LED3_Off(); LED4_Off();
    LED5_Off(); LED6_Off(); LED7_Off(); LED8_Off();
}
