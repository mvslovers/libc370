#include <stdio.h>
#include "amount.h"

long amt_parse(const char *s)
{
    long units = 0, cents = 0;

    sscanf(s, "%ld.%ld", &units, &cents);   /* "12.34" */
    return units * 100 + (units < 0 ? -cents : cents);
}
