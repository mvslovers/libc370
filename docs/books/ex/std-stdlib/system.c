#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /* attach IEFBR14 as a subtask, passing it no parameter */
    unsigned rc = (unsigned)system("IEFBR14");

    if (rc & 0x80000000u)
        printf("IEFBR14 abended, completion code %06X\n",
               rc & 0x00FFFFFFu);
    else
        printf("IEFBR14 ended with return code %u\n", rc);
    return 0;
}
