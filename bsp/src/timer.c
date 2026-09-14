#include "timer.h"
#include "gpio.h"
#include "main.h"
#include "ADC.h"
#include "nec.h"
extern __IO uint16_t KeyEnhancedTimeCount;
extern __IO uint16_t OneSecondTimeCount;

/* 低电闪烁半周期计数。声明在 timer.h, 由 user.c 的 LED1 驱动消费 */
__IO uint16_t BatBlinkTimeCount = BAT_BLINK_HALF_MS;
void TIMER_Init()
{
    TIM_TimeBaseInitType TIM_TimeBaseStructure;
    NVIC_InitType NVIC_InitStructure;

    /* PCLK1 = HCLK */
    RCC_ConfigPclk1(RCC_HCLK_DIV1);

    /* TIM6 clock enable */
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM6, ENABLE);

    /* Time base configuration */
    TIM_InitTimBaseStruct(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.Period = 1000 - 1;
    TIM_TimeBaseStructure.Prescaler = 48 - 1; // tick:1us Freq 48/48000000=1/1000000s=1us
    TIM_TimeBaseStructure.ClkDiv = 0;
    TIM_TimeBaseStructure.CntMode = TIM_CNT_MODE_UP;
    TIM_InitTimeBase(TIM6, &TIM_TimeBaseStructure);

    /* Enable the TIM2 global Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = LPTIM_TIM6_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* TIM6 enable update irq */
    TIM_ConfigInt(TIM6, TIM_INT_UPDATE, ENABLE);

    /* TIM6 enable counter */
    TIM_Enable(TIM6, ENABLE);
}

void LPTIM_TIM6_IRQHandler(void)
{
    if (TIM_GetIntStatus(TIM6, TIM_INT_UPDATE) != RESET)
    {
        TIM_ClearFlag(TIM6, TIM_INT_UPDATE);
        if (KeyEnhancedTimeCount < 0xFFFF)
            KeyEnhancedTimeCount++;
        if (OneSecondTimeCount > 0)
            OneSecondTimeCount--;
        if (AdcScanTimeCount > 0)
            AdcScanTimeCount--;         //减到0后保持,由ADC_Scan处理并重装
        if (NecGapTimeCount > 0)
            NecGapTimeCount--;          //减到0后保持,由NEC_Handle处理并重装
        if (BatBlinkTimeCount > 0)
            BatBlinkTimeCount--;        //减到0后保持,由LED1低电闪烁处理并重装
    }
}
