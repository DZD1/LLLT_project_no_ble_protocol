#ifndef __PWM_H
#define __PWM_H

#include "n32g031.h"
#define BEEP_ON  TIM_SetCmp1(TIM3,50)
#define BEEP_OFF TIM_SetCmp1(TIM3,0)
#define VCSEL_ON  TIM_SetCmp2(TIM8,17)
#define VCSEL_OFF TIM_SetCmp2(TIM8,0)
void Beep_init(uint16_t Period,uint16_t PrescalerValue);
void VCSEL_init(uint16_t Period,uint16_t PrescalerValue);
void IRM_Init(uint16_t Period,uint16_t PrescalerValue);
#endif 


