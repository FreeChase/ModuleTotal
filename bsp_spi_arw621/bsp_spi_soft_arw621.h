#ifndef __BSP_SPI_SOFT_ARW621_H
#define __BSP_SPI_SOFT_ARW621_H

#include <stdint.h>
#include "xgpiops.h"
#include "sleep.h"

extern XGpioPs GpioInstance; 

// ==========================================
// 1. 硬件引脚映射 (加上 ARW_ 前缀防冲突)
// ==========================================
#define ARW_SPI_SCK_PIN   55
#define ARW_SPI_MOSI_PIN  56

#define ARW_SPI_SCK_0()   XGpioPs_WritePin(&GpioInstance, ARW_SPI_SCK_PIN, 0)
#define ARW_SPI_SCK_1()   XGpioPs_WritePin(&GpioInstance, ARW_SPI_SCK_PIN, 1)

#define ARW_SPI_MOSI_0()  XGpioPs_WritePin(&GpioInstance, ARW_SPI_MOSI_PIN, 0)
#define ARW_SPI_MOSI_1()  XGpioPs_WritePin(&GpioInstance, ARW_SPI_MOSI_PIN, 1)

#define ARW_SPI_DELAY()   usleep(1)

// ==========================================
// 2. 标准 SPI 发送时序 (LSB First, Mode 0)
// ==========================================
// 函数名也加上 ARW_ 前缀，防止链接器由于其他软 SPI 也有同名函数而报错
static inline void ARW_BSP_SPI_WriteByte_LSB(uint8_t data)
{
    for (int i = 0; i < 8; i++) 
    {
        ARW_SPI_SCK_0();
        
        if (data & 0x01) {
            ARW_SPI_MOSI_1();
        } else {
            ARW_SPI_MOSI_0();
        }
        ARW_SPI_DELAY(); 
        
        ARW_SPI_SCK_1();
        ARW_SPI_DELAY(); 
        
        data >>= 1; 
    }
    ARW_SPI_SCK_0(); 
}

#endif