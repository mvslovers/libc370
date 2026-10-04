#include <stdio.h>
#include <stdlib.h>
#include "amount.h"

char *amt_format(char *buf, size_t size, long cents)
{
    snprintf(buf, size, "%ld.%02ld", cents / 100, labs(cents % 100));
    return buf;
}
