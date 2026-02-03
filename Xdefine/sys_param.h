#ifndef __SYS_PARAM_H__
#define __SYS_PARAM_H__

#include <stdint.h>

/* ============================================================
 * 1. 核心数据定义 (Single Source of Truth)
 * 所有修改都在这里进行，.c 和 struct 会自动同步
 * ============================================================ */
// 格式: X(类型枚举, C语言类型, 变量名, 打印格式)
#define SYSTEM_PARAMS \
    X(T_INT,    int,        motor_id,    "%d")      \
    X(T_FLOAT,  float,      pid_kp,      "%.3f")    \
    X(T_FLOAT,  float,      pid_ki,      "%.3f")    \
    X(T_HEX,    uint32_t,   error_code,  "0x%08X")

/* ============================================================
 * 2. 类型定义
 * ============================================================ */
typedef enum {
    T_INT,
    T_FLOAT,
    T_HEX
} VarType_e;

// 利用 X-Macro 自动生成结构体定义
typedef struct {
    #define X(tag, type, name, fmt) type name;
    SYSTEM_PARAMS
    #undef X
} Config_t;

/* ============================================================
 * 3. 接口函数声明
 * ============================================================ */

// 获取参数总数
int Param_Get_Count(void);

// 打印单个参数
void Param_Print_By_Index(Config_t *pCfg, int index);

// [新增] 设置参数接口 (通用指针方式)
// 返回 0 表示成功，-1 表示失败
int Param_Set_By_Index(Config_t *pCfg, int index, void *pValue);

// 打印所有参数 (Dump)
void Param_Dump_All(Config_t *pCfg);

#endif // __SYS_PARAM_H__