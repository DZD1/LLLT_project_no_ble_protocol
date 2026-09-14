#ifndef _ADC_H_
#define _ADC_H_

#include "n32g031.h"

/* ADC采样间隔(ms) */
#define ADC_SCAN_INTERVAL   250

extern __IO uint16_t AdcScanTimeCount;

extern __IO uint16_t AdcScanTimeCount;

/* ---------------- 电池电量 ----------------
 * 2S 锂电经 1/3 分压接 PA5, 所以 BAT_Value(引脚毫伏) x 3 = 电池端毫伏。
 * 满电 8.4V 在引脚上是 2.8V, 空电 6.0V 是 2.0V, 都在 3.3V 量程内。
 *
 * 只报 100/75/50/25/0 五个值, 不做连续百分比: 锂电放电曲线中段很平,
 * 单点电压换算出的连续百分比在负载变化时会来回跳, 分档反而更可信。
 */
#define BAT_PERCENT_FULL  100
#define BAT_PERCENT_75    75
#define BAT_PERCENT_50    50
#define  BAT_PERCENT_25    25
#define BAT_PERCENT_20    20
#define BAT_PERCENT_EMPTY 0

/* 当前电量档位(上面五个值之一)。由 ADC_Scan 每 250ms 更新, 外部只读 */
uint8_t Bat_GetPercent(void);
void Adc_Init(void);
uint16_t ADC_GetData(uint8_t ADC_Channel);
void ADC_Scan(void);
#endif

