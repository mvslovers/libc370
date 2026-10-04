#include <stdio.h>
#include <ctype.h>
#include "myutil.h"

static int count_digits(int c, void *arg)
{
    int *n = arg;

    if (isdigit(c))
        *n += 1;
    return 0;                    /* 0: go on with the next one    */
}

int main(void)
{
    static const unsigned char amount[] = { 0x01, 0x23, 0x4C };
    int digits = 0;

    printf("amount %d\n", pd2int(amount, sizeof amount));

    eachchr("MVS 3.8j, 1981", count_digits, &digits);
    printf("%d digits\n", digits);
    return 0;
}
