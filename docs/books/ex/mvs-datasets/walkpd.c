#include <stdio.h>
#include <mvs/dslist.h>

static int show(void *arg, const PDSLIST *entry)
{
    ISPFSTAT st;

    if (__fmtisp((PDSLIST *) entry, &st) == 0 && st.ver[0])
        printf("%-8s %s %s %5s %s\n", st.name, st.ver, st.changed,
               st.size, st.userid);
    else
        printf("%-8s (no statistics)\n", st.name);

    return ++*(int *) arg >= 50;        /* stop after 50 members */
}

int main(void)
{
    int shown = 0;
    int rc;

    rc = __walkpd("SYS1.PARMLIB", "IEA*", show, &shown);
    if (rc < 0) {
        perror("__walkpd");
        return 8;
    }
    printf("%d member(s)\n", rc);
    return 0;
}
