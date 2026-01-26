#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h> // 仅用于 Sleep
#else
#include <unistd.h> // Linux下用 usleep
#endif
#include "key_core.h"

// 全局变量：模拟 GPIO 电平 (0:低/松开, 1:高/按下)
uint8_t simulated_gpio_level = 0;

// 1. [适配层] 读取模拟引脚
uint8_t Sim_ReadPin(void *user_data)
{
    // 直接返回全局变量的值
    return simulated_gpio_level;
}

// 2. [适配层] 事件回调
void Sim_KeyCallback(KeyHandle_t *key, KeyEvent_t event)
{
    const char* evtStr = "UNKNOWN";
    switch (event) {
        case KEY_EVENT_DOWN:       evtStr = "DOWN (Pressed)"; break;
        case KEY_EVENT_UP:         evtStr = "UP (Released)"; break;
        case KEY_EVENT_CLICK:      evtStr = ">>> CLICK (Short Press) <<<"; break;
        case KEY_EVENT_LONG_PRESS: evtStr = "!!! LONG PRESS DETECTED !!!"; break;
        default: break;
    }
    
    // 打印当前时间(tick)和事件
    printf("[Callback] State Machine Triggered: %s\n", evtStr);
}

// 辅助函数：跨平台延时 (ms)
void DelayMs(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

int main()
{
    KeyHandle_t hKey;
    uint32_t current_tick = 0; // 模拟系统时钟
    
    printf("--- Key State Machine Software Simulation ---\n");
    printf("Simulating ticks (1 tick = 10ms)...\n\n");

    // 初始化: 消抖 20ms (2 ticks), 长按 1000ms (100 ticks)
    Key_Init(&hKey, Sim_ReadPin, Sim_KeyCallback, NULL);
    hKey.debounce_ticks = 2;
    hKey.long_press_ticks = 100; 

    // 主循环：模拟时间流逝
    while (current_tick < 400) // 模拟 400 个周期 (4秒)
    {
        // --- 脚本化模拟 GPIO 变化 ---
        
        // 场景 1: 短按测试 (第 50 tick 按下, 第 60 tick 松开)
        if (current_tick == 50) {
            printf("\n[Script] User PRESSES button (Short)\n");
            simulated_gpio_level = 1;
        }
        if (current_tick == 60) {
            printf("[Script] User RELEASES button\n");
            simulated_gpio_level = 0;
        }

        // 场景 2: 长按测试 (第 150 tick 按下, 第 300 tick 松开)
        // 此时应该在按下 100 ticks 后 (即 tick=250+消抖) 触发长按事件
        if (current_tick == 150) {
            printf("\n[Script] User PRESSES button (Long)\n");
            simulated_gpio_level = 1;
        }
        if (current_tick == 300) {
            printf("[Script] User RELEASES button\n");
            simulated_gpio_level = 0;
        }

        // --- 核心逻辑 ---
        // 每次循环调用一次 Tick，假设周期为 10ms
        Key_Tick(&hKey, 10);
        
        current_tick++;
        
        // 为了看清输出，稍微延时一下 (实际MCU中不需要)
        DelayMs(10); 
    }
    
    printf("\n--- Simulation Finished ---\n");
    return 0;
}