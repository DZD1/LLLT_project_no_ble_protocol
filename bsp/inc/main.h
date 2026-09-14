/*****************************************************************************
 * Copyright (c) 2019, Nations Technologies Inc.
 *
 * All rights reserved.
 * ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Nations' name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY NATIONS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL NATIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file main.h
 * @author Nations 
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2019, Nations Technologies Inc. All rights reserved.
 */
#ifndef __MAIN_H__
#define __MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "n32g031.h"


#define MASK_POWER                  0X0003
#define MASK_TREATMENT              0X0010
#define MASK_TREAT_PAUSE            0X0020
#define MASK_TOUCH                  0X0040
#define MASK_THERMAL                0X0080
#define MASK_FANWARE                0X0100
#define MASK_LTT                    0X0200
#define MASK_CHK                    0X0400
#define MASK_ALARM                  0X0800
#define MASK_BAT_LOW                0X1000
    
#define KEY_DONE_FLAG               0X80
#define KEY_DATA_CODE               0X7F
    
#define KEY_LONG_FLAG               0X08
    
#define KEY_POWER                   0X01//PHY


#define F_POWER_OFF                 (((ssw & MASK_POWER) != MASK_POWER))//0
#define F_POWER_ON                  (((ssw & MASK_POWER) == MASK_POWER))//1

#define  F_OUT_TREATMENT            (((ssw & MASK_TREATMENT) != MASK_TREATMENT))//0
#define  F_IN_TREATMENT             (((ssw & MASK_TREATMENT) == MASK_TREATMENT))//1

#define  F_OUT_TREAT_PAUSE          (((ssw & MASK_TREAT_PAUSE) != MASK_TREAT_PAUSE))//0
#define  F_IN_TREAT_PAUSE           (((ssw & MASK_TREAT_PAUSE) == MASK_TREAT_PAUSE))//1

#define F_OUT_TOUCH                 (((ssw & MASK_TOUCH) != MASK_TOUCH))//0
#define F_IN_TOUCH                  (((ssw & MASK_TOUCH) == MASK_TOUCH))//1

#define F_THERMAL_OK                (((ssw & MASK_THERMAL) != MASK_THERMAL))//0
#define F_THERMAL_ERR               (((ssw & MASK_THERMAL) == MASK_THERMAL))//1

#define F_FANWARE_OK                (((ssw & MASK_FANWARE) != MASK_FANWARE))//0
#define F_FANWARE_ERR               (((ssw & MASK_FANWARE) == MASK_FANWARE))//1

#define F_OUT_LTT                   (((ssw & MASK_LTT) != MASK_LTT))//0
#define F_IN_LTT                    (((ssw & MASK_LTT) == MASK_LTT))//1

#define F_OUT_CHK                   (((ssw & MASK_CHK) != MASK_CHK))//0
#define F_IN_CHK                    (((ssw & MASK_CHK) == MASK_CHK))//1

#define F_OUT_ALARM                 (((ssw & MASK_ALARM) != MASK_ALARM))//0
#define F_IN_ALARM                  (((ssw & MASK_ALARM) == MASK_ALARM))//1

#define F_BAT_OK                    (((ssw & MASK_BAT_LOW) != MASK_BAT_LOW))//0
#define F_BAT_LOW                   (((ssw & MASK_BAT_LOW) == MASK_BAT_LOW))//1

#define S_POWER_OFF                 (ssw &= ~MASK_POWER)//0
#define S_POWER_ON                  (ssw |= MASK_POWER)//1

#define S_OUT_TREATMENT             (ssw &= ~MASK_TREATMENT)//0
#define S_IN_TREATMENT              (ssw |= MASK_TREATMENT)//1

#define S_OUT_TREAT_PAUSE           (ssw &= ~MASK_TREAT_PAUSE)//0
#define S_IN_TREAT_PAUSE            (ssw |= MASK_TREAT_PAUSE)//1

#define S_OUT_TOUCH                 (ssw &= ~MASK_TOUCH)//0
#define S_IN_TOUCH                  (ssw |= MASK_TOUCH)//1

#define S_THERMAL_OK                (ssw &= ~MASK_THERMAL)//0
#define S_THERMAL_ERR               (ssw |= MASK_THERMAL)//1

#define S_FANWARE_OK                (ssw &= ~MASK_FANWARE)//0
#define S_FANWARE_ERR               (ssw |= MASK_FANWARE)//1

#define S_OUT_LTT                   (ssw &= ~MASK_LTT)//0
#define S_IN_LTT                    (ssw |= MASK_LTT)//1

#define S_OUT_CHK                   (ssw &= ~MASK_CHK)//0
#define S_IN_CHK                    (ssw |= MASK_CHK)//1

#define S_OUT_ALARM                 (ssw &= ~MASK_ALARM)//0
#define S_IN_ALARM                  (ssw |= MASK_ALARM)//1

#define S_BAT_OK                    (ssw &= ~MASK_BAT_LOW)//0
#define S_BAT_LOW                   (ssw |= MASK_BAT_LOW)//1

typedef enum
{    
    DEBUG = 0,
    RELEASE = 1
} RelaseDebugMode_TypeDef;

#define LOG_CFG_MASK    0xFF
#define xxxx 		    do {if(LOG_CFG_MASK)printf("[X.X.X.X] in File:%s, LINE:%d, Function:%s. \r\n",__FILE__, __LINE__, __func__);}while(0)
#define LOG(fmt, ...)   do {if(fmt & LOG_CFG_MASK){printf("LOG in FUNC:%s, LINE:%d.",__func__,__LINE__);printf(##__VA_ARGS__);}}while(0)

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */
/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
