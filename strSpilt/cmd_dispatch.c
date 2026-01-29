#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "paramparse.h"

// 1. 定义回调函数指针类型
typedef int (*CmdCallback)(int argc, char *argv[]);

// 2. 定义命令表项结构体 (增加 helpInfo 字段)
typedef struct 
{
    const char *cmdName; 
    CmdCallback handler; 
    const char *helpInfo; // [新增] 命令说明信息
} CmdEntry_t;

// 前向声明
static const CmdEntry_t g_CmdTable[];

// ---------------------------------------------------------
// 3. 业务回调函数
// ---------------------------------------------------------

// [新增] Help 命令实现
int Cmd_Handle_Help(int argc, char *argv[])
{
    printf("\r\n=== Supported Commands ===\r\n");
    
    // 指针指向表头
    const CmdEntry_t *pEntry = g_CmdTable;

    // 遍历直到遇到 NULL 结尾
    while (pEntry->cmdName != NULL)
    {
        // 格式化打印：%-10s 表示左对齐占10个字符宽度，使输出对齐美观
        printf("  %-10s : %s\r\n", pEntry->cmdName, pEntry->helpInfo);
        pEntry++;
    }
    
    printf("==========================\r\n");
    return 0;
}

int Cmd_Handle_Time(int argc, char *argv[])
{
    printf("[CMD] Execute Time Command.\r\n");
    if (argc > 2)
    {
        unsigned long long rawVal = paramsParsePara(argv[2]);
        float timeVal;
        if (strchr(argv[2], '.')) {
             unsigned int temp = (unsigned int)rawVal;
             timeVal = *((float*)&temp);
        } else {
             timeVal = (float)rawVal;
        }
        printf("      Set timeVal to: %f\r\n", timeVal);
    }
    else
    {
        printf("      Error: Usage: time [value]\r\n");
    }
    return 0;
}

int Cmd_Handle_Print(int argc, char *argv[])
{
    printf("[CMD] Execute Print Command.\r\n");
    for(int i=0; i<argc; i++)
    {
        printf("      Arg[%d]: %s\r\n", i, argv[i]);
    }
    return 0;
}

// ---------------------------------------------------------
// 4. 定义命令查找表 (在此处注册新命令)
// ---------------------------------------------------------
static const CmdEntry_t g_CmdTable[] = 
{
    // 命令名      回调函数           帮助说明
    {"help",      Cmd_Handle_Help,   "Print all commands"},
    {"?",         Cmd_Handle_Help,   "Alias for help"},
    {"time",      Cmd_Handle_Time,   "Set system time (Usage: time 10.5)"},
    {"print",     Cmd_Handle_Print,  "Print arguments (Debug)"},
    
    // 哨兵，必须保留在最后
    {NULL,        NULL,              NULL} 
};

// ---------------------------------------------------------
// 5. 命令分发器
// ---------------------------------------------------------
int ProcessCommand(char *cmdStr, int argc, char *argv[])
{
    const CmdEntry_t *pEntry = g_CmdTable;

    if (cmdStr == NULL) return -1;

    while (pEntry->cmdName != NULL)
    {
        // 使用 strcmp 进行匹配
        if (strcmp(pEntry->cmdName, cmdStr) == 0)
        {
            if (pEntry->handler != NULL)
            {
                return pEntry->handler(argc, argv);
            }
        }
        pEntry++;
    }

    printf("[CMD] Unknown command: '%s'. Type 'help' for list.\r\n", cmdStr);
    return -1;
}

void DispatchFromParsedData(T_ParamsArg * ptparamsinfo)
{
    if (ptparamsinfo == NULL) return;

    // 假设协议格式: $HEAD, CMD, ARG1...
    // argv[0] 是帧头(如 $ANTI)，argv[1] 是命令字
    char *targetCmd = ptparamsinfo->argv[1]; 

    ProcessCommand(targetCmd, ptparamsinfo->argc, ptparamsinfo->argv);
}