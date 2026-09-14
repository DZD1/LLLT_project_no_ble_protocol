#include "pwm.h"
void Beep_init(uint16_t Period,uint16_t PrescalerValue)
{
    GPIO_InitType GPIO_InitStructure;  
    TIM_TimeBaseInitType TIM_TimeBaseStructure;
    OCInitType TIM_OCInitStructure;

    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3, ENABLE);   
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_AFIO,   ENABLE);

    GPIO_InitStruct(&GPIO_InitStructure);    
    //TIMER3 CH1
    GPIO_InitStructure.Pin        = GPIO_PIN_4;    
    GPIO_InitStructure.GPIO_Mode  = GPIO_MODE_AF_PP;   
    GPIO_InitStructure.GPIO_Current = GPIO_DC_LOW;    
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF2_TIM3;    
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);
  
    /* Time base configuration */
    TIM_InitTimBaseStruct(&TIM_TimeBaseStructure);   //先填默认值,避免局部结构体残留栈上随机值
    TIM_TimeBaseStructure.Period    = Period;
    TIM_TimeBaseStructure.Prescaler = PrescalerValue;
    TIM_TimeBaseStructure.ClkDiv    = 0;
    TIM_TimeBaseStructure.CntMode   = TIM_CNT_MODE_UP;
    TIM_InitTimeBase(TIM3, &TIM_TimeBaseStructure);
    /* PWM1 Mode configuration: Channel1 */   
    TIM_OCInitStructure.OcMode      = TIM_OCMODE_PWM2;    
    TIM_OCInitStructure.OutputState = TIM_OUTPUT_STATE_ENABLE;    
    TIM_OCInitStructure.Pulse       =0;   
    TIM_OCInitStructure.OcPolarity  = TIM_OC_POLARITY_LOW;    
    TIM_InitOc1(TIM3, &TIM_OCInitStructure);   
    TIM_ConfigOc1Preload(TIM3, TIM_OC_PRE_LOAD_ENABLE);

    TIM_ConfigArPreload(TIM3, ENABLE);      
    TIM_Enable(TIM3, ENABLE);
}

void VCSEL_init(uint16_t Period,uint16_t PrescalerValue)
{
    GPIO_InitType GPIO_InitStructure;  
    TIM_TimeBaseInitType TIM_TimeBaseStructure;
    OCInitType TIM_OCInitStructure;

    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_TIM8, ENABLE);   
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_AFIO,   ENABLE);

    GPIO_InitStruct(&GPIO_InitStructure);    
    //TIMER3 CH1
    GPIO_InitStructure.Pin        = GPIO_PIN_7;    
    GPIO_InitStructure.GPIO_Mode  = GPIO_MODE_AF_PP;   
    GPIO_InitStructure.GPIO_Current = GPIO_DC_LOW;    
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF1_TIM8;    
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);
  
    /* Time base configuration */
    /* 必须先填默认值:TIM_InitTimeBase()会把RepetCnt写进TIM8的REPCNT寄存器,
       残留随机值会让CCR2预装载延迟(REPCNT+1)*67ms才生效,导致VCSEL开关延迟数秒 */
    TIM_InitTimBaseStruct(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.Period    = Period;
    TIM_TimeBaseStructure.Prescaler = PrescalerValue;
    TIM_TimeBaseStructure.ClkDiv    = 0;
    TIM_TimeBaseStructure.CntMode   = TIM_CNT_MODE_UP;
    TIM_InitTimeBase(TIM8, &TIM_TimeBaseStructure);
    /* PWM1 Mode configuration: Channel1 */   
		TIM_InitOcStruct(&TIM_OCInitStructure);
    TIM_OCInitStructure.OcMode      = TIM_OCMODE_PWM2;    
    TIM_OCInitStructure.OutputState = TIM_OUTPUT_STATE_ENABLE; 
		TIM_OCInitStructure.OutputNState = TIM_OUTPUT_NSTATE_DISABLE;
		TIM_OCInitStructure.OcNPolarity = 	TIM_OC_POLARITY_HIGH;
		TIM_OCInitStructure.OcNIdleState = TIM_OCN_IDLE_STATE_RESET;
		TIM_OCInitStructure.OcIdleState = TIM_OC_IDLE_STATE_RESET;
    TIM_OCInitStructure.Pulse       = 0;   
    TIM_OCInitStructure.OcPolarity  = TIM_OC_POLARITY_LOW;    
    TIM_InitOc2(TIM8, &TIM_OCInitStructure);   
    TIM_ConfigOc2Preload(TIM8, TIM_OC_PRE_LOAD_ENABLE);

    TIM_ConfigArPreload(TIM8, ENABLE);   
    /* TIM3 enable counter */ 
		
    TIM_Enable(TIM8, ENABLE);
		TIM_EnableCtrlPwmOutputs(TIM8,ENABLE);
}

void IRM_Init(uint16_t Period,uint16_t PrescalerValue)
{
    GPIO_InitType GPIO_InitStructure;  
    TIM_TimeBaseInitType TIM_TimeBaseStructure;
    OCInitType TIM_OCInitStructure;

    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_TIM1, ENABLE);   
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_AFIO,   ENABLE);

    GPIO_InitStruct(&GPIO_InitStructure);    
    //TIMER3 CH1
    GPIO_InitStructure.Pin        = GPIO_PIN_8;    
    GPIO_InitStructure.GPIO_Mode  = GPIO_MODE_AF_PP;   
    GPIO_InitStructure.GPIO_Current = GPIO_DC_LOW;    
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF2_TIM1;    
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);
  
    /* Time base configuration */
    /* 必须先填默认值:RepetCnt会被写进TIM1的REPCNT寄存器,残留随机值会让
       update事件周期变成随机值,后续若启用update中断驱动NEC时序将完全错乱 */
    TIM_InitTimBaseStruct(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.Period    = Period;
    TIM_TimeBaseStructure.Prescaler = PrescalerValue;
    TIM_TimeBaseStructure.ClkDiv    = 0;
    TIM_TimeBaseStructure.CntMode   = TIM_CNT_MODE_UP;
    TIM_InitTimeBase(TIM1, &TIM_TimeBaseStructure);
    /* PWM1 Mode configuration: Channel1 */   
		TIM_InitOcStruct(&TIM_OCInitStructure);
    TIM_OCInitStructure.OcMode      = TIM_OCMODE_PWM2;    
    TIM_OCInitStructure.OutputState = TIM_OUTPUT_STATE_ENABLE; 
		TIM_OCInitStructure.OutputNState = TIM_OUTPUT_NSTATE_DISABLE;
		TIM_OCInitStructure.OcNPolarity = TIM_OCN_POLARITY_LOW;
		TIM_OCInitStructure.OcNIdleState = TIM_OCN_IDLE_STATE_RESET;
		TIM_OCInitStructure.OcIdleState = TIM_OC_IDLE_STATE_RESET;
    TIM_OCInitStructure.Pulse       = 0;   
    TIM_OCInitStructure.OcPolarity  = TIM_OC_POLARITY_LOW;    
    TIM_InitOc1(TIM1, &TIM_OCInitStructure);   
    TIM_ConfigOc1Preload(TIM1, TIM_OC_PRE_LOAD_ENABLE);

    TIM_ConfigArPreload(TIM1, ENABLE);   

		
    TIM_Enable(TIM1, ENABLE);
		TIM_EnableCtrlPwmOutputs(TIM1,ENABLE);
}

