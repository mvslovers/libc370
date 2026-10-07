#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* parse "SPACE=n" style numbers: decimal, octal with 0, hex with 0x */
static int number(const char *s, long *out)
{
    char *end;

    errno = 0;
    *out = strtol(s, &end, 0);
    if (end == s || *end != '\0')
        return -1;                      /* no digits, or trailing junk */
    if (errno == ERANGE)
        return -1;                      /* outside LONG_MIN..LONG_MAX */
    return 0;
}

int main(int argc, char **argv)
{
    long v;

    if (argc > 1 && number(argv[1], &v) == 0)
        printf("%ld\n", v);
    else
        printf("not a number\n");
    return 0;
}
