#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "paramparse.h"

// 1. 定义回调函数指针类型
typedef int (*CmdCallback)(int argc, char *argv[]);

// 2. 定义命令表项结构体
typedef struct 
{
    const char *cmdName; 
    CmdCallback handler; 
} CmdEntry_t;

// ---------------------------------------------------------
// 3. 业务回调函数 (集成 paramparse)
// ---------------------------------------------------------

int Cmd_Handle_Time(int argc, char *argv[])
{
    // argv[0]="$ANTI", argv[1]="time", argv[2]="10"
    printf("[CMD] Execute Time Command.\r\n");
    
    if (argc > 2)
    {
        unsigned long long rawVal = paramsParsePara(argv[2]);
        float timeVal;
        
        // 简单的启发式判断：如果字符串带小数点，就认为是浮点位
        if (strchr(argv[2], '.')) {
             unsigned int temp = (unsigned int)rawVal;
             timeVal = *((float*)&temp); // 重新解释位
        } else {
             timeVal = (float)rawVal;
        }
        printf("      timeVal :%f .\r\n",timeVal);
    }
    else
    {
        printf("      Error: Missing parameter for time.\r\n");
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
// 4. 定义命令查找表 (加了 const，优化 RAM)
// ---------------------------------------------------------
static const CmdEntry_t g_CmdTable[] = 
{
    {"time",  Cmd_Handle_Time},
    {"print", Cmd_Handle_Print},
    {NULL,    NULL} 
};

// ---------------------------------------------------------
// 5. 命令分发器
// ---------------------------------------------------------
int ProcessCommand(char *cmdStr, int argc, char *argv[])
{
    // 指针指向 const 数据
    const CmdEntry_t *pEntry = g_CmdTable;

    if (cmdStr == NULL) return -1;

    while (pEntry->cmdName != NULL)
    {
        // 实际项目中建议用 strcasecmp 忽略大小写
        if (strcmp(pEntry->cmdName, cmdStr) == 0)
        {
            if (pEntry->handler != NULL)
            {
                return pEntry->handler(argc, argv);
            }
        }
        pEntry++;
    }

    printf("[CMD] Unknown command: %s\r\n", cmdStr);
    return -1;
}

void DispatchFromParsedData(T_ParamsArg * ptparamsinfo)
{
    if (ptparamsinfo == NULL || ptparamsinfo->argc < 2) return;

    // 假设协议格式: $HEAD, CMD, ARG1...
    // 所以命令字是 argv[1]
    char *targetCmd = ptparamsinfo->argv[1]; 

    ProcessCommand(targetCmd, ptparamsinfo->argc, ptparamsinfo->argv);
}