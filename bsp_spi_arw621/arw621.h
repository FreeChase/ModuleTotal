#ifndef __ARW621_H
#define __ARW621_H

#include <stdint.h>
#include "xgpiops.h"   
#include "bsp_spi_soft_arw621.h" // 引入底层 SPI 组件

// ARW621 专属的 LE (锁存) 引脚
#define ARW_LE_PIN    54

#define ARW_LE_0()    XGpioPs_WritePin(&GpioInstance, ARW_LE_PIN, 0)
#define ARW_LE_1()    XGpioPs_WritePin(&GpioInstance, ARW_LE_PIN, 1)

// 功能声明
void ARW621_Init(void);
void ARW621_SetAttenuation(float db);

#endif