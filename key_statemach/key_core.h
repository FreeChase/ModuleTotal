#ifndef __KEY_CORE_H__
#define __KEY_CORE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 定义按键事件
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_DOWN,         // 按下
    KEY_EVENT_UP,           // 抬起
    KEY_EVENT_CLICK,        // 短按（按下并释放）
    KEY_EVENT_LONG_PRESS,   // 长按触发
    // KEY_EVENT_DOUBLE_CLICK, // 双击（可选，看需求）
} KeyEvent_t;

// 前向声明
struct KeyHandle;

/**
 * @brief 读取引脚电平的函数指针类型
 * @param user_data 用户私有数据（通常用来传 GPIO 端口号等）
 * @return 1: 有效按下(Active), 0: 无效/释放
 */
typedef uint8_t (*pFuncReadPin)(void *user_data);

/**
 * @brief 事件回调函数指针类型
 * @param key 按键句柄
 * @param event 触发的事件
 */
typedef void (*pFuncKeyCallback)(struct KeyHandle *key, KeyEvent_t event);

// 按键对象结构体
typedef struct KeyHandle {
    // --- 配置参数 (初始化设置) ---
    uint16_t        debounce_ticks;  // 消抖时间 (单位: 扫描周期数)
    uint16_t        long_press_ticks;// 长按时间 (单位: 扫描周期数)
    
    // --- 硬件接口 (多态) ---
    pFuncReadPin    fn_read_pin;     // 读IO的回调
    pFuncKeyCallback fn_callback;    // 触发事件的回调
    void            *user_data;      // 用户私有数据 (保存 Port/Pin 或 ID)

    // --- 内部状态 (运行时) ---
    uint8_t         state;           // 状态机状态
    uint16_t        tick_count;      // 计时器
    uint8_t         last_level;      // 上一次电平状态
    uint8_t         event_triggered; // 标记长按是否已触发，防止重复触发

} KeyHandle_t;

/**
 * @brief 初始化按键
 * @param key 句柄指针
 * @param read_fn 读IO函数
 * @param cb_fn 事件回调函数
 * @param user_data 用户私有数据(如 GPIO_Pin)
 */
void Key_Init(KeyHandle_t *key, pFuncReadPin read_fn, pFuncKeyCallback cb_fn, void *user_data);

/**
 * @brief 按键处理核心函数，需周期性调用
 * @param key 句柄指针
 * @param cycle_ms 调用此函数的周期(ms)，用于计算时间，如果设为1则 ticks = ms
 */
void Key_Tick(KeyHandle_t *key, uint16_t cycle_ms);

#ifdef __cplusplus
}
#endif

#endif