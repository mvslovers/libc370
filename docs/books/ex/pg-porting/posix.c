#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("pid %d\n", (int)getpid());
    return close(3);
}
