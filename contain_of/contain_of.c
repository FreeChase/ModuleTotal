#include "stdio.h"

typedef struct contain_of
{
    int a;
    int b;
    int c;
}TTEST;


int main(int argc,int * argv[])
{
    TTEST t1 ;

    printf("offset_c %d\r\n", (int)(&((TTEST*)0)->c) );
    return 0;
}