#include "sys_param.h"
#include <stdio.h>
#include <stddef.h> // 必须包含 offsetof
#include <string.h>

/* ============================================================
 * 1. 私有元数据定义
 * ============================================================ */
typedef struct {
    const char* name;   // 变量名
    uint32_t    offset; // 偏移量
    VarType_e   type;   // 类型标签
    const char* format; // 打印格式
} ParamMeta_t;

// 利用 X-Macro 自动生成查找表
// static 关键字限制在当前文件，避免符号污染
static const ParamMeta_t ParamTable[] = {
    #define X(tag, type, name, fmt) \
        { #name, offsetof(Config_t, name), tag, fmt },
        
    SYSTEM_PARAMS
    #undef X
};

#define PARAM_COUNT (sizeof(ParamTable) / sizeof(ParamTable[0]))

/* ============================================================
 * 2. 接口实现
 * ============================================================ */

int Param_Get_Count(void) {
    return PARAM_COUNT;
}

// 内部辅助函数：获取参数地址
static void* _get_param_addr(Config_t *pCfg, int index) {
    if (index < 0 || index >= PARAM_COUNT) return NULL;
    return (char*)pCfg + ParamTable[index].offset;
}

void Param_Print_By_Index(Config_t *pCfg, int index) {
    if (index < 0 || index >= PARAM_COUNT) {
        printf("[Error] Index %d out of range\n", index);
        return;
    }

    const ParamMeta_t *meta = &ParamTable[index];
    void *pAddr = _get_param_addr(pCfg, index);

    printf("  [%d] %-12s : ", index, meta->name);

    switch (meta->type) {
        case T_INT:   printf(meta->format, *(int*)pAddr);      break;
        case T_FLOAT: printf(meta->format, *(float*)pAddr);    break;
        case T_HEX:   printf(meta->format, *(uint32_t*)pAddr); break;
    }
    printf("\n");
}

/* * [新增功能] 参数设置
 * pValue: 指向要设置的数据的指针 (例如 &new_int_val)
 */
int Param_Set_By_Index(Config_t *pCfg, int index, void *pValue) {
    if (index < 0 || index >= PARAM_COUNT || pValue == NULL) {
        return -1; // Error
    }

    const ParamMeta_t *meta = &ParamTable[index];
    void *pAddr = _get_param_addr(pCfg, index);

    // 根据类型进行不同长度/类型的赋值
    switch (meta->type) {
        case T_INT:
            *(int*)pAddr = *(int*)pValue;
            break;
        case T_FLOAT:
            *(float*)pAddr = *(float*)pValue;
            break;
        case T_HEX:
            *(uint32_t*)pAddr = *(uint32_t*)pValue;
            break;
        default:
            return -1;
    }
    
    // 可选：设置成功后打印一条日志
    // printf("  -> Set %s success.\n", meta->name);
    return 0; // OK
}

void Param_Dump_All(Config_t *pCfg) {
    printf("--- System Parameters Dump ---\n");
    for (int i = 0; i < PARAM_COUNT; i++) {
        Param_Print_By_Index(pCfg, i);
    }
    printf("------------------------------\n");
}