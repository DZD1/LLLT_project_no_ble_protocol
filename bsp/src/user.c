#include "user.h"
#include "gpio.h"
#include "usart.h"
#include "stdio.h"
#include "key.h"
#include "main.h" 
#include "ADC.h"
#include "pwm.h"
#include "delay.h"
#include "nec.h"
#include "timer.h"

/* ---------------- NEC 持续发码 ---------------- */
#define NEC_ADDR 0x00      /* 循环发送的地址, 按需改 */
#define NEC_CMD 0x45       /* 循环发送的命令, 按需改 */
#define NTC_OVER_HEAD 1500 /* ADC 值低于值表示温度>45*/
#define WEAR_ON_FRAMES 10
#define WEAR_OFF_FRAMES 2


/* 最近一帧的引导码期间接收头是否收到载波: 1=收到, 0=没收到, 0xFF=还没发过帧。
   每 108ms 刷新一次, 其他文件加 extern 后可随时读 */
uint8_t NecRxOk = 0xFF;
// static uint8_t NecFramePending = 0; /* 1 = 有一帧正在发, 发完要取采样结果 */

extern __IO uint16_t IdleTimeCount;
extern __IO uint16_t OneSecondTimeCount;
extern uint8_t Key, Fac, Mode;
extern uint16_t NTC1_Value, NTC2_Value, BAT_Value;
extern uint16_t ssw;
extern __IO uint32_t DelayTimeCount;
extern __IO uint16_t TreatmentCnt;
extern __IO uint16_t TreatmentTiming;
static uint8_t NecFramePending = 0;

#ifndef WEAR_REQUIRE_LINK_CHECK
#define WEAR_REQUIRE_LINK_CHECK 1
#endif

/* ---------------- 治疗模式选择 ----------------
 * 开机状态下短按电源键在四挡之间循环:
 *   待机 → 蓝光(LED2) → 红光(LED3) → 红蓝光(LED4) → 待机
 *
 * 取值直接用协议的 BLE_MODE_xxx, 不另立一套枚举: App 下发的 SET_MODE 后续要
 * 落到同一个变量上, 两套枚举迟早有一处忘了同步。
 *
 * 机器只有一个按键, 所以"选挡"和"开始治疗"是同一个动作 —— 选中非待机挡即置
 * 治疗态, 循环回待机挡即结束。这里只决定"想不想出光", 真的出不出光由下面的
 * 佩戴联锁裁决, 本函数不直接碰 VCSEL。
 *
 * 长按(KEY_POWER|KEY_LONG_FLAG, 0x09)是关机, 和短按(0x01)取值不同, 不会互相
 * 触发, 也不会在关机那一下顺带切一挡。
 */
__IO uint8_t s_mode;

static uint8_t WearOnCnt = 0;   /* 连续判为"已佩戴"的帧数 */
static uint8_t WearOffCnt = 0;  /* 连续判为"未佩戴"的帧数 */
static uint8_t WearLinkOk = 0;  /* 1 = 链路确认可用, 达到 WEAR_LINK_FRAMES 才置位 */
static uint8_t WearLinkCnt = 0; /* 连续收到载波的帧数, 单次瞬时误收不足以置信链路 */

#define WEAR_LINK_FRAMES 3 /* 连续收到几帧载波才认可链路可用, 防止一次瞬时干扰永久解锁佩戴判定 */

#define TREAT_SEC_BLUE 600U  /* 蓝光段 10 分钟 */
#define TREAT_SEC_RED 1200U  /* 红光段 20 分钟 */
#define TREAT_BEEP_STEP 300U /* 每 5 分钟一声进度提示 */

#define TREAT_PHASE_BLUE 0
#define TREAT_PHASE_RED 1

__IO uint8_t TreatPhase = TREAT_PHASE_BLUE;

/* 本段剩余秒数。摘下期间冻结不减, 戴回去从断点接着走 */
__IO uint16_t TreatSecLeft = 0; 
static void Wear_Update(uint8_t rx)
{
    if (rx)
    {
        /* 光路通畅: 收发两端和线束都是活的, 同时本帧判为未佩戴。
           连续收到 WEAR_LINK_FRAMES 帧才认可链路, 避免一次瞬时误收(干扰/
           接触瞬间导通)就永久解锁佩戴判定 */
        if (WearLinkCnt < WEAR_LINK_FRAMES)
            WearLinkCnt++;
        if (WearLinkCnt >= WEAR_LINK_FRAMES)
            WearLinkOk = 1;
        WearOnCnt = 0;
        if (WearOffCnt < WEAR_OFF_FRAMES)
            WearOffCnt++;
        if (WearOffCnt >= WEAR_OFF_FRAMES && F_IN_TOUCH)
        {
            S_OUT_TOUCH;
            LOG(0xFF, "WearOff\r\n");
        }
    }
    else
    {
        WearOffCnt = 0;
        WearLinkCnt = 0; /* 收不到载波说明链路本帧不通, 连续计数清零重来 */
        if (WearOnCnt < WEAR_ON_FRAMES)
            WearOnCnt++;

#if WEAR_REQUIRE_LINK_CHECK
        if (!WearLinkOk)
            return;
#endif
        if (WearOnCnt >= WEAR_ON_FRAMES && F_OUT_TOUCH)
        {
            S_IN_TOUCH;
        }
    }
}
/**
 * @brief 去抖后的佩戴状态。1=已戴上, 0=未戴上。
 * @note  安全联锁和 BLE 状态上报都用这个, 不要直接读 NEC_RxSeen()。
 */
uint8_t Wear_IsOn(void)
{
    return F_IN_TOUCH ? 1 : 0;
}

/**
 * @brief 短按电源键: 治疗模式步进一挡。
 * @note  循环顺序 待机 → 蓝光 → 红光 → 红蓝光 → 待机。
 *        只改 TreatMode, 不碰 VCSEL/LED —— 出光和指示灯都由 App_Handle 末尾
 *        按当前挡位统一驱动, 保证光输出只有一个写者。
 */
static void Mode_Next(void)
{
    switch (s_mode)
    {
    case BLE_MODE_RED:
        s_mode = BLE_MODE_BLUE;
        break;
    case BLE_MODE_BLUE:
        s_mode = BLE_MODE_RED_BLUE;
        break;
    case BLE_MODE_RED_BLUE:
        s_mode = BLE_MODE_OFF;
        break;
    default:
        s_mode = BLE_MODE_RED;
        break;
    }
}

/**
 * @brief 把当前挡位显示到 LED2/LED3/LED4。
 * @note  每轮无条件重刷三个灯, 不做"只在变化时改"的优化: 电平驱动比边沿驱动
 *        少一类"状态变了但灯没跟上"的问题, 三次 GPIO 写的开销可忽略。
 *        LED1 不在这里管 —— 它表示"正在出光", 由下面的治疗块驱动。
 */
static void Mode_ShowLed(void)
{
    switch (s_mode)
    {
    case BLE_MODE_RED:
        LED2_ON;
        LED3_OFF;
        LED4_OFF;
        break;
    case BLE_MODE_BLUE:
        LED2_OFF;
        LED3_ON;
        LED4_OFF;
        break;
    case BLE_MODE_RED_BLUE:
        LED2_OFF;
        LED3_OFF;
        LED4_ON;
        break;
    default:
        LED2_OFF;
        LED3_OFF;
        LED4_OFF;
        break;
    }
}

/**
 * @brief 驱动 LED1。低电闪烁优先于"正在出光"指示。
 * @param emitting 1 = 本轮确实在出光, 0 = 没出光
 * @note  LED1 有两个用途, 必须定优先级, 否则两处各写一次电平就会互相覆盖:
 *        低电(<20%)时 1Hz 闪烁盖过出光常亮 —— 低电是要用户处理的事, 而
 *        "正在出光"用户从帽子里的光就能看到, 不靠这个灯。
 *
 *        闪烁只在开机态生效: 关机后整机断电, 让灯继续闪没有意义。
 *        半周期由 TIM6 的 BatBlinkTimeCount 计, 减到 0 翻一次电平并重装,
 *        和 AdcScanTimeCount 一样是"减到 0 后保持"的约定, 主循环被阻塞
 *        (如 NEC 发帧、蜂鸣 delay)只会让这次翻转延后, 不会漏掉。
 */
static void Led1_Update(void)
{
    if (F_BAT_LOW && F_POWER_ON)
    {
        if (BatBlinkTimeCount == 0)
        {
            BatBlinkTimeCount = BAT_BLINK_HALF_MS;
            LED1_TOGGLE;
        }
        return;
    }

    if (F_POWER_ON && F_BAT_OK)
        LED1_ON; /* 正在出光 */
    else
        LED1_OFF;
}

/**
 * @brief 按当前挡位装载治疗时长, 并置治疗态。
 * @note  挡位到"第一段是什么光、跑多少秒"的映射只在这里定义一次。
 *        红蓝光挡从蓝光段起步, 蓝光段跑完由 Treat_Tick() 接到红光段。
 */
static void Treat_Start(void)
{
    S_IN_TREATMENT;
    S_OUT_TREAT_PAUSE;

    if (s_mode == BLE_MODE_RED)
    {
        TreatPhase = TREAT_PHASE_RED;
        TreatSecLeft = TREAT_SEC_RED;
    }
    else /* 蓝光挡, 以及红蓝光挡的第一段 */
    {
        TreatPhase = TREAT_PHASE_BLUE;
        TreatSecLeft = TREAT_SEC_BLUE;
    }
}
static void Treat_Stop(void)
{
    S_OUT_TREATMENT;
    TreatSecLeft = 0;
    s_mode = BLE_MODE_OFF;
}

/**
 * @brief 治疗计时, 每满 1 秒调一次(仅在确实出光时)。
 * @note  摘下/暂停时调用方不会进来, 于是剩余秒数天然冻结在断点。
 */
static void Treat_Tick(void)
{
    if (TreatSecLeft > 0)
        TreatSecLeft--;

    if (TreatSecLeft > 0)
    {
        /* 每 5 分钟一声短提示。600/300 都是整数倍, 段末的 0 归下面处理 */
        if ((TreatSecLeft % TREAT_BEEP_STEP) == 0)
        {
            BEEP_ON;
            delay_ms(200);
            BEEP_OFF;
        }
        /*LOG(0xFF, "Treat %s:%d\r\n",
            (TreatPhase == TREAT_PHASE_BLUE) ? "blue" : "red", TreatSecLeft);*/
        return;
    }

    /* 本段走完 */
    if (s_mode == BLE_MODE_RED_BLUE && TreatPhase == TREAT_PHASE_BLUE)
    {
        /* 红蓝光挡: 蓝光 10 分钟结束, 接红光 20 分钟 */
        TreatPhase = TREAT_PHASE_RED;
        TreatSecLeft = TREAT_SEC_RED;
        BEEP_ON;
        delay_ms(500);
        BEEP_OFF;
        return;
    }
    BEEP_ON;
    delay_ms(1000);
    BEEP_OFF;
    Treat_Stop();
}

void App_Handle(void)
{
    uint8_t i = 0;
    uint8_t NecRx;
    if (F_POWER_OFF)
    {
        if (Key == (KEY_POWER | KEY_LONG_FLAG) && F_THERMAL_OK) // 电源按键长按从关机状态进入开机状态
        {
            SYS_POWER_ON;
            for (i = 0; i < 4; i++)
            {
                LED1_TOGGLE;
                LED2_TOGGLE;
                LED3_TOGGLE;
                LED4_TOGGLE;
                delay_ms(200);
            }
            OneSecondTimeCount = 1000;
            S_POWER_ON;
        }
    }
    else
    {
        /* 每 108ms 发一帧 NEC 并在引导码期间采样 RX。整帧由 TIM1 中断发出,
       这里只做调度, 不阻塞, 下面的按键/治疗逻辑照常执行 */
        if (!NEC_IsBusy())
        {
            if (NecFramePending) /* 上一帧刚发完, 锁存它的采样结果 */
            {
                NecFramePending = 0;
                NecRx = NEC_RxSeen();
                if (NecRx != NecRxOk) /* 只在变化时打印, 避免每 108ms 刷屏 */
                {
                    NecRxOk = NecRx;
                    LOG(0xFF, "NEC rx=%d\r\n", NecRxOk);
                }
                /* 原始采样喂给去抖层。NecRxOk 只是给调试看的原始值,
                    业务判断一律走 Wear_IsOn() */
                Wear_Update(NecRx);
            }
            if (NecGapTimeCount == 0) /* 起下一帧 */
            {
                NecGapTimeCount = NEC_FRAME_PERIOD_MS;
                NEC_SendFrameVerify(NEC_ADDR, NEC_CMD);
                NecFramePending = 1;
            }
        }
        if (F_THERMAL_OK && (NTC1_Value < NTC_OVER_HEAD || NTC2_Value < NTC_OVER_HEAD))
        {
            S_THERMAL_ERR;
        }
        else if (F_THERMAL_ERR && (NTC1_Value > NTC_OVER_HEAD && NTC2_Value > NTC_OVER_HEAD))
        {
            S_THERMAL_OK;
        }

        if(F_IN_TREATMENT && F_OUT_TREAT_PAUSE && !Wear_IsOn())
        {
            S_IN_TREAT_PAUSE;
        }
        else if(F_IN_TREATMENT && F_IN_TREAT_PAUSE && Wear_IsOn())
        {
            S_OUT_TREAT_PAUSE;
        }
        if (Key == (KEY_POWER | KEY_LONG_FLAG) || IdleTimeCount >= 600) // 电源按键长按从开机状态进入关机状态，或者空闲10分钟自动关机F
        {
            OneSecondTimeCount = 1000;
            IdleTimeCount = 0;
            DelayTimeCount = 0;
            TreatSecLeft = 0; /* ssw=0 已退出治疗态, 剩余秒数一并清掉 */
            ssw = 0;
            /* 关机连佩戴判定一起清掉: 下次开机重新做一遍链路自检, 也避免上一次
               的计数残留让开机后一帧就认定戴上 */
            WearOnCnt = WearOffCnt = WearLinkOk = 0;
            s_mode = BLE_MODE_OFF; /* 回待机档, 三个模式灯随之熄灭 */
            VCSEL_PWR_OFF;
            VCSEL_OFF;
            BLUE_LED_OFF;
            S_POWER_OFF;
            SYS_POWER_OFF;
        }
        if (Key == KEY_POWER)
        {
            Mode_Next();
            if (s_mode == BLE_MODE_OFF)
                Treat_Stop(); /* 循环回待机挡 = 主动结束 */
            else
                Treat_Start(); /* 三个出光挡的装载逻辑收在一处 */
        }
        else if (F_THERMAL_ERR) /* 过温: 无条件断光回待机, 时长走完由 Treat_Tick 处理 */
        {
            Treat_Stop();
        }
        Mode_ShowLed();
    }
    Led1_Update(); /* 出光中: 常亮, 低电时改为闪烁 */
    if (F_IN_TREATMENT && F_OUT_TREAT_PAUSE && (s_mode != BLE_MODE_OFF) && F_THERMAL_OK && F_BAT_NOT_EMPTY) // TODO：头戴式检测条件需要加上
    {
        /* 出光: 两路互斥, 只看 TreatPhase。挡位到颜色的映射在 Treat_Start()
           里已经定过, 这里不再各挡判一遍。
           两路都显式写一次电平, 别只开该开的那路 —— 红蓝光挡切段时蓝光是开着的,
           漏掉 BLUE_LED_OFF 就会在红光段里蓝红同亮。 */
        if (TreatPhase == TREAT_PHASE_RED)
        {
            BLUE_LED_OFF;
            VCSEL_PWR_ON;
            VCSEL_ON;
        }
        else
        {
            VCSEL_OFF;
            VCSEL_PWR_ON; /* 蓝光段不给 VCSEL 供电, 不只是占空比归零 */
            BLUE_LED_ON;
        }
        IdleTimeCount = 0;
        if (!OneSecondTimeCount) // 治疗中，计时器每秒中断一次，治疗时间计数递减
        {
            OneSecondTimeCount = 1000;
            Treat_Tick();
        }
    }
    else 
    {
        /* 待机挡、用户暂停、摘下、关机 —— 四种情形都在这里断光。
           重装 OneSecondTimeCount 使治疗计时冻结在原处: 摘下期间不递减,
           戴回去从断点接着走。 */
        VCSEL_OFF;
        VCSEL_PWR_OFF;
        BLUE_LED_OFF;
        Led1_Update(); /* 没出光: 常灭, 低电时仍要闪 */
        if(!OneSecondTimeCount)
        {
           OneSecondTimeCount = 1000; 
           IdleTimeCount++;
        }
    }
    Key |= KEY_DONE_FLAG;
    Fac |= KEY_DONE_FLAG;
}
