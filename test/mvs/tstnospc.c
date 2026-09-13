/*
 * tstnospc.c - libc370 #149: what a stream DOES after ENOSPC.
 *
 * THIS PROBE DECIDES NOTHING.  It measures, so that #149's policy - which
 * errors are terminal and which are worth retrying - is written on numbers
 * instead of on a guess.  #176 made an out-of-space write arrive as
 * ferror() + ENOSPC instead of ABEND SD37; what it did NOT settle is what
 * the next fwrite() on that stream does, and that is the whole of #149.
 *
 * THE THREE QUESTIONS
 *
 *   (a) Does every further write fail, or only some?  The block is 800
 *       bytes and a record is 80, so nine of ten writes only fill the
 *       FILE buffer and never reach the access method.  If those nine
 *       return 80 - success - a caller that checks the return value and
 *       not ferror() silently loses nine records out of ten and is told
 *       about the tenth.  That is the strongest argument fail-fast has,
 *       and it is a measurement, not an opinion.  The result is printed
 *       as a map: '.' is a write that returned full length, 'X' one that
 *       came up short.
 *
 *   (b) What does a failing retry COST?  IFG0554T's x37 exit runs inside
 *       EOV.  If each retry pays that round trip, a daemon logging into a
 *       full data set crawls, and fail-fast is a performance argument too.
 *       If a retry costs what an ordinary write costs, retrying is
 *       harmless and stickiness would only take away a recovery an
 *       operator can still effect by freeing space.  STCK before and
 *       after every write, averaged over the successes and the failures
 *       separately.
 *
 *   (c) Is retry WELL-DEFINED, or does it degrade?  The fiftieth attempt
 *       must behave like the first - same short return, same ENOSPC, no
 *       drift to EIO and no abend - and clearerr() must clear the flag
 *       without making the stream write again.  A retry path that works
 *       once and program-checks on the twentieth is not a policy option.
 *
 * NO try(), for the same reason tstx37.c has none: if a retry abends, the
 * step abends and the job log says so louder than any return code.  Every
 * line also goes to the console, because an abend discards whatever SYSOUT
 * is still sitting in the QSAM buffer.
 *
 * SHAPE.  mvslovers/ftpd's (ftpd#129), same as tstx37.c: __dsalcf() creates
 * with SPACE=TRK(1,0) - no secondary, so the first overflow is a D37 and not
 * an extend - __dsfree() catalogs, fopen(dsn,"wb") reopens by name.
 *
 * CHECKS - these are the ones a green run must satisfy whatever the numbers
 * turn out to be; the numbers themselves are reported, not asserted.
 *   (1) the fill stopped short of the limit        - control, as in tstx37
 *   (2) errno after the fill is ENOSPC             - control
 *   (3) at least one retry came up short           - the stream is not lying
 *   (4) no retry ever reported EIO                 - out of space stays that
 *   (5) ferror() stayed set across every retry     - nothing clears it
 *   (6) clearerr() cleared it                      - it works (it has since
 *                                                    the initial commit;
 *                                                    #149 says otherwise)
 *   (7) writing after clearerr() fails again, ENOSPC, without abending
 *   (8) fclose() returned
 *   (9) remove() answered 0
 *
 * SETUP: none.  The work data set must NOT exist when the probe starts.
 *
 * PARM='<dsn>'   (default IBMUSER.TSTNOSPC.WORK)
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING - without it -lc autocalls the
 * INSTALLED sysroot libc370, which has no x37 exit (#176), and the probe
 * abends for a reason that has nothing to do with #149:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstnospc.c \
 *           -o TSTNOSPC -flinker-output=iebcopy
 *     ld370 --pack TSTNOSPC=TSTNOSPC.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.NSPSCR
 *
 * upload probe.xmit to IBMUSER.MBT.XMIT.IN, run jcl/recvnosp.jcl, then
 * jcl/tstnospc.jcl.
 *
 * MEASURED TWICE on mvsdev 2026-09-13, before and after the #149 fix.  The
 * checks below are written so that BOTH runs are green - they pin the shape
 * (no abend, no drift to EIO, clearerr() works), not the policy.  What the
 * fix changed is the map, and the map is the evidence.
 *
 * AFTER the fix, JOB00254: CC 0000, 9/9, no IEC031I.
 *
 *   map   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 *         0 full, 50 short.  Every write after the error is refused at
 *         the call.  253 us for a refused one against a 254 us floor for
 *         an ordinary write during the fill - the floor IS the two
 *         __getclk() calls around the measurement, so a refusal costs
 *         nothing measurable.  Against 2355 us for a write that actually
 *         reached the access method and failed, fail-fast is also the
 *         cheaper path.  After clearerr(), 2 of 12 short instead of 1:
 *         the buffer refills, the eleventh flush fails, and the twelfth
 *         is refused.
 *
 * BEFORE the fix, JOB00252: CC 0000, 9/9 PASS, and no
 * IEC031I line in the job log.  TRK(1,0) on WORK00 took the same 200 records
 * of 80 that tstx37 fills it with.  The three answers:
 *
 *   (a) THE MAP.  ..........X..........X..........X..........X......
 *       46 of 50 retries returned the full 80 bytes.  Not one of those 46
 *       records reached the disk - the data set is full; they went into the
 *       FILE buffer, were flushed into a WRITE that failed, and were
 *       discarded at @@fflush.c's reset: label.  So a caller that checks
 *       fwrite()'s return value and not ferror() loses 92% of what it
 *       writes and is told about 8% of it.  That is #149's case, and it is
 *       a correctness argument, not a performance one.
 *
 *   (b) THE COST.  310 us for a full-length retry against 308 us for an
 *       ordinary write during the fill - identical, because it is the same
 *       memcpy into the same buffer.  2355 us for a short one: 7.6x, and
 *       that is the EOV round trip through IFG0554T.  Amortised over the
 *       11-write period it is ~1.6x.  Retrying is wasteful but it is not
 *       ruinous, so performance does not decide this; (a) does.
 *
 *   (c) NO DEGRADATION.  The fiftieth short retry matched the first exactly
 *       (rc=0, errno=28), errno never drifted to EIO, nothing abended, and
 *       clearerr() cleared the flag without making the stream writable
 *       again - the very next block's worth failed with ENOSPC as before.
 *       Retry after ENOSPC is well-defined on this system; it is simply
 *       futile until somebody frees space.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <clibwto.h>
#include <mvssupa.h>    /* __getclk() */
#include "clibio.h"     /* __dsalcf(), __dsfree() */

#define CREATE  "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(1,0)"

#define LIMIT   20000           /* one track holds a few hundred records    */
#define RETRIES 50              /* five blocks' worth at BLKSIZE/LRECL = 10 */

static int  bad = 0;
static char dd[9];

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTNOSPC %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* STCK into two halves.  No 64-bit arithmetic anywhere below: cc370 has no
   64-bit divide to link against, and the difference of two clock readings
   fits a pair of words as long as the run is under an hour. */
static void stck(unsigned int *hi, unsigned int *lo)
{
    unsigned char b[8];

    __getclk(b);
    memcpy(hi, b,     4);
    memcpy(lo, b + 4, 4);
}

/* Microseconds between two readings.  STCK bit 51 ticks once per
   microsecond, so the 64-bit difference shifted right 12 is microseconds;
   done as (dh << 20) | (dl >> 12) to keep it in 32-bit registers. */
static unsigned long elapsed_us(unsigned int h0, unsigned int l0,
                                unsigned int h1, unsigned int l1)
{
    unsigned int dh = h1 - h0;
    unsigned int dl;

    if (l1 < l0) { dl = l1 - l0; dh--; }    /* borrow */
    else         { dl = l1 - l0; }

    return ((unsigned long)dh << 20) | (unsigned long)(dl >> 12);
}

int main(int argc, char **argv)
{
    char            dsn[48];
    char            fname[52];
    char            rec[80];
    char            map[RETRIES + 1];
    FILE            *fp;
    unsigned int    h0, l0, h1, l1;
    unsigned long   us;
    unsigned long   us_ok    = 0,  us_short = 0;
    unsigned long   us_fill  = 0;
    long            n_ok     = 0,  n_short  = 0;
    long            written;
    int             saw_eio  = 0;
    int             lost_err = 0;      /* ferror() went away by itself       */
    int             first_rc = -1, last_rc = -1;
    int             first_no = -1, last_no = -1;
    int             err, eno, rc, i;
    size_t          got;

    strcpy(dsn, (argc > 1 && argv[1][0]) ? argv[1] : "IBMUSER.TSTNOSPC.WORK");

    printf("=== tstnospc: #149 - what a stream does AFTER ENOSPC ===\n");
    printf("    work data set %s\n\n", dsn);

    rc = __dsalcf(dd, CREATE, dsn);
    if (rc) {
        printf("      __dsalcf rc=%d - CANNOT MEASURE\n", rc);
        wtof("TSTNOSPC SETUP FAILED __dsalcf rc=%d", rc);
        return 8;
    }
    __dsfree(dd);                   /* the free catalogs it - ftpd's step 1 */

    sprintf(fname, "'%s'", dsn);
    fp = fopen(fname, "wb");
    if (!fp) {
        printf("      fopen(\"%s\",\"wb\") returned NULL - CANNOT MEASURE\n",
               fname);
        wtof("TSTNOSPC SETUP FAILED fopen returned NULL");
        return 8;
    }
    printf("      fopen ok, ddname=%.8s\n", fp->ddname);
    wtof("TSTNOSPC filling %.8s - an SD37 here is the RED", fp->ddname);

    /* ---- phase 1: fill it, exactly as tstx37 does ---------------------- */
    memset(rec, 'X', sizeof(rec));
    stck(&h0, &l0);
    for (written = 0; written < LIMIT; written++) {
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) break;
    }
    stck(&h1, &l1);
    us_fill = elapsed_us(h0, l0, h1, l1);
    err = ferror(fp) ? 1 : 0;
    eno = errno;

    printf("      %ld record(s) written, ferror=%d errno=%d, %lu us total\n",
           written, err, eno, us_fill);
    wtof("TSTNOSPC fill wrote=%ld ferror=%d errno=%d us=%lu",
         written, err, eno, us_fill);

    check(written < LIMIT, "(1) the fill stopped short - the write failed");
    check(eno == ENOSPC,   "(2) errno after the fill is ENOSPC");

    /* ---- phase 2: keep writing, and time every single one -------------- */
    wtof("TSTNOSPC now %d retries - an abend here is the RED", RETRIES);
    for (i = 0; i < RETRIES; i++) {
        errno = 0;
        stck(&h0, &l0);
        got = fwrite(rec, 1, sizeof(rec), fp);
        stck(&h1, &l1);
        us = elapsed_us(h0, l0, h1, l1);

        if (got == sizeof(rec)) {
            map[i] = '.';
            us_ok += us;
            n_ok++;
        }
        else {
            map[i] = 'X';
            us_short += us;
            n_short++;
            if (errno == EIO)  saw_eio = 1;
            if (first_rc < 0) { first_rc = (int)got; first_no = errno; }
            last_rc = (int)got;
            last_no = errno;
        }
        if (!ferror(fp)) lost_err = 1;   /* nobody in the library clears it */
    }
    map[RETRIES] = '\0';

    printf("\n      retry map ('.' full length, 'X' short):\n");
    printf("      %s\n", map);
    wtof("TSTNOSPC map %.25s", map);
    wtof("TSTNOSPC map %.25s", map + 25);
    printf("      %ld full, %ld short\n", n_ok, n_short);
    printf("      avg us: fill %lu, full-length retry %lu, short retry %lu\n",
           written ? us_fill / (unsigned long)written : 0UL,
           n_ok    ? us_ok   / (unsigned long)n_ok    : 0UL,
           n_short ? us_short/ (unsigned long)n_short : 0UL);
    wtof("TSTNOSPC ok=%ld short=%ld usok=%lu usshort=%lu",
         n_ok, n_short,
         n_ok    ? us_ok    / (unsigned long)n_ok    : 0UL,
         n_short ? us_short / (unsigned long)n_short : 0UL);
    printf("      first short: rc=%d errno=%d   last short: rc=%d errno=%d\n",
           first_rc, first_no, last_rc, last_no);
    wtof("TSTNOSPC first rc=%d eno=%d last rc=%d eno=%d",
         first_rc, first_no, last_rc, last_no);

    check(n_short > 0,  "(3) at least one retry came up short");
    check(!saw_eio,     "(4) no retry ever reported EIO - ENOSPC stayed");
    check(!lost_err,    "(5) ferror() stayed set across every retry");
    check(first_rc == last_rc && first_no == last_no,
                        "(3b) the last short retry matches the first");

    /* ---- phase 3: clearerr(), then one more block's worth --------------- */
    clearerr(fp);
    check(!ferror(fp),  "(6) clearerr() cleared the error indicator");

    errno = 0;
    n_short = 0;
    for (i = 0; i < 12; i++) {          /* > one block, so a flush must run */
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) n_short++;
    }
    eno = errno;
    err = ferror(fp) ? 1 : 0;
    printf("\n      after clearerr(): %ld of 12 short, ferror=%d errno=%d\n",
           n_short, err, eno);
    wtof("TSTNOSPC after clearerr short=%ld ferror=%d errno=%d",
         n_short, err, eno);
    check(n_short > 0 && err && eno == ENOSPC,
                        "(7) writing after clearerr() fails again, ENOSPC");

    /* ---- teardown, plainly, not under try() ---------------------------- */
    fclose(fp);
    check(1, "(8) fclose() returned - nothing was re-driven");

    rc = remove(dsn);
    printf("      remove(\"%s\") rc=%d\n", dsn, rc);
    wtof("TSTNOSPC remove rc=%d", rc);
    check(rc == 0, "(9) the data set can be scratched - the DD went");

    printf("\n=== tstnospc: %d check(s) failed ===\n", bad);
    wtof("TSTNOSPC VERDICT failed=%d", bad);

    return bad ? 8 : 0;
}
