#include <stdarg.h>
#include <stdio.h>

/* Write a message with an identifier in front, as printf would
   write the rest. */
static int msg(const char *fmt, ...)
{
    va_list ap;
    int n;

    fputs("PAY001I ", stdout);
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

/* Add up count values of type long. */
static long sum(int count, ...)
{
    va_list ap;
    long total = 0;

    va_start(ap, count);
    while (count-- > 0)
        total += va_arg(ap, long);
    va_end(ap);
    return total;
}

int main(void)
{
    msg("%d records, total %ld\n", 3, sum(3, 10L, 20L, 12L));
    return 0;
}
