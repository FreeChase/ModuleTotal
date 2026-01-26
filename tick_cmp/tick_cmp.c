#include <stdint.h>
#include <stdio.h>

/**
 * @brief 计算两个时间戳之间的毫秒差，能正确处理u32溢出。
 *
 * @param new_time_ms 最新的时间戳。
 * @param old_time_ms 上次记录的时间戳。
 * @return 两个时间戳之间的毫秒差。
 */
uint32_t tick_diff(uint32_t new_time_ms, uint32_t old_time_ms) {
    // 利用无符号整数的模运算特性，自动处理溢出（回卷）情况。
    // 无论 new_time_ms 是否发生溢出，这个减法的结果都是正确的。
    return new_time_ms - old_time_ms;
}



int main(void)
{
    uint32_t time ;
    time = tick_diff(1,2);

    printf("diff time %d\r\n",time);
    return 0;
}