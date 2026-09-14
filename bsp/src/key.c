#include "key.h"
#include "main.h"

extern uint8_t Key, Fac;
extern __IO uint16_t IdleTimeCount;
__IO uint16_t KeyEnhancedTimeCount = 0xFFFF;
void Key_Init(void)
{    
	GPIO_InitType GPIO_InitStructure;    
	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_AFIO, ENABLE);        

	GPIO_InitStruct(&GPIO_InitStructure);       
	GPIO_InitStructure.Pin          = GPIO_PIN_11|GPIO_PIN_10;      
	GPIO_InitStructure.GPIO_Pull    = GPIO_PULL_UP;	
	GPIO_InitStructure.GPIO_Mode    = GPIO_MODE_INPUT;      
	GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);    
}

void Key_Scan(void)
{
		uint8_t cur = 0;
    const uint16_t mask = 0xFFF8;
    static uint8_t pre = 0, rec = 0;

    if (!GPIO_ReadInputDataBit(GPIOA, GPIO_PIN_11))
        cur |=KEY_POWER; // pulse

    if (cur != pre)
    {
        KeyEnhancedTimeCount = Key = Fac = IdleTimeCount = 0;
    } // unstable:reset counter
    else if (KeyEnhancedTimeCount > 10) // stable:low or high
    {
        rec = cur ? cur : rec;                                                      // default:short pressed
        rec = (cur && (KeyEnhancedTimeCount > 1200)) ? (cur | KEY_LONG_FLAG) : rec; // long pressed

        if ((!cur && rec && !((mask >> rec) & 0x0001)) || (cur && rec && ((mask >> rec) & 0x0001) && (rec != (Key & KEY_DATA_CODE))))
            Key = rec; // released rep | //pressed rep

        if (cur && (rec != (Fac & KEY_DATA_CODE)))
        {
            Fac = rec;
        } // pressed rep

        rec = cur ? rec : 0;
    }

    pre = cur;
	
}
