#include <stdio.h>
#include <stddef.h> // 必须包含，用于 offsetof
#include <stdint.h>

/* ================= 定义数据类型枚举 ================= */
typedef enum {
    T_INT,
    T_FLOAT,
    T_HEX
} VarType_e;

/* ================= 定义 X-Macro ================= */
#define SYSTEM_PARAMS \
    X(T_INT,   int,       motor_id,    "%d")   \
    X(T_FLOAT, float,     pid_kp,      "%.3f") \
    X(T_FLOAT, float,     pid_ki,      "%.3f") \
    X(T_HEX,   uint32_t,  error_code,  "0x%08X")

/* ================= 1. 生成结构体 ================= */
typedef struct {
    #define X(tag, type, name, fmt) type name;
    SYSTEM_PARAMS
    #undef X
} Config_t;

/* ================= 2. 生成元数据表 (核心技巧) ================= */
// 定义一个结构体用来存“变量的身份信息”
typedef struct {
    const char* name;   // 变量名字符串
    uint32_t    offset; // 在结构体中的字节偏移量
    VarType_e   type;   // 变量类型
    const char* format; // 打印格式
} ParamMeta_t;

// 自动生成查找表
const ParamMeta_t ParamTable[] = {
    #define X(tag, type, name, fmt) \
        { #name, offsetof(Config_t, name), tag, fmt },
        
    SYSTEM_PARAMS
    #undef X
};

// 计算参数总数
#define PARAM_COUNT (sizeof(ParamTable) / sizeof(ParamTable[0]))

/* ================= 3. 实现“通过 Index 取值”函数 ================= */
void Print_Param_By_Index(Config_t *pCfg, int index) {
    if (index < 0 || index >= PARAM_COUNT) {
        printf("Index %d out of range!\n", index);
        return;
    }

    // A. 获取元数据
    const ParamMeta_t *meta = &ParamTable[index];

    // B. 计算变量的内存地址 = 结构体基地址 + 偏移量
    // 注意：必须先转为 (char*) 或 (uint8_t*) 才能做字节加法
    void *pAddr = (char*)pCfg + meta->offset;

    // C. 根据类型解析并打印
    printf("[%d] %-12s : ", index, meta->name);
    
    switch (meta->type) {
        case T_INT:
            // 强转为 int* 取值
            printf(meta->format, *(int*)pAddr); 
            break;
        case T_FLOAT:
            // 强转为 float* 取值
            printf(meta->format, *(float*)pAddr);
            break;
        case T_HEX:
            // 强转为 uint32_t* 取值
            printf(meta->format, *(uint32_t*)pAddr);
            break;
    }
    printf("\n");
}

/* ================= 测试 ================= */
int main() {
    Config_t myCfg = {
        .motor_id = 1,
        .pid_kp = 1.5f,
        .pid_ki = 0.02f,
        .error_code = 0xDEADBEEF
    };

    // 模拟：我想读取第 2 个参数 (pid_ki)
    Print_Param_By_Index(&myCfg, 2);
    
    // 模拟：遍历所有参数
    printf("--- Loop All ---\n");
    for(int i=0; i<PARAM_COUNT; i++) {
        Print_Param_By_Index(&myCfg, i);
    }

    return 0;
}