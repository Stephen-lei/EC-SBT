#include <stdio.h>

int *test(int a,int b)
{
    if(a==2)
    {
    return a+b;
    }else{
        return 0;
    }
}

int main() {
    printf("hello");
    int *c=test(2,3);
    return 0;
}