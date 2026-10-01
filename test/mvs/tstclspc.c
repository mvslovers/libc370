/*
 * tstclspc.c - libc370 #182, #228: an out-of-space on the block CLOSE writes.
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
 * #228: THE OTHER CLOSES.  The same R + p cases, finished by something
 * other than fclose(), each of which closes the DCB itself:
 *
 *   freopen()  the "wb" stream is reopened onto DD OTHER (DUMMY).  C99
 *              7.19.5.4 ignores a failure to close, so freopen() still
 *              succeeds; errno is the contract - ENOSPC exactly when the
 *              read-back is short.
 *   turn       a "w+b" stream reads after its writes, so fgetc() turns the
 *              DCB round.  EOF with ferror() and errno ENOSPC exactly when
 *              the read-back is short; after clearerr() and a seek to 0 the
 *              stream reads what is on disk.
 *   rclose()   ropen()/rwrite() onto a second TRK(1,0) data set, <dsn>R,
 *              held allocated by __dsalcf() and opened as "dd:<its DD>",
 *              so #229's leaked allocation stays out of the measurement.  ropen() sets its own DCB, so RCLS is
 *              calibrated separately, and p runs until rwrite() refuses -
 *              that is the next full block, no longer a close case.
 *              rclose() answers -1, errno ENOSPC, exactly when the
 *              read-back is short.
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
 *   (8) freopen: at least one case lost its short block - control
 *   (9) freopen: never answered NULL
 *  (10) freopen: errno ENOSPC exactly when records were lost    - #228
 *  (11) turn: at least one case lost its short block            - control
 *  (12) turn: fgetc() EOF with ferror() exactly when lost       - #228
 *  (13) turn: ferror() comes with errno ENOSPC
 *  (14) turn: after clearerr() + fseek(0), it reads what is on disk
 *  (15) rclose: the <dsn>R fill stopped short                     - control
 *  (16) rclose: at least one case lost its short block          - control
 *  (17) rclose: -1 exactly when records were lost               - #228
 *  (18) rclose: -1 comes with errno ENOSPC
 *
 * RED before the fix: (4) fails for every case that lost records - fclose()
 * answers 0.  Nothing abends; a WRITE at close under the #176 exit returns.
 * Before #228, (10), (12) and (17) fail the same way (JOB00744); (13) passes
 * then only because ferror() is never set.
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
#include <mvs/dynalloc.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/rfile.h>
#include "stdio.h"     /* __dsalcf(), __dsfree() */

#define CREATE  "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(1,0)"

#define LRECL   80
#define PERBLK  10              /* BLKSIZE / LRECL                          */
#define LIMIT   20000           /* one track holds a few hundred records    */
#define RMAX    100             /* rclose: more than a V block of 80s holds  */

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

/* #228: n records, then freopen() onto DD OTHER.  NULL goes back in *nul,
   errno after the freopen() in *ceno. */
static long fill_freopen(const char *fname, long n, int *nul, int *ceno)
{
    char    rec[LRECL];
    FILE    *fp, *f2;
    long    i;

    fp = fopen(fname, "wb");
    if (!fp) return -1;

    for (i = 0; i < n; i++) {
        memset(rec, 'A' + (int)(i % 26), sizeof(rec));
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) break;
    }

    errno = 0;
    f2    = freopen("dd:OTHER", "rb", fp);
    *ceno = errno;
    *nul  = (f2 == NULL);
    fclose(f2 ? f2 : fp);
    return i;
}

/* #228: n records on a "w+b" stream, then a read turns it round.  *err is
   ferror() after that fgetc(), *ceno errno, *reread what the stream reads
   after clearerr() and fseek(0) (-1 if the seek fails). */
static long fill_turn(const char *fname, long n, int *c, int *err,
                      int *ceno, long *reread)
{
    char    rec[LRECL];
    FILE    *fp;
    long    i;

    fp = fopen(fname, "w+b");
    if (!fp) return -1;

    for (i = 0; i < n; i++) {
        memset(rec, 'A' + (int)(i % 26), sizeof(rec));
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) break;
    }

    errno = 0;
    *c    = fgetc(fp);
    *ceno = errno;
    *err  = ferror(fp) != 0;

    clearerr(fp);
    *reread = -1;
    if (fseek(fp, 0L, SEEK_SET) == 0) {
        *reread = 0;
        while (fread(rec, 1, sizeof(rec), fp) == sizeof(rec)) (*reread)++;
    }
    fclose(fp);
    return i;
}

/* #228: n records through ropen()/rwrite() onto rname ("dd:<DD>"), then
   rclose().  Returns the records rwrite() accepted; *short_ is set when
   one refused. */
static long fill_rclose(const char *rname, long n, int *short_, int *crc,
                        int *ceno)
{
    char    rec[LRECL];
    RFILE   *rf = NULL;
    long    i;

    *short_ = 0;
    if (ropen(rname, 1, &rf) || !rf) return -1;

    for (i = 0; i < n; i++) {
        memset(rec, 'A' + (int)(i % 26), sizeof(rec));
        if (rwrite(rf, rec, sizeof(rec))) {
            *short_ = 1;
            break;
        }
    }

    errno = 0;
    *crc  = rclose(rf);
    *ceno = errno;
    return i;
}

/* Records on rname, or -1 if it cannot be opened. */
static long readback_rclose(const char *rname)
{
    static char buf[512];   /* a V record from ropen() is at most 255 */
    RFILE   *rf = NULL;
    size_t  len;
    long    n = 0;

    if (ropen(rname, 0, &rf) || !rf) return -1;
    while (rread(rf, buf, &len) == 0) n++;
    rclose(rf);
    return n;
}

int main(int argc, char **argv)
{
    char    dsn[48];
    char    fname[52];
    char    dd[9];
    char    dsn2[48];
    char    dd2[9];
    char    rname[16];
    int     have2 = 0;
    long    wrote, r, got, want;
    int     crc, ceno, rc, p;
    int     lost_any = 0;
    int     ok4 = 1, ok5 = 1, ok6 = 1;
    int     nul, c, ferr, sh;
    long    reread;
    int     ok9 = 1, ok10 = 1, ok12 = 1, ok13 = 1, ok14 = 1, ok17 = 1, ok18 = 1;

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

    /* ---- #228: freopen() ---------------------------------------------- */
    printf("\n      freopen\n       p  want  disk  null  errno\n");
    lost_any = 0;
    for (p = 1; p < PERBLK; p++) {
        want  = r + p;
        wrote = fill_freopen(fname, want, &nul, &ceno);
        got   = readback(fname);

        printf("      %2d  %4ld  %4ld  %4d  %5d%s\n", p, want, got, nul, ceno,
               got != want ? "   lost" : "");
        wtof("TSTCLSPC freopen p=%d want=%ld wrote=%ld disk=%ld null=%d "
             "errno=%d", p, want, wrote, got, nul, ceno);

        if (got != want) lost_any = 1;
        if (nul) ok9 = 0;
        if ((ceno == ENOSPC) != (got != want)) ok10 = 0;
    }
    check(lost_any, "(8) freopen: at least one case lost its short block");
    check(ok9,      "(9) freopen: never answered NULL");
    check(ok10,     "(10) freopen: errno ENOSPC exactly when records were lost");

    /* ---- #228: a "w+" stream turned round by a read -------------------- */
    printf("\n      turn\n       p  want  disk  fgetc  ferror  errno  reread\n");
    lost_any = 0;
    for (p = 1; p < PERBLK; p++) {
        want  = r + p;
        wrote = fill_turn(fname, want, &c, &ferr, &ceno, &reread);
        got   = readback(fname);

        printf("      %2d  %4ld  %4ld  %5d  %6d  %5d  %6ld%s\n", p, want, got,
               c, ferr, ceno, reread, got != want ? "   lost" : "");
        wtof("TSTCLSPC turn p=%d want=%ld wrote=%ld disk=%ld fgetc=%d "
             "ferror=%d errno=%d reread=%ld", p, want, wrote, got, c, ferr,
             ceno, reread);

        if (got != want) lost_any = 1;
        if ((c == EOF && ferr) != (got != want)) ok12 = 0;
        if (ferr && ceno != ENOSPC)             ok13 = 0;
        if (reread != got)                      ok14 = 0;
    }
    check(lost_any, "(11) turn: at least one case lost its short block");
    check(ok12,     "(12) turn: fgetc() EOF with ferror() exactly when lost");
    check(ok13,     "(13) turn: ferror() comes with errno ENOSPC");
    check(ok14,     "(14) turn: after clearerr() + fseek(0) it reads the disk");

    /* ---- #228: rclose() on <dsn>R, held by its DD ---------------------- */
    sprintf(dsn2, "%sR", dsn);
    rc = __dsalcf(dd2, CREATE, dsn2);
    if (rc) {
        printf("      __dsalcf(%s) rc=%d - CANNOT MEASURE\n", dsn2, rc);
        wtof("TSTCLSPC SETUP FAILED __dsalcf(%s) rc=%d", dsn2, rc);
        bad++;
        goto done;
    }
    have2 = 1;
    sprintf(rname, "dd:%s", dd2);
    for (p = (int)strlen(rname); p > 3 && rname[p - 1] == ' '; p--)
        rname[p - 1] = 0;

    wrote = fill_rclose(rname, LIMIT, &sh, &crc, &ceno);
    r     = readback_rclose(rname);
    printf("\n      rclose calibrate: rwrite accepted %ld, rclose rc=%d "
           "errno=%d, %ld on disk\n", wrote, crc, ceno, r);
    wtof("TSTCLSPC rclose calibrate wrote=%ld rclose=%d errno=%d disk=%ld",
         wrote, crc, ceno, r);
    check(wrote >= 0 && sh, "(15) rclose: the RCLS fill stopped short");
    if (wrote < 0 || !sh || r <= 0) goto done;

    printf("       p  want  disk  rclose  errno\n");
    lost_any = 0;
    for (p = 1; p < RMAX; p++) {
        want  = r + p;
        wrote = fill_rclose(rname, want, &sh, &crc, &ceno);
        if (sh) break;          /* a full block refused: not a close case */
        got   = readback_rclose(rname);

        printf("      %2d  %4ld  %4ld  %6d  %5d%s\n", p, want, got, crc, ceno,
               got != want ? "   lost" : "");
        wtof("TSTCLSPC rclose p=%d want=%ld disk=%ld rclose=%d errno=%d",
             p, want, got, crc, ceno);

        if (got != want) lost_any = 1;
        if ((crc == -1) != (got != want)) ok17 = 0;
        if (crc == -1 && ceno != ENOSPC)  ok18 = 0;
    }
    check(lost_any, "(16) rclose: at least one case lost its short block");
    check(ok17,     "(17) rclose: -1 exactly when records were lost");
    check(ok18,     "(18) rclose: -1 comes with errno ENOSPC");

done:
    rc = remove(dsn);
    printf("      remove(\"%s\") rc=%d\n", dsn, rc);
    if (have2) {
        __dsfree(dd2);
        c = remove(dsn2);
        printf("      remove(\"%s\") rc=%d\n", dsn2, c);
        if (c) rc = c;
    }
    check(rc == 0, "(7) the data sets can be scratched");

    printf("\n=== tstclspc: %d check(s) failed ===\n", bad);
    wtof("TSTCLSPC VERDICT failed=%d", bad);

    return bad ? 8 : 0;
}
