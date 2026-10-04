#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char *dsn = getenv("INPUT");

    if (dsn == NULL) {
        fprintf(stderr, "INPUT is not set\n");
        return 8;
    }
    printf("reading %s\n", dsn);

    if (setenv("LEVEL", "TEST", 0) != 0)    /* only if not set already */
        fprintf(stderr, "setenv failed\n");
    printf("LEVEL=%s\n", getenv("LEVEL"));
    return 0;
}
