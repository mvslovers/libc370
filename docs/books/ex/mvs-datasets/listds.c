#include <stdio.h>
#include <errno.h>
#include <ext/array.h>
#include <mvs/dslist.h>

int main(int argc, char **argv)
{
    const char *level = argc > 1 ? argv[1] : "SYS1";
    DSLIST     **list;
    unsigned   i, n;

    list = __listds(level, "NONVSAM VOLUME", NULL);
    if (!list) {
        if (errno) {
            perror("__listds");
            return 8;
        }
        printf("no data sets under %s\n", level);
        return 4;
    }

    n = arraycount(&list);
    for (i = 0; i < n; i++) {
        DSLIST *ds = list[i];
        printf("%-44s %-6s %-2s %-4s %5u %5u\n", ds->dsn, ds->volser,
               ds->dsorg, ds->recfm, ds->lrecl, ds->blksize);
    }

    __freeds(&list);
    return 0;
}
