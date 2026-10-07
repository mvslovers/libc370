#include <stdio.h>
#include <mvs/mtt.h>

int main(void)
{
    CMTT     *cmtt = cmtt_new();
    MTENTRY  **list;
    unsigned i, n;

    if (!cmtt) return 8;
    list = cmtt_get_array(cmtt);
    n    = list ? array_count(&list) : 0;

    /* print the last 20 entries, oldest first */
    for (i = n > 20 ? n - 20 : 0; i < n; i++) {
        MTENTRY *e = list[i];
        printf("%.*s\n", (int)e->mtentlen, e->mtentdat);
    }
    cmtt_free(&cmtt);               /* frees the array as well */
    return 0;
}
