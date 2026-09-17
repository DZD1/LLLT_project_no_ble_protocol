#include "nec.h"
#include "pwm.h"

/* ---------------- NEC 协议时序 (单位 us) ----------------
 * 引导码 : 9000us 载波 + 4500us 空闲
 * 逻辑 0 :  560us 载波 +  560us 空闲   (共 1120us)
 * 逻辑 1 :  560us 载波 + 1690us 空闲   (共 2250us)
 * 数据   : 地址 + 地址反码 + 命令 + 命令反码, 每字节低位先发
 * 停止位 :  560us 载波
 * 重复码 : 9000us 载波 + 2250us 空闲 + 560us 载波, 每 108ms 一次
 */
#define NEC_LEAD_ON         9000
#define NEC_LEAD_OFF        4500
#define NEC_REPEAT_OFF      2250
#define NEC_BIT_ON          560
#define NEC_BIT0_OFF        560
#define NEC_BIT1_OFF        1690

/* 引导码前约 1ms 不采样 RX, 留给接收头 AGC 稳定。1000us / 26us = 38 个载波周期 */
#define NEC_RX_SETTLE_TICKS 38

/* ---------------- 发送状态机 ---------------- */
typedef enum
{
    NEC_ST_IDLE = 0,
    NEC_ST_LEAD_ON,
    NEC_ST_LEAD_OFF,
    NEC_ST_BIT_ON,
    NEC_ST_BIT_OFF,
    NEC_ST_STOP_ON
} NEC_State;

static volatile NEC_State NecState   = NEC_ST_IDLE;
static volatile uint16_t  NecTicks   = 0;    /* 当前元素还剩几个载波周期 */
static volatile uint32_t NecData     = 0;    /* 待发数据, 低位先发 */
static volatile uint8_t  NecBitIdx   = 0;    /* 下一个要发的位序号 */
static volatile uint8_t  NecBitTotal = 0;    /* 普通帧为 32, 重复码为 0 */
static volatile int16_t  NecErr      = 0;    /* 累计量化误差(us) */
static volatile uint8_t  NecRxSeen   = 0;    /* 引导码期间 RX 是否拉低过 */
static volatile uint16_t NecRxSettle = 0;    /* 采样 RX 前还需等待的周期数 */
static volatile uint8_t  NecVerify   = 0;    /* 1:本帧需要采样 RX */

/* 自动重发调度, 由 TIM6 的 1ms 中断递减 */
__IO uint16_t NecGapTimeCount = 0;
static volatile uint8_t  NecAutoRepeat = 0;
static volatile uint8_t  NecAutoAddr   = 0;
static volatile uint8_t  NecAutoCmd    = 0;

static void NEC_CarrierOn(void)
{
    TIM_SetCmp1(TIM1, NEC_CARRIER_ON_CMP);
}

static void NEC_CarrierOff(void)
{
    TIM_SetCmp1(TIM1, NEC_CARRIER_OFF_CMP);
}

/**
 * @brief 把 us 时长换算成整数个载波周期, 并补偿前面元素残留的误差。
 * @note  载波周期 26us 除不尽 560us。若每个元素独立四舍五入, 所有边沿会朝
 *        同一方向偏移; 把误差带到下一个元素可以让每条边沿误差保持在
 *        ±13us 以内, 且不会在一帧内累积。
 */
static uint16_t NEC_Ticks(uint16_t us)
{
    int32_t  need = (int32_t)us - NecErr;
    uint16_t n;

    if (need < (int32_t)NEC_CARRIER_TICKS)
        need = NEC_CARRIER_TICKS;               /* 不允许排出 0 个周期 */

    /* 四舍五入而非截断 */
    n = (uint16_t)((need + NEC_CARRIER_TICKS / 2) / NEC_CARRIER_TICKS);
    if (n == 0)
        n = 1;

    /* 误差 = 实际将发出的时长 - 本应发出的时长 */
    NecErr = (int16_t)((int32_t)n * NEC_CARRIER_TICKS - need);

    return n;
}

/**
 * @brief 装入待发数据并启动状态机。
 * @param data     待发数据, 低位先发
 * @param bits     普通帧为 32, 重复码为 0
 * @param verify   1:引导码期间采样 RX 引脚
 * @note  引导码之后的空闲时长不作为参数传入: 中断里根据 bits 是否为 0
 *        自行选择 4500us(普通帧) 或 2250us(重复码)。
 */
static void NEC_Start(uint32_t data, uint8_t bits, uint8_t verify)
{
    NecData     = data;
    NecBitTotal = bits;
    NecBitIdx   = 0;
    NecErr      = 0;
    NecRxSeen   = 0;
    NecVerify   = verify;
    NecRxSettle = NEC_RX_SETTLE_TICKS;
    NecState    = NEC_ST_LEAD_ON;

    NEC_CarrierOn();
    NecTicks = NEC_Ticks(NEC_LEAD_ON);

    /* 先清掉可能已挂起的 update 标志, 再开中断 */
    TIM1->STS = (uint32_t)~TIM_FLAG_UPDATE;
    TIM_ConfigInt(TIM1, TIM_INT_UPDATE, ENABLE);
}

/**
 * @brief 初始化 38kHz 载波和调制中断。
 *        与 pwm.c 的 IRM_Init() 共用 TIM1_CH1 / PA8。
 */
void NEC_Init(void)
{
    NVIC_InitType NVIC_InitStructure;

    IRM_Init(NEC_CARRIER_PERIOD, NEC_CARRIER_PRESCALER);
    /* 关掉 CCR 预装载, 使载波开关立即生效, 而不是延迟一个载波周期 */
    TIM_ConfigOc1Preload(TIM1, TIM_OC_PRE_LOAD_DISABLE);
    NEC_CarrierOff();

    /* TIM1 的 update 中断驱动 NEC 状态机。优先级设 0, 让位时序优先于
     * TIM6 的 1ms 时基(优先级 1) */
    NVIC_InitStructure.NVIC_IRQChannel         = TIM1_BRK_UP_TRG_COM_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd      = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 中断在发送启动前保持关闭, 空闲时不为 38kHz 的 update 事件付出任何开销 */
    TIM_ConfigInt(TIM1, TIM_INT_UPDATE, DISABLE);

    NecState = NEC_ST_IDLE;
}

/**
 * @brief 把 PA9 配置为红外接收头 VOUT 的输入, 并把 EXTI9 接到这个引脚上。
 *        一体化接收头内部有上拉, 输出为开漏。
 * @note  EXTI9 只做下降沿捕获, 平时保持屏蔽(IMASK 清零), 只在引导码稳定期
 *        结束后由 TIM1 中断打开, 离开引导码时关闭 —— 见
 *        TIM1_BRK_UP_TRG_COM_IRQHandler()。开着的这段时间里, 一旦接收头拉低
 *        VOUT 就直接置位 NecRxSeen, 不需要 TIM1 中断每 26us 轮询一次 IO。
 */
void NEC_RxInit(void)
{
    GPIO_InitType GPIO_InitStructure;
    EXTI_InitType EXTI_InitStructure;
    NVIC_InitType NVIC_InitStructure;

    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_AFIO, ENABLE);

    GPIO_InitStruct(&GPIO_InitStructure);
    GPIO_InitStructure.Pin       = NEC_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.GPIO_Pull = GPIO_PULL_UP;
    GPIO_InitPeripheral(NEC_RX_PORT, &GPIO_InitStructure);

    GPIO_ConfigEXTILine(GPIOA_PORT_SOURCE, GPIO_PIN_SOURCE9);

    EXTI_InitStruct(&EXTI_InitStructure);
    EXTI_InitStructure.EXTI_Line    = EXTI_LINE9;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&EXTI_InitStructure);
    /* 配置完触发边沿后立即屏蔽, 平时不产生中断 */
    EXTI->IMASK &= ~EXTI_LINE9;
    EXTI_ClrITPendBit(EXTI_LINE9);

    NVIC_InitStructure.NVIC_IRQChannel         = EXTI4_15_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority = 1;   /* 与 TIM6 同级, 让位给 TIM1 的位时序 */
    NVIC_InitStructure.NVIC_IRQChannelCmd      = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief EXTI9 (PA9) 下降沿中断: 引导码期间接收头拉低了 VOUT。
 * @note  只在引导码的"稳定期结束→引导码结束"这段窗口内会被打开, 见
 *        TIM1_BRK_UP_TRG_COM_IRQHandler()。窗口外触发不到这里, 因为线已被屏蔽。
 */
void EXTI4_15_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_LINE9) != RESET)
    {
        EXTI_ClrITPendBit(EXTI_LINE9);
        NecRxSeen = 1;
    }
}

/**
 * @brief TIM1 update 中断, 每 26us(一个载波周期) 进一次。
 *        数载波周期, 在元素边界翻转载波。
 * @note  这里直接操作寄存器而不用驱动函数: 38.46kHz 下函数调用开销
 *        大约要吃掉 1% 的 CPU。
 */
void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    if ((TIM1->STS & TIM_FLAG_UPDATE) == 0)
        return;
    TIM1->STS = (uint32_t)~TIM_FLAG_UPDATE;

    /* 引导码期间跳过开头让接收头稳定的那段, 稳定期一结束就打开 EXTI9 边沿捕获,
     * 后续不必再每 26us 轮询一次 PA9 —— 接收头一拉低 VOUT, EXTI9_IRQHandler
     * 会直接置位 NecRxSeen */
    if (NecVerify && NecState == NEC_ST_LEAD_ON && NecRxSettle && --NecRxSettle == 0)
    {
        EXTI_ClrITPendBit(EXTI_LINE9);   /* 清掉稳定期内可能因抖动挂起的标志 */
        EXTI->IMASK |= EXTI_LINE9;
    }

    if (NecTicks > 1)
    {
        NecTicks--;
        return;                     /* 当前元素未结束, 这是最常走的路径 */
    }

    /* 当前元素刚结束, 推进状态机 */
    switch (NecState)
    {
    case NEC_ST_LEAD_ON:
        NEC_CarrierOff();
        EXTI->IMASK &= ~EXTI_LINE9;    /* 离开引导码, 关闭 RX 边沿捕获 */
        /* bits 为 0 表示这是重复码, 引导码后的空闲为 2250us */
        NecTicks = NEC_Ticks(NecBitTotal ? NEC_LEAD_OFF : NEC_REPEAT_OFF);
        NecState = NEC_ST_LEAD_OFF;
        break;

    case NEC_ST_LEAD_OFF:
        NEC_CarrierOn();
        NecTicks = NEC_Ticks(NEC_BIT_ON);
        /* 重复码没有数据位, 直接跳到停止位 */
        NecState = NecBitTotal ? NEC_ST_BIT_ON : NEC_ST_STOP_ON;
        break;

    case NEC_ST_BIT_ON:
        NEC_CarrierOff();
        NecTicks = NEC_Ticks(((NecData >> NecBitIdx) & 0x01)
                             ? NEC_BIT1_OFF : NEC_BIT0_OFF);
        NecState = NEC_ST_BIT_OFF;
        break;

    case NEC_ST_BIT_OFF:
        NecBitIdx++;
        NEC_CarrierOn();
        NecTicks = NEC_Ticks(NEC_BIT_ON);
        NecState = (NecBitIdx >= NecBitTotal) ? NEC_ST_STOP_ON : NEC_ST_BIT_ON;
        break;

    case NEC_ST_STOP_ON:
    default:
        NEC_CarrierOff();
        TIM_ConfigInt(TIM1, TIM_INT_UPDATE, DISABLE);
        NecState = NEC_ST_IDLE;
        break;
    }
}

uint8_t NEC_IsBusy(void)
{
    return (NecState != NEC_ST_IDLE) ? 1 : 0;
}

uint8_t NEC_RxSeen(void)
{
    return NecRxSeen;
}

/**
 * @brief 发送一帧标准 NEC 码 (8 位地址 + 8 位命令)。
 * @note  立即返回。整帧约 67.5ms 由中断发完, 用 NEC_IsBusy() 查询是否结束。
 */
void NEC_SendFrame(uint8_t addr, uint8_t cmd)
{
    uint32_t data;

    if (NEC_IsBusy())
        return;

    data = (uint32_t)addr
         | ((uint32_t)(uint8_t)(~addr) << 8)
         | ((uint32_t)cmd << 16)
         | ((uint32_t)(uint8_t)(~cmd) << 24);

    NEC_Start(data, 32, 0);
}

/**
 * @brief 发一帧, 同时在引导码期间采样 PA9, 判断接收头是否真的收到载波。
 * @note  立即返回; 等 NEC_IsBusy() 变为 0 后用 NEC_RxSeen() 读结果。
 */
void NEC_SendFrameVerify(uint8_t addr, uint8_t cmd)
{
    uint32_t data;

    if (NEC_IsBusy())
        return;

    data = (uint32_t)addr
         | ((uint32_t)(uint8_t)(~addr) << 8)
         | ((uint32_t)cmd << 16)
         | ((uint32_t)(uint8_t)(~cmd) << 24);

    NEC_Start(data, 32, 1);
}

/**
 * @brief 发送一帧扩展 NEC 码 (16 位地址, 不发地址反码)。
 */
void NEC_SendFrameExt(uint16_t addr, uint8_t cmd)
{
    uint32_t data;

    if (NEC_IsBusy())
        return;

    data = (uint32_t)addr
         | ((uint32_t)cmd << 16)
         | ((uint32_t)(uint8_t)(~cmd) << 24);

    NEC_Start(data, 32, 0);
}

/**
 * @brief 发送重复码, 按键长按时每 108ms 发一次。
 */
void NEC_SendRepeat(void)
{
    if (NEC_IsBusy())
        return;

    NEC_Start(0, 0, 0);
}

/**
 * @brief 开始每 108ms 重发同一帧, 直到 NEC_StopRepeatMode()。
 */
void NEC_StartRepeatMode(uint8_t addr, uint8_t cmd)
{
    NecAutoAddr     = addr;
    NecAutoCmd      = cmd;
    NecAutoRepeat   = 1;
    NecGapTimeCount = 0;            /* 下一次 NEC_Handle() 就发 */
}

void NEC_StopRepeatMode(void)
{
    NecAutoRepeat = 0;
}

/**
 * @brief 自动重发调度。在主循环里调用, 不会阻塞。
 */
void NEC_Handle(void)
{
    if (!NecAutoRepeat)
        return;

    if (NecGapTimeCount == 0 && !NEC_IsBusy())
    {
        NecGapTimeCount = NEC_FRAME_PERIOD_MS;
        NEC_SendFrame(NecAutoAddr, NecAutoCmd);
    }
}
