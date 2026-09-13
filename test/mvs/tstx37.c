/*
 * tstx37.c - libc370 #176: an out-of-space write is a return code, not an
 * ABEND.
 *
 * WHAT WAS WRONG.  An fwrite() that fills the last track of a data set with
 * no secondary allocation took ABEND SD37 in @@ATROUT's CHECK.  A caller
 * with an ESTAE could recover, but the abend unwound THROUGH @@fflush.c
 * before its reset: label, so fp->upto still pointed past the block that
 * failed and the next flush re-drove the identical WRITE.  Measured on
 * mvsdev JOB00235/241/245: that second attempt does not fail cleanly, it
 * PROGRAM-CHECKS - 0x0C4 on one run and 0x0C6 on another for identical
 * code.  And one path reaches it with no API call at all: @@exit.c walks
 * grt->grtfile at program termination and fclose()s every survivor with no
 * ESTAE around it.
 *
 * WHAT CHANGED.  @@AOPEN now plants an EXLST type X'08' exit.  IFG0554T -
 * the module named in the IEC031I D37-04 line this probe used to produce -
 * scans the exit list for it before it abends, and takes R15=1 as "rewrite
 * the format-1 DSCB, clear the unit exception bits, and return to the
 * access method to drive the caller's SYNAD with output error, no space
 * available".  libc370 has had that SYNAD stub since #147, so the condition
 * arrives as ferror() + errno, exactly the way an uncorrectable I/O error
 * already did - and __fflush() reaches its reset: label, so there is
 * nothing left for fclose() to re-drive.
 *
 * THIS PROBE USES NO try().  That is deliberate: if the exit is not planted,
 * is not taken, or returns the wrong code, the step ABENDS SD37 and the job
 * log carries IEC031I.  A green run is the absence of that line as much as
 * it is CC 0000.  Every check is also written to the console, because an
 * abend discards whatever SYSOUT is still sitting in the QSAM buffer.
 *
 * SHAPE.  mvslovers/ftpd's (ftpd#129): __dsalcf() creates the data set with
 * SPACE=TRK(1,0) - no secondary, so the first overflow is a D37 and not an
 * extend - __dsfree() catalogs it, and fopen(dsn,"wb") reopens it by name.
 *
 * CHECKS
 *   (1) fwrite() stops short of the limit         - the write did fail
 *   (2) ferror() is set on the FILE               - and it is visible
 *   (3) errno is ENOSPC, not EIO                  - and it says out of space
 *   (4) fclose() returns                          - nothing was re-driven
 *   (5) remove() answers 0                        - the DD went with it
 *
 * SETUP: none.  The work data set must NOT exist when the probe starts; it
 * is created and deleted here.
 *
 * PARM='<dsn>'   (default IBMUSER.TSTX37.WORK)
 *
 * UNIT=SYSDA below is the esoteric name on the reference system.
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING: without it -lc autocalls the
 * INSTALLED sysroot libc370, which has no x37 exit, and the probe abends for
 * a reason that has nothing to do with the target.  The cheap check is a
 * symbol only the new code carries:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstx37.c \
 *           -o TSTX37 -flinker-output=iebcopy
 *     python3 -c "print(open('TSTX37','rb').read().count( \
 *                       'X37EXIT'.encode('cp037')))"   # 0 - it is not an
 *                                                      # external symbol
 *
 * X37EXIT is internal to @@AOPEN, so grep the listing instead, or trust
 * `make build` plus the -L.  Then:
 *
 *     ld370 --pack TSTX37=TSTX37.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.X37SCR
 *
 * upload probe.xmit to IBMUSER.MBT.XMIT.IN, run jcl/recvx37.jcl, then
 * jcl/tstx37.jcl.
 *
 * MEASURED 2026-09-13 on mvsdev, JOB00247: CC 0000, 5/5 PASS, and - the
 * check no return code can make - NO IEC031I LINE IN THE JOB LOG AT ALL.
 * TRK(1,0) on WORK00 took the same 200 records of 80 that produced the D37
 * in JOB00245; this time fwrite() stopped at 200 with ferror() set and
 * errno 28 (ENOSPC), fclose() returned, and remove() answered 0.
 *
 * Cross-checked the same day by JOB00249: test/mvs/tstfabnd.c, whose three
 * steps each produced a D37 in JOB00245, now report try()=0x00000000 and
 * "nothing to measure" in all three - the same shape, no abend.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <clibwto.h>
#include "clibio.h"     /* __dsalcf(), __dsfree() */

#define CREATE  "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(1,0)"

#define LIMIT   20000           /* one track holds a few hundred records    */

static int  bad = 0;
static char dd[9];

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTX37 %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(int argc, char **argv)
{
    char    dsn[48];
    char    fname[52];
    char    rec[80];
    FILE    *fp;
    long    written;
    int     err;
    int     eno;
    int     rc;

    strcpy(dsn, (argc > 1 && argv[1][0]) ? argv[1] : "IBMUSER.TSTX37.WORK");

    printf("=== tstx37: #176 - out of space is a return code, not an ABEND"
           " ===\n");
    printf("    work data set %s\n\n", dsn);

    rc = __dsalcf(dd, CREATE, dsn);
    if (rc) {
        printf("      __dsalcf rc=%d - CANNOT MEASURE\n", rc);
        wtof("TSTX37 SETUP FAILED __dsalcf rc=%d", rc);
        return 8;
    }
    __dsfree(dd);                       /* the free catalogs it - ftpd's step 1 */

    sprintf(fname, "'%s'", dsn);
    fp = fopen(fname, "wb");
    if (!fp) {
        printf("      fopen(\"%s\",\"wb\") returned NULL - CANNOT MEASURE\n",
               fname);
        wtof("TSTX37 SETUP FAILED fopen returned NULL");
        return 8;
    }
    printf("      fopen ok, ddname=%.8s\n", fp->ddname);
    wtof("TSTX37 filling %.8s - an SD37 here is the RED", fp->ddname);

    /* No try().  If the exit does not do its job this abends, and the
       IEC031I in the job log says so louder than any return code. */
    memset(rec, 'X', sizeof(rec));
    for (written = 0; written < LIMIT; written++) {
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) break;
    }
    err = ferror(fp) ? 1 : 0;
    eno = errno;

    printf("      %ld record(s) written, ferror=%d errno=%d\n",
           written, err, eno);
    wtof("TSTX37 wrote=%ld ferror=%d errno=%d", written, err, eno);

    check(written < LIMIT, "(1) fwrite() stopped short - the write failed");
    check(err,             "(2) ferror() is set on the FILE");
    check(eno == ENOSPC,   "(3) errno is ENOSPC, not EIO");

    /* Plainly, not under try(): re-driving the failed block is the whole
       defect, and if it still happens this does not return. */
    fclose(fp);
    check(1, "(4) fclose() returned - nothing was re-driven");

    rc = remove(dsn);
    printf("      remove(\"%s\") rc=%d\n", dsn, rc);
    wtof("TSTX37 remove rc=%d", rc);
    check(rc == 0, "(5) the data set can be scratched - the DD went");

    printf("\n=== tstx37: %d check(s) failed ===\n", bad);
    wtof("TSTX37 VERDICT failed=%d", bad);

    return bad ? 8 : 0;
}
