/*
 * tsts42.c - libc370 #538: out of space at 16 extents is ENOSPC, not EIO.
 *
 * IFG0554T calls the x37 exit (#176) only while the DEB holds fewer than
 * 16 extents.  At 16 it skips the exit and drives SYNAD itself, with X'42'
 * ("put error, no space on volume") in the high byte of R1.  Before #538
 * the SYNAD stub recorded that as an I/O error, so the write failed with
 * EIO; now the stub reads the X'42' and the write fails with ENOSPC.
 *
 *   (a) D37 control: SPACE=TRK(1,0), no secondary.  The exit runs, errno
 *       is ENOSPC - before and after #538.
 *   (b) the case: SPACE=TRK(1,1).  Every overflow extends by a track until
 *       the 16th extent, then IFG0554T skips the exit.  errno is EIO
 *       before #538 (RED) and ENOSPC after it (GREEN).
 *
 * NO try(): if anything abends, the step abends and the job log says so.
 * Every line also goes to the console.
 *
 * PARM='<hlq>'   (default IBMUSER.TSTS42) - the probe creates <hlq>.D37
 * and <hlq>.X16 and deletes them; neither may exist when it starts.
 *
 * RC 0 = every check passed, 8 = a check failed.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tsts42.c -o TSTS42 -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror test/mvs/tsts42.c \
 *                -o TSTS42R -flinker-output=iebcopy     (red: installed libc)
 *          ld370 --pack TSTS42=TSTS42.iebcopy TSTS42R=TSTS42R.iebcopy \
 *                -o tsts42 -xmit --dsn IBMUSER.LIBC370.S42SCR
 * Upload:  tsts42.xmit to IBMUSER.LIBC370.S42XMIT (FB 80, binary)
 * Install: jcl/recvs42.jcl.   Run: jcl/tsts42.jcl.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/dynalloc.h>

#define ATTRS   "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(%s)"

#define LIMIT   200000          /* 16 tracks hold a few thousand records    */

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTS42 %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* fill dsn until a write fails; returns errno, or -1 if it cannot measure */
static int fill(const char *dsn, const char *space, long *written)
{
    char    dd[9];
    char    fname[52];
    char    rec[80];
    FILE    *fp;
    int     eno;
    int     rc;

    *written = 0;
    rc = __dsalcf(dd, ATTRS, dsn, space);
    if (rc) {
        printf("      __dsalcf(%s) rc=%d - CANNOT MEASURE\n", dsn, rc);
        wtof("TSTS42 SETUP FAILED __dsalcf %s rc=%d", dsn, rc);
        return -1;
    }
    __dsfree(dd);

    sprintf(fname, "'%s'", dsn);
    fp = fopen(fname, "wb");
    if (!fp) {
        printf("      fopen(%s) returned NULL - CANNOT MEASURE\n", fname);
        wtof("TSTS42 SETUP FAILED fopen %s", dsn);
        remove(dsn);
        return -1;
    }

    memset(rec, 'X', sizeof(rec));
    errno = 0;
    while (*written < LIMIT) {
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) break;
        (*written)++;
    }
    eno = (*written < LIMIT) ? errno : 0;
    fclose(fp);

    printf("      %s SPACE=TRK(%s): %ld record(s), errno=%d (%s)\n",
           dsn, space, *written, eno,
           eno == ENOSPC ? "ENOSPC" : eno == EIO ? "EIO" : "?");
    wtof("TSTS42 %s TRK(%s) wrote=%ld errno=%d", dsn, space, *written, eno);

    rc = remove(dsn);
    if (rc) {
        printf("      remove(%s) rc=%d\n", dsn, rc);
        wtof("TSTS42 remove %s rc=%d", dsn, rc);
    }
    return eno;
}

int main(int argc, char **argv)
{
    char    hlq[36];
    char    dsn[48];
    long    n1, n2;
    int     e1, e2;

    strcpy(hlq, (argc > 1 && argv[1][0]) ? argv[1] : "IBMUSER.TSTS42");

    printf("=== tsts42: #538 - 16 extents are ENOSPC, not EIO ===\n");

    sprintf(dsn, "%s.D37", hlq);
    e1 = fill(dsn, "1,0", &n1);
    sprintf(dsn, "%s.X16", hlq);
    e2 = fill(dsn, "1,1", &n2);

    check(e1 == ENOSPC, "(a) no secondary (D37, exit taken): ENOSPC");
    check(n2 > n1 * 8,  "(b) the secondary extended the data set");
    check(e2 == ENOSPC, "(b) 16 extents (exit skipped): ENOSPC, not EIO");

    printf("=== tsts42: %d check(s) failed ===\n", bad);
    wtof("TSTS42 VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
