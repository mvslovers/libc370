/*
 * tstclspc.c - libc370 #182: an out-of-space on the block CLOSE writes.
 *
 * #176 turned an out-of-space WRITE into ferror() + ENOSPC, but only for
 * the blocks that go out through @@AWRITE.  The last, partial block of a
 * data set is written by FIXWRITE inside @@ACLOSE, which ended FUNEXIT
 * RC=0 whatever happened, and fclose() returned 0 on every path.  So a
 * data set that had room for every full block but not for the short one
 * at the end lost its tail and the caller was told it had succeeded.
 * tstnospc.c cannot see this: every failure it provokes is on a full
 * block, mid-stream.
 *
 * SHAPE.  Same as tstnospc.c: __dsalcf() creates with SPACE=TRK(1,0), so
 * the first overflow is a D37 and not an extend, __dsfree() catalogs, and
 * fopen(dsn,"wb") opens by name.
 *
 *   calibrate  fill the track until ENOSPC, fclose(), read it back: R
 *              records survive, all of them in full blocks.  Nothing is
 *              asserted about R - it depends on the device - only that it
 *              is a whole number of blocks.
 *   p = 1..9   rewrite the data set with R + p records.  The first R go out
 *              as full blocks and fit; the last p sit in a short block that
 *              only CLOSE writes.  Whether that block fits depends on what
 *              the track has left, so some p lose it and some may not.
 *
 * THE CONTRACT, per case: fclose() returns EOF exactly when the read-back
 * is short, and errno is ENOSPC when it does.  C99 7.19.5.1 - "EOF if any
 * errors were detected".
 *
 * CHECKS
 *   (1) calibration: the fill stopped on ENOSPC        - control
 *   (2) calibration: R is a positive whole number of blocks - control
 *   (3) at least one case lost its short block         - the probe reached
 *                                                        the close path
 *   (4) per case: fclose() == EOF  <=>  records were lost   - THE FIX
 *   (5) per case: EOF comes with errno ENOSPC
 *   (6) per case: the full blocks survived (read-back >= R)
 *   (7) remove() answered 0
 *
 * RED before the fix: (4) fails for every case that lost records - fclose()
 * answers 0.  Nothing abends; a WRITE at close under the #176 exit returns.
 *
 * SETUP: none.  The work data set must NOT exist when the probe starts.
 *
 * PARM='<dsn>'   (default IBMUSER.TSTCLSPC.WORK)
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING - it links the branch libc,
 * not the installed sysroot:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstclspc.c \
 *           -o TSTCLSPC -flinker-output=iebcopy
 *     ld370 --pack TSTCLSPC=TSTCLSPC.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.CLSSCR
 *
 * upload probe.xmit to IBMUSER.LIBC370.CLSXMIT and run jcl/tstclspc.jcl,
 * which RECEIVEs it and runs the probe.
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

#define LRECL   80
#define PERBLK  10              /* BLKSIZE / LRECL                          */
#define LIMIT   20000           /* one track holds a few hundred records    */

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTCLSPC %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* Write n records (or until one comes up short, when n is LIMIT) and close.
   Returns the records fwrite() accepted; fclose()'s rc and errno go back
   through the pointers. */
static long fill(const char *fname, long n, int *crc, int *ceno)
{
    char    rec[LRECL];
    FILE    *fp;
    long    i;

    fp = fopen(fname, "wb");
    if (!fp) return -1;

    for (i = 0; i < n; i++) {
        memset(rec, 'A' + (int)(i % 26), sizeof(rec));
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) break;
    }

    errno = 0;
    *crc  = fclose(fp);
    *ceno = errno;
    return i;
}

/* Records on disk, or -1 if the data set cannot be opened. */
static long readback(const char *fname)
{
    char    rec[LRECL];
    FILE    *fp;
    long    n = 0;

    fp = fopen(fname, "rb");
    if (!fp) return -1;
    while (fread(rec, 1, sizeof(rec), fp) == sizeof(rec)) n++;
    fclose(fp);
    return n;
}

int main(int argc, char **argv)
{
    char    dsn[48];
    char    fname[52];
    char    dd[9];
    long    wrote, r, got, want;
    int     crc, ceno, rc, p;
    int     lost_any = 0;
    int     ok4 = 1, ok5 = 1, ok6 = 1;

    strcpy(dsn, (argc > 1 && argv[1][0]) ? argv[1] : "IBMUSER.TSTCLSPC.WORK");
    sprintf(fname, "'%s'", dsn);

    printf("=== tstclspc: #182 - out of space on the block CLOSE writes ===\n");
    printf("    work data set %s\n\n", dsn);

    rc = __dsalcf(dd, CREATE, dsn);
    if (rc) {
        printf("      __dsalcf rc=%d - CANNOT MEASURE\n", rc);
        wtof("TSTCLSPC SETUP FAILED __dsalcf rc=%d", rc);
        return 8;
    }
    __dsfree(dd);

    /* ---- calibrate: how many records fit in full blocks --------------- */
    wrote = fill(fname, LIMIT, &crc, &ceno);
    r     = readback(fname);
    printf("      calibrate: fwrite accepted %ld, fclose rc=%d errno=%d, "
           "%ld on disk\n", wrote, crc, ceno, r);
    wtof("TSTCLSPC calibrate wrote=%ld fclose=%d errno=%d disk=%ld",
         wrote, crc, ceno, r);

    check(wrote >= 0 && wrote < LIMIT, "(1) the fill stopped short");
    check(r > 0 && r % PERBLK == 0,
          "(2) R is a positive whole number of blocks");
    if (wrote < 0 || r <= 0 || r % PERBLK) goto done;

    /* ---- R + p: a short block that only CLOSE writes ------------------ */
    printf("\n       p  want  disk  fclose  errno\n");
    for (p = 1; p < PERBLK; p++) {
        want = r + p;
        wrote = fill(fname, want, &crc, &ceno);
        got   = readback(fname);

        printf("      %2d  %4ld  %4ld  %6d  %5d%s\n", p, want, got, crc, ceno,
               got != want ? "   lost" : "");
        wtof("TSTCLSPC p=%d want=%ld wrote=%ld disk=%ld fclose=%d errno=%d",
             p, want, wrote, got, crc, ceno);

        if (got != want) lost_any = 1;
        if ((crc == EOF) != (got != want)) ok4 = 0;
        if (crc == EOF && ceno != ENOSPC)  ok5 = 0;
        if (got < r)                       ok6 = 0;
    }
    printf("\n");

    check(lost_any, "(3) at least one case lost its short block");
    check(ok4,      "(4) fclose() == EOF exactly when records were lost");
    check(ok5,      "(5) EOF comes with errno ENOSPC");
    check(ok6,      "(6) the full blocks survived every case");

done:
    rc = remove(dsn);
    printf("      remove(\"%s\") rc=%d\n", dsn, rc);
    check(rc == 0, "(7) the data set can be scratched");

    printf("\n=== tstclspc: %d check(s) failed ===\n", bad);
    wtof("TSTCLSPC VERDICT failed=%d", bad);

    return bad ? 8 : 0;
}
