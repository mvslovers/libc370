#include <stdio.h>
#include <mvs/dslist.h>

/* List the load modules of a library with their attributes. */
static int show(void *arg, const PDSLIST *entry)
{
    LOADSTAT st;

    /* the entry is only valid during the call: format it now */
    if (__fmtloa((PDSLIST *) entry, &st) == 0)
        printf("%-8s %-8s size %s ac %s %s\n", st.name,
               st.aliasof[0] ? st.aliasof : "", st.size, st.ac, st.attr);
    ++*(unsigned *) arg;
    return 0;                           /* go on */
}

int main(void)
{
    unsigned count = 0;
    int      rc;

    rc = __walkpd("DD:LOADLIB", NULL, show, &count);
    if (rc < 0) {
        perror("LOADLIB");
        return 8;
    }
    printf("%u member(s)\n", count);
    return 0;
}
