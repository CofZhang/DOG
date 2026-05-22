/**
 * @file tim_delay.h
 * @brief 基于SysTick的延迟工具
 *
 * 提供两种延迟方式：
 *   Tim_Delay()       - 阻塞式延迟，用于初始化阶段（替代HAL_Delay）
 *   Tim_IsElapsed()   - 非阻塞式定时检查，用于主循环中定时触发任务
 *
 * 用法示例（主循环非阻塞定时）：
 *   static uint32_t t = 0;
 *   if (Tim_IsElapsed(&t, 100)) {
 *       // 每100ms执行一次
 *   }
 */

#ifndef __TIM_DELAY_H
#define __TIM_DELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief 阻塞式延迟（单位：ms）
 * @note  仅在初始化阶段使用，主循环中请用 Tim_IsElapsed()
 */
void Tim_Delay(uint32_t ms);

/**
 * @brief 非阻塞式定时检查
 * @param last_tick   上次触发时的时间戳（由函数自动更新，首次传入0即可）
 * @param interval_ms 期望的触发间隔（ms）
 * @return 1 = 间隔已到，0 = 尚未到达
 *
 * @note 首次调用时 *last_tick 为0，会立即返回1并记录当前时间。
 *       若需要首次不立即触发，初始化时将 *last_tick 赋值为 HAL_GetTick()。
 */
uint8_t Tim_IsElapsed(uint32_t *last_tick, uint32_t interval_ms);

/**
 * @brief 获取当前毫秒时间戳（自上电起）
 */
uint32_t Tim_GetMs(void);

#ifdef __cplusplus
}
#endif

#endif /* __TIM_DELAY_H */
