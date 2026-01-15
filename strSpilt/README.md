//Net Mcu
输入一串字符 如 "$TSTMODE,print,1,2,3.14,,Hello,world,*FC" "BDCOV,0,25,0,"
根据逗号进行分割,逗号可修改

## 20260115 
1. 增加无copy解析功能
2. paramsParsePara 能解析"10.2"成浮点数，但是返回是 unsigned long long ，需要 *(float*)&val 使用