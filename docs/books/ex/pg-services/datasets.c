#include <stdio.h>
#include <errno.h>
#include <ext/array.h>
#include <mvs/dslist.h>

/* List the cataloged data sets under a level that match a pattern. */
int main(int argc, char **argv)
{
    const char *level  = argc > 1 ? argv[1] : "SYS1";
    const char *filter = argc > 2 ? argv[2] : NULL;
    DSLIST     **list;
    unsigned   i, n;

    list = __listds(level, "NONVSAM VOLUME", filter);
    if (!list) {
        if (errno) {                    /* a failure, not "none" */
            perror("__listds");
            return 8;
        }
        printf("no data sets under %s\n", level);
        return 4;
    }

    n = arraycount(&list);
    for (i = 0; i < n; i++) {
        DSLIST *ds = list[i];

        if (ds->dsorg[0])
            printf("%-44s %-6s %-2s %4u-%02u-%02u\n", ds->dsn, ds->volser,
                   ds->dsorg, ds->cryear, ds->crmon, ds->crday);
        else                            /* DSCB not read: volume offline */
            printf("%-44s %-6s (not mounted)\n", ds->dsn, ds->volser);
    }

    __freeds(&list);
    return 0;
}
