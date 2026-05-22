/**
 * @file tim_delay.c
 * @brief 基于SysTick的延迟工具实现
 */

#include "tim_delay.h"

/**
 * @brief 阻塞式延迟
 * @note  底层使用HAL_GetTick()（SysTick，1ms精度），不占用额外硬件定时器。
 *        主循环中禁止使用此函数，会阻塞所有任务。
 */
void Tim_Delay(uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < ms);
}

/**
 * @brief 非阻塞式定时检查
 * @note  调用后立即返回，不阻塞主循环。
 *        内部使用无符号减法，自动处理HAL_GetTick()溢出（约49.7天回绕）。
 */
uint8_t Tim_IsElapsed(uint32_t *last_tick, uint32_t interval_ms)
{
    if ((HAL_GetTick() - *last_tick) >= interval_ms) {
        *last_tick = HAL_GetTick();
        return 1;
    }
    return 0;
}

/**
 * @brief 获取当前毫秒时间戳
 */
uint32_t Tim_GetMs(void)
{
    return HAL_GetTick();
}
