#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    void *p = malloc(8 * 1024 * 1024);      /* more than malloc() allows */

    if (p == NULL) {
        printf("malloc: %s\n", strerror(errno));
        return 8;
    }
    free(p);
    return 0;
}
