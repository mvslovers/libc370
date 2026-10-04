#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Convert a number from the parameter, telling an overflow
   from a value that merely happens to be LONG_MAX. */
int main(int argc, char **argv)
{
    char *end;
    long n;

    if (argc < 2)
        return 4;
    errno = 0;
    n = strtol(argv[1], &end, 10);
    if (errno == ERANGE) {
        printf("%s: %s\n", argv[1], strerror(errno));
        return 8;
    }
    if (end == argv[1] || *end != '\0') {
        printf("%s: not a number\n", argv[1]);
        return 8;
    }
    printf("%ld\n", n);
    return 0;
}
