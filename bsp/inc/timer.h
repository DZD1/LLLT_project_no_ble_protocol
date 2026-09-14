#ifndef __TIMER_H_
#define __TIMER_H_

#include "n32g031.h"

void TIMER_Init(void);
/* 开机至今的秒数, 由 TIM6 的 1ms 中断累加。只读。
   给 BLE 的 PING 响应用, 也可用于日志打时间戳 */
uint32_t TIMER_GetUptimeSeconds(void);

/* 低电闪烁的半周期计数(ms), 由 TIM6 递减到 0 后保持, 由消费者翻灯并重装。
   与 AdcScanTimeCount 那几个同一套约定: 减到 0 不自动重装, 主循环被阻塞
   只会让这一次闪烁延后, 不会漏掉 */
extern __IO uint16_t BatBlinkTimeCount;
#define BAT_BLINK_HALF_MS 500 /* 半周期 500ms -> 1Hz 闪烁 */
#endif

