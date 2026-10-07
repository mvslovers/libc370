#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *msg;
    void *p = malloc(8 * 1024 * 1024);      /* more than malloc() allows */

    if (p == NULL) {
        msg = strerror(errno);
        printf("malloc: %s\n", msg ? msg : "unknown error");
        return 8;
    }
    free(p);
    return 0;
}
