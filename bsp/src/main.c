#include "main.h"
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "sys.h"
#include "gpio.h"
#include "pwm.h"
#include "key.h"
#include "timer.h"
#include "usart.h"
#include "user.h"
#include "ADC.h"
#include "delay.h"
#include "nec.h"

#define SYSTICK_1MS ((uint32_t)1000)
uint8_t Key = 0, Fac = 0, Mode = 0;
uint16_t ssw = 0;
extern uint16_t NTC1_Value, NTC2_Value, BAT_Value;
__IO uint16_t IdleTimeCount = 0;
__IO uint16_t OneSecondTimeCount = 1000; // default:1000ms
__IO uint16_t FaultLedAlarmTimeCount = 0xFFFF;
__IO uint16_t LTTDelayTime = 10000; // default:10s
__IO uint32_t DelayTimeCount = 0 ;

uint16_t TreatmentCnt = 2;
uint16_t TreatmentTiming = 300;
extern uint16_t NTC1_Value, NTC2_Value, BAT_Value;
int main()
{
  GPIO_Init();
  delay_init();
  Beep_init(369,47);
  Key_Init();
  TIMER_Init();
  Usart_Init();
  Adc_Init();
	NEC_Init();
	NEC_RxInit();
  VCSEL_init(67 - 1, 48000 - 1);
  while (1)
  {
    BAT_VLT_ON;
		ADC_Scan();
    Key_Scan();
    App_Handle();	//含NEC持续发码调度,不阻塞
  }
}

/**
 * @brief Assert failed function by user.
 * @param file The name of the call that failed.
 * @param line The source line number of the call that failed.
 */
#ifdef USE_FULL_ASSERT
void assert_failed(const uint8_t *expr, const uint8_t *file, uint32_t line)
{
  while (1)
  {
  }
}
#endif // USE_FULL_ASSERT
