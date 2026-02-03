#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* =========================================================================
 * 1. 核心定义区域：这是你唯一需要维护的地方
 * 格式: X(类型, 变量名, 打印格式占位符)
 * ========================================================================= */
#define SYSTEM_PARAM_LIST \
    X(int,       device_id,     "%d")      \
    X(float,     voltage_cali,  "%.4f")    \
    X(uint32_t,  baud_rate,     "%u")      \
    X(uint32_t,  error_flags,   "0x%08X")  \
    X(int,       retry_count,   "%d")

/* =========================================================================
 * 2. 宏魔法阶段
 * ========================================================================= */

// --- 魔法 A: 自动生成结构体定义 ---
typedef struct {
    #define X(type, name, format) type name;
    SYSTEM_PARAM_LIST
    #undef X
} SystemParams_t;

// --- 魔法 B: 自动生成变量名字符串数组 (用于索引或查找) ---
const char* ParamNames[] = {
    #define X(type, name, format) #name,
    SYSTEM_PARAM_LIST
    #undef X
};

// --- 魔法 C: 自动生成打印/Dump 函数 ---
void Dump_All_Params(SystemParams_t *pParam) {
    printf("==========================================\n");
    printf("%-20s | %-10s\n", "Variable Name", "Value");
    printf("------------------------------------------\n");

    // 这里利用 format 参数，让每种类型用正确的格式打印
    #define X(type, name, format) \
        printf("%-20s | " format "\n", #name, pParam->name);
        
    SYSTEM_PARAM_LIST
    #undef X
    
    printf("==========================================\n");
}

/* =========================================================================
 * 3. 业务代码示例
 * ========================================================================= */
int main() {
    // 模拟从 Flash 读取数据或初始化
    SystemParams_t myParams = {
        .device_id    = 101,
        .voltage_cali = 3.3051f,
        .baud_rate    = 115200,
        .error_flags  = 0xDEAD0000,
        .retry_count  = 5
    };

    // 1. 打印所有参数 (不用手写 printf 了！)
    Dump_All_Params(&myParams);

    // 2. 演示如何获取参数个数 (通过计算数组大小)
    int param_count = sizeof(ParamNames) / sizeof(ParamNames[0]);
    printf("\nTotal Params: %d\n", param_count);

    // 3. 演示高级用法：根据索引获取变量名 (常用于命令行提示)
    // 假设你想让用户输入 "set baud_rate 9600"
    printf("Index 2 is param: [%s]\n", ParamNames[2]);

    return 0;
}