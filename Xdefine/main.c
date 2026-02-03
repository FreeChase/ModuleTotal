#include <stdio.h>
#include "sys_param.h"

int main() {
    // 1. 初始化参数
    Config_t myCfg = {
        .motor_id = 101,
        .pid_kp = 1.5f,
        .pid_ki = 0.05f,
        .error_code = 0x00
    };

    printf("1. Initial Status:\n");
    Param_Dump_All(&myCfg);

    // 2. 修改参数测试
    printf("\n2. Modifying Params via Index...\n");

    // 修改 motor_id (Index 0, T_INT)
    int new_id = 888;
    Param_Set_By_Index(&myCfg, 0, &new_id);

    // 修改 pid_kp (Index 1, T_FLOAT)
    float new_kp = 99.9f;
    Param_Set_By_Index(&myCfg, 1, &new_kp);

    // 3. 打印验证
    printf("   (Verify modifications)\n");
    Param_Print_By_Index(&myCfg, 0); // Should be 888
    Param_Print_By_Index(&myCfg, 1); // Should be 99.900

    return 0;
}