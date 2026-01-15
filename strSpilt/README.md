//Net Mcu
输入一串字符 如 "$TSTMODE,print,1,2,3.14,,Hello,world,*FC" "BDCOV,0,25,0,"
根据逗号进行分割,逗号可修改

## 20260115 
1. 增加无copy解析功能
2. paramsParsePara 能解析"10.2"成浮点数，但是返回是 unsigned long long ，需要 *(float*)&val 使用
3.  文件夹划分三个文件，三个功能

    | 文件名         | 功能项                                          |
    | -------------- | ----------------------------------------------- |
    | paramparse.c   | 解析传入ascii文本<br>浮点、十进制、十六进制均可 |
    | strSplit.c     | 主要实现my_strtok,分割字符                      |
    | cmd_dispatch.c | 实现命令回调函数功能                            |