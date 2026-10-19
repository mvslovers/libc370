/*
 * tstvsopm.c - libc370 #411 on MVS: vsopen() of a cluster an abend left
 * open, and of a DD name in lower case.
 *
 * __vsopen() closed the handle again and failed with errno 4 when OPEN
 * ended with R15 4 although the ACB was open - the warning OPEN gives for
 * a cluster an earlier program did not close.  A DD name in lower case
 * failed with EVSOPEN.  __vsclos() always returned 0.
 *
 * Run with PARM='ABEND' the program opens the ESDS for output, writes a
 * record and ends with S0C1, nothing recovering it, so the cluster stays
 * open for output.  Without a PARM it tests:
 *
 *   1 vsopen() of that cluster returns 0 and reads a record
 *   2 vsclose() returns 0
 *   3 vsopen("vsamesds", ...) with the DD name in lower case works
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstvsopm.c \
 *         -o $S/TSTVSOG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstvsopm.c \
 *         -o $S/TSTVSOR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTVSOG=TSTVSOG.iebcopy \
 *         TSTVSOR=TSTVSOR.iebcopy -o vo -xmit --dsn IBMUSER.LIBC370.VOSCR
 *
 * then jcl/recvvsopm.jcl and jcl/tstvsopm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/vsam.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTVSOPM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(int argc, char **argv)
{
    VSFILE  *vs = NULL;
    char    rec[80];
    int     rc, r;

    if (argc > 1 && strcmp(argv[1], "ABEND") == 0) {
        rc = vsopen("VSAMESDS", VSTYPE_ESDS, VSACCESS_SEQ, VSMODE_OUT, &vs);
        wtof("TSTVSOPM   abender: vsopen rc %d", rc);
        memset(rec, 'A', sizeof(rec));
        if (vs) vswrite(vs, rec, sizeof(rec), NULL, 0);
        __asm__("DC\tH'0'");        /* S0C1, the cluster stays open */
        return 0;
    }

    errno = 0;
    rc = vsopen("VSAMESDS", VSTYPE_ESDS, VSACCESS_SEQ, VSMODE_IN, &vs);
    wtof("TSTVSOPM   vsopen after the abend: rc %d, errno %d, vs %s",
         rc, errno, vs ? "set" : "NULL");
    want(rc == 0 && vs != NULL, "vsopen() of a cluster an abend left open");
    if (vs) {
        r = vsread(vs, rec, sizeof(rec), NULL, 0);
        wtof("TSTVSOPM   vsread %d", r);
        want(r == 80, "a record can be read");
        rc = vsclose(vs);
        want(rc == 0, "vsclose() returns 0");
        vs = NULL;
    }

    rc = vsopen("vsamesds", VSTYPE_ESDS, VSACCESS_SEQ, VSMODE_IN, &vs);
    wtof("TSTVSOPM   vsopen(\"vsamesds\"): rc %d", rc);
    want(rc == 0 && vs != NULL, "a DD name in lower case");
    if (vs) vsclose(vs);
    return bad ? 8 : 0;
}
