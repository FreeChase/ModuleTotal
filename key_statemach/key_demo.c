

#include "key_core.h"
#include "stdio.h"
// --- 按键适配层开始 ---


#if 1
// 模拟事件
typedef enum {
    EVENT_NONE = 0,                 // 0
    EVENT_IDLE,                     // 1
    EVENT_TICK,                     // 2
    EVENT_CMD_LV1_SwitchtoInit,     // 3  * Power Up and Timeout ,Enter Normal Work
    EVENT_CMD_LV1_SwitchtoWork,     // 4
    EVENT_CMD_LV1_PeriodicRead,     // 5  * periodic read sensor data
    EVENT_CMD_LV1_ErrorDetect,      // 6  * function output
    EVENT_CMD_LV1_FunctionOutput,   // 7  * function output
    EVENT_CMD_LV1_CleanMode,        // 8
    EVENT_CMD_LV1_SwitchtoAlarm,    // 9
    EVENT_CMD_LV1_SwitchtoDeliver,  // 10

    // ------ Gap (11 ~ 511) ------

    EVENT_LV2_DS18B20READ   = 0x200,// 512 (0x200)
    EVENT_LV2_ATTRREAD,             // 513 (0x201)
    EVENT_LV2_FANREAD,              // 514 (0x202)
    EVENT_LV2_FUNCCHECK,            // 515 (0x203)
    EVENT_LV2_RETURN_WORK,          // 516 (0x204)
    EVENT_MAX,                      // 517 (0x205)
} EventType_t;
#endif

typedef struct {
    uint32_t *port;     // GPIO 端口
    uint16_t      pin;      // GPIO 引脚
    int           short_event_id; // 短按发送的事件ID
    int           long_event_id;  // 长按发送的事件ID (0表示无长按)
    int           up_event_id;  // 长按发送的事件ID (0表示无长按)
} KeyHwConfig_t;

// 定义按键句柄
KeyHandle_t hKeyClean;
// KeyHandle_t hKeyMode;
// KeyHandle_t hKeyAC;

// 定义按键的硬件配置 (根据 main.h 中的定义)
// 注意：根据你的 gpio.c，这些引脚是上拉输入，所以低电平有效
KeyHwConfig_t cfgClean = {(uint32_t*)0xffff,   0xffff, 0, EVENT_CMD_LV1_CleanMode, EVENT_CMD_LV1_SwitchtoWork};
// KeyHwConfig_t cfgMode  = {KEY_MODE_GPIO_Port,  KEY_MODE_Pin,  EVENT_CMD_LV1_ErrorDetect, EVENT_CMD_LV1_FunctionOutput};
// KeyHwConfig_t cfgAC    = {KEY_AC_GPIO_Port,    KEY_AC_Pin,    EVENT_CMD_LV1_SwitchtoWork, 0}; // 假设 AC 键切回工作模式

// --- 适配函数实现 ---

// 1. 告诉核心层如何读取你的 STM32 引脚
uint8_t Platform_ReadPin(void *user_data)
{
    KeyHwConfig_t *hw = (KeyHwConfig_t *)user_data;
    
    static uint16_t state_array[6] ={1,1,0,0,0,0};
    // 读取物理电平
    static uint16_t cnt =0;
    uint16_t state ;

    if(cnt<6)
    {
        state = state_array[cnt++];
    }
    else
    {
        state = 1;
    }
    
    // 因为是 GPIO_PULLUP (上拉)，所以：
    // 物理低电平 (RESET) = 按下 (逻辑 1)
    // 物理高电平 (SET)   = 松开 (逻辑 0)
    return (state == 0) ? 1 : 0;
}

// 2. 告诉核心层事件发生后如何通知你的系统
void Platform_KeyCallback(KeyHandle_t *key, KeyEvent_t event)
{
    KeyHwConfig_t *hw = (KeyHwConfig_t *)key->user_data;
    printf("Key Click11: Event %d\r\n", event);
    switch (event)
    {
    case KEY_EVENT_CLICK: // 短按
        if (hw->short_event_id != 0) {
            printf("Key Click: Event %d\r\n", hw->short_event_id);
            // Event_Put(hw->short_event_id); // 发送事件给你的状态机
        }
        break;

    case KEY_EVENT_LONG_PRESS: // 长按
        if (hw->long_event_id != 0) {
            printf("Key LongPress: Event %d\r\n", hw->long_event_id);
            // Event_Put(hw->long_event_id);
        }
        break;
    case KEY_EVENT_UP: // 抬起
        if (hw->up_event_id != 0) {
            printf("Key Up: Event %d\r\n", hw->up_event_id);
            // Event_Put(hw->up_event_id);
        }
        break;
        
    default:
        break;
    }
}
// --- 按键适配层结束 ---



void PreInit(void)
{



	// 初始化 Clean 键 (默认参数: 消抖20ms, 长按1s)
    Key_Init(&hKeyClean, Platform_ReadPin, Platform_KeyCallback, &cfgClean);


}

int main(int argc,char* argv[])
{
    PreInit();

    for (size_t i = 0; i < 10; i++)
    {
        Key_Tick(&hKeyClean, 500);
    }

    printf("Hello World\r\n");
    return 0;
}