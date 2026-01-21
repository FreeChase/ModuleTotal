#include <stdio.h>
#include <stdint.h>

#pragma pack(4)
typedef struct pack_test
{
    char a;
    short b;
    int c;
}T_T1;
#pragma pack()

int main(int argc,char * argv[])
{
    printf(" sizeof T1 %d\n",sizeof(T_T1));

    return 0;
}