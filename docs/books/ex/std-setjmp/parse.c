#include <setjmp.h>
#include <stdio.h>

static jmp_buf on_error;
static const char *pos;

static long number(void)
{
    long n = 0;

    if (*pos < '0' || *pos > '9')
        longjmp(on_error, 1);           /* back to parse()  */
    while (*pos >= '0' && *pos <= '9')
        n = n * 10 + (*pos++ - '0');
    return n;
}

static long sum(void)
{
    long total = number();

    while (*pos == '+') {
        pos++;
        total += number();
    }
    return total;
}

static int parse(const char *text, long *result)
{
    pos = text;
    if (setjmp(on_error) != 0)
        return -1;                      /* came back by longjmp */
    *result = sum();
    return *pos == '\0' ? 0 : -1;
}

int main(void)
{
    long r;

    if (parse("12+30", &r) == 0)
        printf("12+30 = %ld\n", r);
    if (parse("12+x", &r) != 0)
        printf("12+x: syntax error at '%s'\n", pos);
    return 0;
}
