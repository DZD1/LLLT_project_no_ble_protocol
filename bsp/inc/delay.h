#ifndef __DELAY_H
#define __DELAY_H

#include "n32g031.h"
#include <stdint.h>
#include <stdio.h>
void delay_init(void);
void delay_us(uint32_t nus);
void delay_ms(uint16_t nms);
#endif

