#include <stdio.h>
#include <mvs/dynalloc.h>

/* Create a cataloged work data set, write one record, and free it. */
int main(void)
{
    char    dd[9];
    char    name[12];
    FILE    *fp;
    int     rc;

    rc = __dsalcf(dd, "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;"
                      "RECFM=FB;LRECL=80;BLKSIZE=3120;"
                      "UNIT=SYSDA;SPACE=TRK(1,1)",
                  "IBMUSER.DEMO.DATA");
    if (rc) {
        printf("allocation failed, rc=%d\n", rc);
        return 8;
    }

    sprintf(name, "DD:%s", dd);         /* the DD name SVC 99 chose */
    fp = fopen(name, "w");
    if (fp) {
        fputs("HELLO FROM A DYNAMICALLY ALLOCATED DATA SET\n", fp);
        fclose(fp);
    }

    return __dsfree(dd) ? 4 : 0;        /* DISP=CATLG takes effect here */
}
