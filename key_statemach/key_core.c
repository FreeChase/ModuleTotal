#include "key_core.h"
#include <stddef.h> // for NULL

// 内部状态机状态
typedef enum {
    STATE_IDLE = 0,
    STATE_DEBOUNCE,
    STATE_PRESSED,
    STATE_LONG_PRESS_HOLD
} InternalState_t;

void Key_Init(KeyHandle_t *key, pFuncReadPin read_fn, pFuncKeyCallback cb_fn, void *user_data)
{
    key->fn_read_pin = read_fn;
    key->fn_callback = cb_fn;
    key->user_data = user_data;
    
    // 默认参数 (假设 Tick 周期 10ms)
    key->debounce_ticks = 2;     // 2 * 10ms = 20ms
    key->long_press_ticks = 150; // 150 * 10ms = 1.5s
    
    key->state = STATE_IDLE;
    key->tick_count = 0;
    key->last_level = 0;
    key->event_triggered = 0;
}

void Key_Tick(KeyHandle_t *key, uint8_t cycle_ms)
{
    if (!key->fn_read_pin) return;

    // 读取当前物理电平 (1表示按下/有效)
    uint8_t active = key->fn_read_pin(key->user_data);

    switch (key->state)
    {
    case STATE_IDLE:
        if (active) {
            key->state = STATE_DEBOUNCE;
            key->tick_count = 0;
        }
        break;

    case STATE_DEBOUNCE:
        if (active) {
            key->tick_count++;
            if (key->tick_count >= key->debounce_ticks) {
                // 消抖通过，确认按下
                key->state = STATE_PRESSED;
                key->tick_count = 0; // 重置计数用于长按
                key->event_triggered = 0;
                if(key->fn_callback) key->fn_callback(key, KEY_EVENT_DOWN);
            }
        } else {
            // 抖动，回到空闲
            key->state = STATE_IDLE;
        }
        break;

    case STATE_PRESSED:
        if (active) {
            // 持续按下，计算长按
            key->tick_count++;
            if (key->tick_count >= key->long_press_ticks) {
                if (!key->event_triggered) {
                    key->state = STATE_LONG_PRESS_HOLD;
                    key->event_triggered = 1;
                    if(key->fn_callback) key->fn_callback(key, KEY_EVENT_LONG_PRESS);
                }
            }
        } else {
            // 松开，触发 CLICK
            key->state = STATE_IDLE;
            if(key->fn_callback) {
                key->fn_callback(key, KEY_EVENT_CLICK);
                key->fn_callback(key, KEY_EVENT_UP);
            }
        }
        break;

    case STATE_LONG_PRESS_HOLD:
        if (!active) {
            // 长按后松开
            key->state = STATE_IDLE;
            if(key->fn_callback) key->fn_callback(key, KEY_EVENT_UP);
        }
        break;
        
    default:
        key->state = STATE_IDLE;
        break;
    }
}