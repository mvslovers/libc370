#include <stdio.h>
#include <stdlib.h>
#include "amount.h"

/* Two functions in one source: a program that calls one of them  */
/* gets both. See the text.                                       */

char *amt_format(char *buf, size_t size, long cents)
{
    snprintf(buf, size, "%ld.%02ld", cents / 100, labs(cents % 100));
    return buf;
}

long amt_parse(const char *s)
{
    long units = 0, cents = 0;

    sscanf(s, "%ld.%ld", &units, &cents);   /* "12.34" */
    return units * 100 + (units < 0 ? -cents : cents);
}
