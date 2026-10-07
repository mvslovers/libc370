#include <stdio.h>
#include <mvs/idcams.h>

/* Remember the first message that is not one of the two summaries. */
static void first(void *arg, int msgno, const char *text, int len)
{
    int *keep = arg;

    (void) text;
    (void) len;
    if (!*keep && msgno && msgno != 1 && msgno != 2)
        *keep = msgno;
}

int main(void)
{
    int msgno = 0;
    int rc;

    rc = idcams_sysprint(first, &msgno, " DELETE 'IBMUSER.DEMO.DATA'");
    if (rc)
        printf("DELETE failed: IDC%04dI, condition code %d\n", msgno, rc);
    return rc;
}
