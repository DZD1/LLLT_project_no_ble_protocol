#ifndef __USER_H__
#define __USER_H__
#include "n32g031.h"

#define BLE_MODE_OFF        0x00
#define BLE_MODE_RED        0x01
#define BLE_MODE_BLUE       0x02
#define BLE_MODE_RED_BLUE   0x03
void App_Handle(void);

/* 最近一帧 NEC 的接收头采样结果: 1=收到载波, 0=没收到, 0xFF=还没发过帧。
   由 App_Handle() 每 108ms 刷新一次 */
extern uint8_t NecRxOk;

#endif /* __USER_H__ */


