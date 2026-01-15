#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stdint.h"
#include <stdio.h>
#include <string.h>
#include "strSplit.h"


uint8_t my_crc(uint8_t * pDat, uint32_t datLen)
{
    uint32_t i = 0;
    uint8_t crc = 0;

    if (pDat == NULL)
    {
        return 0;
    }

    for ( i = 0; i < datLen; i++)
    {
        crc ^= pDat[i];
    }
    
    return crc;
}

char *my_strtok_r(char *s, char * delim,char ** save_ptr)
{
    char * token;
    if(s == NULL) s = *save_ptr;

    s += strspn(s,delim);
    if(*s == '\0')
        return NULL;

    token = s;
    s = strpbrk(token,delim);
    if(s==NULL)
        *save_ptr=strchr(token,'\0');
    else
    {
        *s = '\0';
        *save_ptr = s + 1;
    }
    return token;
}

int isNumeric(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] != '-' && str[i] != '.' && (str[i] < '0' || str[i] > '9')) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief 优化后的解析函数 (零拷贝)
 * * @param data      输入的可变字符串 (会被 strtok 修改)
 * @param argv      输出的指针数组
 * @param max_args  argv 数组的最大容量
 * @return int      解析出的参数个数 argc
 */
int parse_args_zero_copy(char *data, T_ParamsArg * parg)
{
    char *psp;
    char *token;
    int argc = 0;
    char *checksum_ptr = NULL;

    // 1. 查找是否存在校验位分隔符 '*'
    //    如果有，我们需要确保 strtok 不会跨越它，或者在这里先处理它
    //    通常协议里 * 后面的不属于常规参数，这里简单处理：
    //    如果在字符串里发现 '*'，可以视为后续是校验和，不是参数
    
    token = my_strtok_r(data, ",", &psp);

    while (token != NULL && argc < sizeof(parg->argv)/sizeof(parg->argv[0]))
    {
        // 处理形如 "10*FC" 的情况
        char *star = strchr(token, '*');
        if (star != NULL)
        {
            *star = '\0'; // 将 '*' 替换为结束符，"10*FC" 变为 "10"
            // 如果你需要保留校验和字符串，可以用 checksum_ptr = star + 1;
        }

        // 只有非空字符串才算参数 (避免连续逗号产生空参数，视协议而定)
        if (strlen(token) > 0) 
        {
            parg->argv[argc++] = token; // 直接存储指针，不拷贝字符串
        }

        token = my_strtok_r(NULL, ",", &psp);
    }

    parg->argc = argc;
    return argc;
}
int parseDataProtocol(char *data , T_ParamsInfo * ptParamsInfo)
{
    char *psp;
    char *token = my_strtok_r(data, ",", &psp);
    // char pDst[50][50];
    int param_count = 0;

    if(ptParamsInfo == NULL)
    {
        //提示fail信息
        return -1;
    }

    while (token != NULL)
    {
        // strcpy(pDst[param_count], token);
        strcpy(ptParamsInfo->pdst + param_count*ptParamsInfo->itemUnitMax , token);
        param_count++;

        token = my_strtok_r(NULL, ",", &psp);
    }
//* 直接输出参数打印，方便调试
#if 0
    printf("参数: ");
    for (int i = 0; i < param_count; i++)
    {
        printf("\r\nindex %d ",i);
        if (isNumeric(ptParamsInfo->pdst+i*ptParamsInfo->itemUnitMax))
        {
            if (strchr(ptParamsInfo->pdst+i*ptParamsInfo->itemUnitMax, '.'))
            {
                printf(" f ");
                printf("%f ", atof(ptParamsInfo->pdst+i*ptParamsInfo->itemUnitMax));
            }
            else
            {
                printf(" i ");
                printf("%d ", atoi(ptParamsInfo->pdst+i*ptParamsInfo->itemUnitMax));
            }
        }
        else
        {
            printf(" s ");
            printf("%s ", ptParamsInfo->pdst+i*ptParamsInfo->itemUnitMax);
        }
    }
    printf("\n");
#endif 
    return param_count;
}

void DispatchFromParsedData(T_ParamsArg * ptparamsinfo );
int main() {

    T_ParamsArg argParams;


    char data5[] = "$ANTI,time,15*FC";
    parse_args_zero_copy(data5,&argParams);

    DispatchFromParsedData(&argParams);
    printf("argc : %d\n", argParams.argc);
    printf("argv 0 : %s\n", argParams.argv[0]);
    printf("argv 1 : %s\n", argParams.argv[1]);
    printf("argv 2 : %s\n", argParams.argv[2]);
    // printf("argv 3 : %s\n", argParams.argv[3]);


    return 0;
}
