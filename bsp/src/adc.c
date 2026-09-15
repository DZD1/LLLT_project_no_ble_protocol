#include "ADC.h"
#include "main.h"
#include"timer.h"
extern __IO uint16_t OneSecondTimeCount;
extern uint16_t ssw;
uint16_t NTC1_Value, NTC2_Value, BAT_Value;

/* ---------------- 电量分档 ----------------
 * 2S 锂电经 1/3 分压接 PA5, 所以 BAT_Value(引脚毫伏) x 3 = 电池端毫伏。
 * 比较一律在**电池端毫伏**上做, 不把门限除到引脚侧: 门限除以 3 会截断
 * (7000/3=2333, 折回去只有 6999), 乘到电池侧则是精确的。8400 远小于
 * uint16_t 上限, 不会溢出。
 *
 * 单节按 4.2/3.95/3.8/3.65/3.5V 取点, x2 得 2S 值。中段(50%~75%)刻意取得密,
 * 因为锂电放电曲线在 3.7~4.0V 那段很平, 门限拉开会导致 75% 档停留过久。
 */
#define BAT_DIVIDER 3							 /* 1/3 分压 */
#define BAT_PIN_TO_MV(pin) ((pin) * BAT_DIVIDER) /* 引脚毫伏 -> 电池端毫伏 */

#define BAT_MV_FULL 8400	/* 4.20V/节 -> 100% */
#define BAT_MV_75 7900		/* 3.95V/节 */
#define BAT_MV_50 7600		/* 3.80V/节 */
#define BAT_MV_25 7300		/* 3.65V/节 */
#define BAT_MV_LOW 7000		  /* 3.50V/节 -> 低于 20%, 触发闪烁 */
#define BAT_MV_LOW_CLR 7200	  /* 回差: 高于此值才解除低电 */
#define BAT_MV_EMPTY 6300	  /* 3.00V/节, 放电截止。低于此值报 0% 并禁止出光 */
#define BAT_MV_EMPTY_CLR 6700 /* 回差: 高于此值才解除空电 */

/* 低电判定要连续 8 次(约 2s)成立才置位。
 * VCSEL 出光是脉冲负载, 会把电池电压瞬时拉低几百毫伏; 单次采样直接置位会在
 * 治疗中误报低电并让 LED1 无故闪烁。回差 + 连续计数一起用, 两个方向都不抖。 */
#define BAT_LOW_CONFIRM 8

/* 空电判定同样要连续确认, 而且比低电更不能误判: 低电误判只是灯闪, 空电误判会
 * 直接断光停机。脉冲负载在接近放电截止时内阻已经变大, 瞬时跌落比满电时更深,
 * 所以确认次数取得比低电多一倍(约 4s), 并配 300mV 回差防止停机后电压回弹又启动。 */
#define BAT_EMPTY_CONFIRM 16

static uint8_t BatPercent = BAT_PERCENT_FULL; /* 开机先当满电, 由首次采样纠正 */
static uint8_t BatLowCnt = 0;
static uint8_t BatEmptyCnt = 0;

/**
 * @brief 按最新 BAT_Value 更新电量档位和低电标志。每次 ADC 采样后调一次。
 * @note  分成两段互不干扰的判断, 刻意不写成一条 if/else 链:
 *        档位链是从上往下吃区间的, 低电标志的回差判断一旦排在它后面就再也
 *        拿不到控制权(7.9~8.4V 会被 100% 那档吃掉), 于是低电置位后无法解除。
 *        两段各自独立判一次, 就没有这个顺序依赖。
 *
 *        档位本身不做回差: 档位只用于显示, 边界上抖一格无害, 加回差反而会让
 *        充电时的档位迟迟不涨。低电标志要驱动 LED1 闪烁, 必须做回差。
 */
static void Bat_Update(void)
{
	uint16_t mv = BAT_PIN_TO_MV(BAT_Value); /* 电池端毫伏 */

	/* ---- 第一段: 显示档位 ---- */
	/* 上界开放到无穷: 充电器顶电压 8.4V 叠上 ADC 噪声很容易越过 BAT_MV_FULL,
	   写成 mv <= BAT_MV_FULL 的话这些采样会全部落空, BatPercent 保留上一次的
	   旧值(实测 8.403V 时仍报 50%)。 */
	if (mv > BAT_MV_75)
		BatPercent = BAT_PERCENT_FULL;
	else if (mv > BAT_MV_50)
		BatPercent = BAT_PERCENT_75;
	else if (mv > BAT_MV_25)
		BatPercent = BAT_PERCENT_50;
	else if (mv > BAT_MV_LOW)
		BatPercent = BAT_PERCENT_25;
	else if (mv > BAT_MV_EMPTY)
		BatPercent = BAT_PERCENT_20;
	else
		BatPercent = BAT_PERCENT_EMPTY; /* 放空, 或分压电阻脱焊导致读数为 0 */

	/* ---- 第二段: 低电标志(带回差与连续确认) ---- */
	if (mv <= BAT_MV_LOW)
	{
		if (BatLowCnt < BAT_LOW_CONFIRM)
			BatLowCnt++;
		if (BatLowCnt >= BAT_LOW_CONFIRM)
			S_BAT_LOW;
	}
	else if (mv >= BAT_MV_LOW_CLR) /* 回差以上才解除, 免得在门限上抖 */
	{
		BatLowCnt = 0;
		S_BAT_OK;
	}
	else
	{
		/* 落在回差带内(7.0~7.2V): 标志维持现状。计数必须清零 ——
		   否则"连续 8 次"退化成"累计 8 次", VCSEL 每次脉冲拉低都累加一次,
		   治疗中约 2s 就凑满并误报低电, 正好打掉这个计数器的设计目的。 */
		BatLowCnt = 0;
	}

	/* ---- 第三段: 空电标志(放电截止, 断光并拒绝启动) ----
	   和低电分成两段独立判定, 结构与第二段一致: 空电必然也是低电, 但反过来不成立,
	   写成 if/else 链会让空电吃掉低电的判定机会, LED1 就不闪了。 */
	if (mv <= BAT_MV_EMPTY)
	{
		if (BatEmptyCnt < BAT_EMPTY_CONFIRM)
			BatEmptyCnt++;
		if (BatEmptyCnt >= BAT_EMPTY_CONFIRM)
			S_BAT_EMPTY;
	}
	else if (mv >= BAT_MV_EMPTY_CLR)
	{
		BatEmptyCnt = 0;
		S_BAT_NOT_EMPTY;
	}
	else
	{
		BatEmptyCnt = 0; /* 回差带内(6.0~6.3V): 维持现状, 计数清零, 同第二段 */
	}
}

/**
 * @brief 当前电量档位。
 * @return BAT_PERCENT_xxx 之一(100/75/50/25/0)。
 * @note  BLE 上报和 LED1 低电闪烁都用这个, 不要直接读 BAT_Value 各判一套。
 */
uint8_t Bat_GetPercent(void)
{
	return BatPercent;
}

/* ADC采样间隔计数,在TIM6的1ms中断里递减,减到0后由ADC_Scan采样并重装。
   用"减到0"而不是判断某个特定值,计数到0后会一直保持0等待处理,
   即使主循环被长时间阻塞(如NEC发送)也只是延后采样,不会漏掉。 */
__IO uint16_t AdcScanTimeCount = ADC_SCAN_INTERVAL;

void Adc_Init(void)
{
	ErrorStatus HSIStartUpStatus;
	GPIO_InitType GPIO_InitStructure;
	ADC_InitType ADC_InitStructure;

	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
	/* Enable ADC clocks */
	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);
	/* enable ADC 1M clock */
	RCC_EnableHsi(ENABLE);
	/* Wait til1 HSI is ready*/
	HSIStartUpStatus = RCC_WaitHsiStable();
	if (HSIStartUpStatus == SUCCESS)
	{
	}
	else
	{
		/* If HSI fails to start-up, the application will have wrong clock configuration. User can add here some code to deal with this error*/
		/* Go to infinitel1oop*/
		while (1)
		{
		}
	}
	RCC_ConfigAdc1mClk(RCC_ADC1MCLK_SRC_HSI, RCC_ADC1MCLK_DIV8);
	/* RCC_ADCHCLK_DIV16*/
	ADC_ConfigClk(ADC_CTRL3_CKMOD_AHB, RCC_ADCHCLK_DIV16);

	GPIO_InitStruct(&GPIO_InitStructure);
	/* Configure PA.01 as analog input -------------------------*/
	GPIO_InitStructure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_ANALOG;
	GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);

	/* ADC configuration ------------------------------------------------------*/
	ADC_InitStructure.MultiChEn = DISABLE;
	ADC_InitStructure.ContinueConvEn = DISABLE;
	ADC_InitStructure.ExtTrigSelect = ADC_EXT_TRIGCONV_NONE;
	ADC_InitStructure.DatAlign = ADC_DAT_ALIGN_R;
	ADC_InitStructure.ChsNumber = 1;
	ADC_Init(ADC, &ADC_InitStructure);

	/* Enable ADC */
	ADC_Enable(ADC, ENABLE);
	/* Check ADC Ready */
	while (ADC_GetFlagStatusNew(ADC, ADC_FLAG_RDY) == RESET)
		;
	while (ADC_GetFlagStatusNew(ADC, ADC_FLAG_PD_RDY))
		;
}

uint16_t ADC_GetData(uint8_t ADC_Channel)
{
	uint16_t dat;
	ADC_ConfigRegularChannel(ADC, ADC_Channel, 1, ADC_SAMP_TIME_56CYCLES5);
	/* Start ADC Software Conversion */
	ADC_EnableSoftwareStartConv(ADC, ENABLE);
	while (ADC_GetFlagStatus(ADC, ADC_FLAG_ENDC_ANY) == 0)
	{
	}
	ADC_ClearFlag(ADC, ADC_FLAG_ENDC_ANY);
	ADC_ClearFlag(ADC, ADC_FLAG_STR);
	dat = ADC_GetDat(ADC);
	return dat;
}
static uint16_t ADC_Average(u8 ADC_Channel)
{
	uint8_t i = 0;
	uint16_t adc_average = 0;
	uint32_t all = 0;

	for (i = 0; i < 8; i++)
		all += ADC_GetData(ADC_Channel);

	adc_average = (all >> 3);

	return adc_average;
}

void ADC_Scan(void)
{
	float temp1, temp2;
	uint16_t ADCConvertedValue[3];
	if (AdcScanTimeCount == 0)
	{
		AdcScanTimeCount = ADC_SCAN_INTERVAL; // 重装下一次采样间隔

		ADCConvertedValue[0] = ADC_Average(ADC_CH_1_PA1);
		temp1 = (float)ADCConvertedValue[0] * 3.3;
		temp2 = temp1 / 4096;
		NTC1_Value = temp2 * 1000;

		ADCConvertedValue[1] = ADC_Average(ADC_CH_0_PA0);
		temp1 = (float)ADCConvertedValue[1] * 3.3;
		temp2 = temp1 / 4096;
		NTC2_Value = temp2 * 1000;

		ADCConvertedValue[2] = ADC_Average(ADC_CH_5_PA5);
		temp1 = (float)ADCConvertedValue[2] * 3.3;
		temp2 = temp1 / 4096;
		BAT_Value = temp2 * 1000;

		Bat_Update(); // 电量分档与低电判定, 依赖本次的 BAT_Value
		LOG(0xFF, "NTC1:%d NTC2:%d BAT:%d", NTC1_Value, NTC2_Value, BAT_Value);
	}
}
