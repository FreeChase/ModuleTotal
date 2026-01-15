/**
 * @file shell_ext.h
 * @author Letter (NevermindZZT@gmail.com)
 * @brief shell extensions
 * @version 3.0.0
 * @date 2019-12-31
 * 
 * @copyright (c) 2019 Letter
 * 
 */

#ifndef __PARAMPARSE_H__
#define __PARAMPARSE_H__



/**
 * @brief 数字类型
 * 
 */
typedef enum
{
    NUM_TYPE_DEC,                                           /**< 十进制整型 */
    NUM_TYPE_BIN,                                           /**< 二进制整型 */
    NUM_TYPE_OCT,                                           /**< 八进制整型 */
    NUM_TYPE_HEX,                                           /**< 十六进制整型 */
    NUM_TYPE_FLOAT                                          /**< 浮点型 */
} ShellNumType;


typedef struct 
{
    int itemMax;        
    int itemUnitMax;
    char * pdst;
    int itemNumGet;
}T_ParamsInfo;


typedef struct 
{
    int argc;        
    char * argv[10];
}T_ParamsArg;

#endif
