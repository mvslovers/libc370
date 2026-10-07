#include <stdio.h>
#include "amount.h"
#include "myutil.h"

int main(void)
{
    static const unsigned char price[] = { 0x00, 0x19, 0x99, 0x9C };
    char buf[32];

    printf("total %s\n", amt_format(buf, sizeof buf, pd2int(price, 4)));
    return 0;
}
