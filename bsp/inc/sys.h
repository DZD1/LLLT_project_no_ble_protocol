#ifndef __SYS_H
#define __SYS_H

#include "n32g031.h"


typedef struct
{
    u16 bit0 : 1;
    u16 bit1 : 1;
    u16 bit2 : 1;
    u16 bit3 : 1;
    u16 bit4 : 1;
    u16 bit5 : 1;
    u16 bit6 : 1;
    u16 bit7 : 1;
    u16 bit8 : 1;
    u16 bit9 : 1;
    u16 bit10 : 1;
    u16 bit11 : 1;
    u16 bit12 : 1;
    u16 bit13 : 1;
    u16 bit14 : 1;
    u16 bit15 : 1;
} Bits16_TypeDef;


#define PAout(n)  	(((Bits16_TypeDef*)(&(GPIOA->POD)))->bit##n)  
#define PAin(n)    	((GPIOA->POD &(1<<(n))) >> n)

#define PBout(n)   	(((Bits16_TypeDef*)(&(GPIOB->POD)))->bit##n)  
#define PBin(n)    	((GPIOB->POD &(1<<(n))) >> n)

#define PCout(n)   	(((Bits16_TypeDef*)(&(GPIOC->POD)))->bit##n)  
#define PCin(n)    	((GPIOC->POD &(1<<(n))) >> n)

#define PFout(n)   	(((Bits16_TypeDef*)(&(GPIOF->POD)))->bit##n)  
#define PFin(n)    	((GPIOF->POD &(1<<(n))) >> n)
	

#endif 

