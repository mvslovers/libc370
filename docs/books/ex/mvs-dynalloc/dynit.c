#include <stdio.h>
#include <dynit.h>

/* Create a cataloged FB 80 data set, write one record to it through
   the DD name MVS generated, and free the DD again. */
int main(void)
{
    __dyn_t d;
    char    dd[12];
    FILE    *fp;
    int     rc;

    dyninit(&d);
    d.__dsname    = "IBMUSER.NEW.DATA";
    d.__status    = __DISP_NEW;
    d.__normdisp  = __DISP_CATLG;
    d.__conddisp  = __DISP_DELETE;
    d.__unit      = "SYSDA";
    d.__dsorg     = __DSORG_PS;
    d.__recfm     = _FB_;
    d.__lrecl     = 80;
    d.__blksize   = 3120;
    d.__alcunit   = __TRK;
    d.__primary   = 5;
    d.__secondary = 5;

    rc = dynalloc(&d);
    if (rc != 0) {
        printf("dynalloc rc=%d error=%04X info=%04X\n", rc,
               (unsigned short)d.__errcode, (unsigned short)d.__infocode);
        return 8;
    }

    snprintf(dd, sizeof(dd), "DD:%s", d.__retddn);    /* the name MVS chose */
    fp = fopen(dd, "w");
    if (fp) {
        fputs("FIRST RECORD\n", fp);
        fclose(fp);
    }

    dyninit(&d);                          /* now free it by name */
    d.__dsname = "IBMUSER.NEW.DATA";
    return dynfree(&d) == 0 ? 0 : 8;
}
