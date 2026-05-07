#include "arw621.h"

void ARW621_Init(void)
{

    //TODO 增加GPIO初始化动作
    //* 在 zynq下设置模式方向使能
    // 初始化外设空闲电平
    ARW_LE_0();
    SPI_SCK_0();
    SPI_MOSI_0();
}

void ARW621_SetAttenuation(float db)
{
    // 1. 数据边界保护
    if (db > 31.5f) db = 31.5f;
    if (db < 0.0f)  db = 0.0f;

    // 2. 物理量转为寄存器 Raw 值 (1步 = 0.5dB)
    uint8_t raw_step = (uint8_t)(db * 2.0f);

    // 3. 开始通讯前，确保 LE 处于低电平释放状态
    ARW_LE_0();
    SPI_DELAY();

    // 4. 调用标准的底层 SPI 接口发送数据包
    ARW_BSP_SPI_WriteByte_LSB(raw_step);

    // 5. 产生 ARW621 特有的 LE (Latch Enable) 高脉冲，将移位寄存器锁存
    SPI_DELAY();
    ARW_LE_1();
    usleep(2); // 保持一点脉宽，确保硬件读到上升沿
    ARW_LE_0();
}