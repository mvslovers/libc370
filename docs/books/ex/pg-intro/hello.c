#include <stdio.h>
#include <ext/version.h>

int main(void)
{
    /* record which library this module was linked with */
    printf("HELLO running with %s\n", libc370_version());
    puts("Hello, MVS");
    return 0;
}
