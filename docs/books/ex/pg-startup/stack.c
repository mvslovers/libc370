#include <stdio.h>
#include <string.h>

/* The start-up reads this word before main() is called:
   the C stack is 512 KB instead of 256 KB. */
unsigned __stklen = 512 * 1024;

static int fill(int depth)
{
    char block[32760];                  /* one record, on the stack */

    memset(block, '*', sizeof block);
    return depth > 0 ? fill(depth - 1) + block[0] - '*' : 0;
}

int main(void)
{
    /* eleven frames of 32 KB each: too deep for the default stack */
    printf("result %d\n", fill(10));
    return 0;
}
