#ifndef __NEC_H_
#define __NEC_H_

#include "n32g031.h"

/* ---------------- 载波参数 ----------------
 * TIM1_CH1 (PA8) 输出 38kHz 载波,由 IRM_Init() 配置:
 *   Prescaler = 48-1  -> 计数时钟 48MHz/48 = 1MHz (1 tick = 1us)
 *   Period    = 26-1  -> 载波周期 26us -> 38.46kHz
 *
 * 通道模式 PWM2 + OcPolarity_LOW (见 pwm.c 的 IRM_Init):
 *   CNT <  CCR -> 无效电平 -> 引脚为高
 *   CNT >= CCR -> 有效电平 -> 引脚为低
 * 即: 高电平时间 = CCR, 低电平时间 = 26 - CCR
 */
#define NEC_CARRIER_PERIOD      (26 - 1)    /* ARR */
#define NEC_CARRIER_PRESCALER   (48 - 1)    /* 1us tick */
#define NEC_CARRIER_TICKS       (NEC_CARRIER_PERIOD + 1)    /* 26 */

/* ---------------- 驱动极性 (必须与实际电路一致) ----------------
 * NEC_LED_ACTIVE_LOW = 1 : PA8 输出低电平时红外管导通
 *                          (发射管阳极经限流电阻接 VCC, PA8 灌电流)
 * NEC_LED_ACTIVE_LOW = 0 : PA8 输出高电平时红外管导通
 *                          (PA8 直接驱动发射管或 NPN 基极)
 *
 * 配错了接收端完全收不到,原因有两条:
 *   1. 1/3 占空比的载波被反相
 *   2. "载波关断"会变成"发射管持续点亮"
 */
#ifndef NEC_LED_ACTIVE_LOW
#define NEC_LED_ACTIVE_LOW      0
#endif

#if NEC_LED_ACTIVE_LOW
/* 导通时间 = 低电平时间 = 26 - CCR, 取 9us (约 1/3) -> CCR = 17 */
#define NEC_CARRIER_ON_CMP      (NEC_CARRIER_TICKS - 9)     /* 17 */
/* CCR > ARR -> CNT 恒小于 CCR -> 引脚恒为高 -> 发射管始终关断 */
#define NEC_CARRIER_OFF_CMP     NEC_CARRIER_TICKS           /* 26 */
#else
/* 导通时间 = 高电平时间 = CCR, 取 9us (约 1/3) -> CCR = 9 */
#define NEC_CARRIER_ON_CMP      9
/* CCR = 0 -> CNT 恒大于等于 CCR -> 引脚恒为低 -> 发射管关断 */
#define NEC_CARRIER_OFF_CMP     0
#endif

/* ---------------- 接收 ----------------
 * PA9 接一体化红外接收头 (VS1838 / HS0038 等) 的 VOUT。
 * 这类模块是低电平有效: 无载波时 VOUT 为高, 检测到 38kHz 载波时拉低。
 * 内部上拉即可,不需要外部电阻。
 */
#define NEC_RX_PORT             GPIOA
#define NEC_RX_PIN              GPIO_PIN_9
#define NEC_RX_ACTIVE()         (GPIO_ReadInputDataBit(NEC_RX_PORT, NEC_RX_PIN) == 0)

/* 相邻两帧起点之间的间隔,单位 ms (NEC 规范为 108ms) */
#define NEC_FRAME_PERIOD_MS     108

/* 在 TIM6 的 1ms 中断里递减,由 NEC_Handle() 处理并重装 */
extern __IO uint16_t NecGapTimeCount;

void NEC_Init(void);
void NEC_RxInit(void);

/* ---------------- 发送接口 ----------------
 * 所有发送函数都是非阻塞的: 使能 TIM1 update 中断后几微秒内即返回,
 * 整帧由 TIM1_BRK_UP_TRG_COM_IRQHandler() 在中断里发出。
 * 用 NEC_IsBusy() 查询是否发送完毕。
 *
 * NEC_IsBusy() 为真时调用发送函数会被直接忽略,不会破坏正在发送的帧。
 */
void NEC_SendFrame(uint8_t addr, uint8_t cmd);
void NEC_SendFrameExt(uint16_t addr, uint8_t cmd);
void NEC_SendRepeat(void);

/* 与 NEC_SendFrame() 相同,但会在引导码期间采样 RX 引脚。
 * 等 NEC_IsBusy() 变为 0 后用 NEC_RxSeen() 读结果。 */
void NEC_SendFrameVerify(uint8_t addr, uint8_t cmd);

/* 1 = 当前正在发送一帧 */
uint8_t NEC_IsBusy(void);

/* 1 = 最近一帧的 9ms 引导码期间接收头曾拉低 VOUT。
 * NEC_IsBusy() 返回 0 之后该值才有效。
 * 用来替代原来阻塞版的 NEC_SendFrameVerify() 返回值: 发一帧、等
 * NEC_IsBusy()==0、再读这个。采样范围从原来的单个瞬间变成整个引导码期间,
 * 灵敏度只会更高。
 */
uint8_t NEC_RxSeen(void);

/* ---------------- 持续发送 ----------------
 * NEC_StartRepeatMode() 先立即发一帧,随后每 NEC_FRAME_PERIOD_MS 重发一次。
 * 需要在主循环里调用 NEC_Handle() 来续发每一帧;无事可做时它立即返回,
 * 不会阻塞主循环。
 */
void NEC_StartRepeatMode(uint8_t addr, uint8_t cmd);
void NEC_StopRepeatMode(void);
void NEC_Handle(void);

#endif
