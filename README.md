# c_test 简介
## printf
自定义实现类似`printf`功能，可单独移植，不依赖于库

## strSpilt
输入一串字符 如 `"$TSTMODE,print,1,2,3.14,,Hello,world,*FC" "BDCOV,0,25,0,"`
根据逗号进行分割,逗号可修改

## tick_cmp
利用u32溢出特性，实现了考虑溢出情况的tick_diff比较

## 字符单个输入匹配指定字符串
KMP算法，在一串字符串中匹配目标字符

## Ymodem2
实现Ymodem协议下载功能，存到ram空间，不依赖于文件系统

## uartRoute
串口字符实现路由功能，不同的uart字符流向不同串口

## memdis
实现xprintf 和 hex+ascii输出RAM数据，方便打印RAM内容并分析

## wear-level2
实现flash的均衡磨损算法，详细信息看目录下的README

## spiflash_loopwrite
增加flash ring 读写，通过全0xff 来定位最后一次写位置