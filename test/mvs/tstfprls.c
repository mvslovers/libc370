/*
 * tstfprls.c - libc370 #167: RLSE through fopen(), measured on MVS.
 *
 * The host test (test/host/tstfprls.c) pins that the DALRLSE text unit is
 * BUILT, and only when asked.  Two things it cannot answer, and this probe
 * exists for exactly those:
 *
 *   - does SVC 99 ACCEPT DALRLSE in the shape __fpold() sends it, DISP=OLD
 *     with no space keys at all?  The JCL equivalent, DISP=OLD with
 *     SPACE=(,,RLSE), is valid - but "valid in JCL" is not a measurement, and
 *     this is the shape mvslovers/ftpd hits (ftpd#100, ftpd#127): it
 *     allocates with __dsalcf(), __dsfree()s that DD, and then fopen()s the
 *     data set by name.
 *   - does MVS then RELEASE the unused space at CLOSE?
 *
 * The second one is why this is a measuring probe and not a return-code test.
 * Every case allocates TRK(30,5), writes ONE record, closes, and then reads
 * the format-1 DSCB back with OBTAIN and adds up the extents.  30 tracks
 * retained means nothing was released; a handful means it was.
 *
 * RLSE is honoured at CLOSE of the DCB opened against the DD that carried it.
 * fclose() runs __aclose(fp->dcb) before __fpfree() drops the DD, so the DD
 * fopen() allocated is still there when CLOSE looks - that is the whole
 * reason the fix sits in @@fpold.c/@@fpnew.c and not in the __dsalc() opts
 * parser, whose DD is long gone by then.
 *
 * CASES
 *   (1) create TRK(30,5) with __dsalcf() - ftpd's step 1 - and measure.
 *       30 tracks, or the probe cannot measure and says so instead of
 *       printing a verdict.
 *   (2) fopen(dsn,"wb") on it, one record, fclose.  Still 30 tracks: the
 *       regression guard for every consumer that does not ask for RLSE.
 *   (3) fopen(dsn,"wb,rlse"), one record, fclose.  FEWER than 30 tracks.
 *       This is the case ftpd is waiting on.
 *   (4) the DISP=NEW path with no "rlse": fopen() creates the data set itself
 *       from the space in the mode string.  30 tracks.  This is the control
 *       (5) needs - without it, "(5) ended at 1 track" cannot tell 29 tracks
 *       RELEASED from 30 tracks never allocated.
 *   (5) the same with "rlse".  FEWER than 30 tracks.
 *
 * (4) and (5) both note that __fpnew() sends no UNIT text unit, so the
 * allocation rides on the system default: if the fopen() itself fails, the
 * case reports that it could not measure and does NOT fail the run.  It says
 * nothing about RLSE either way.
 *
 * SETUP: none.  The work data set is created by (1) and deleted at the end;
 * it must NOT exist when the test starts.
 *
 * PARM='<dsn>'   (default IBMUSER.TSTFPRLS.WORK)
 *
 * UNIT=SYSDA below is the esoteric name on the reference system - change it
 * if yours differs.
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING: without it -lc autocalls the
 * INSTALLED sysroot libc370, which does not carry the #167 fix, and (3) and
 * (5) go red for a reason that has nothing to do with the target.  `make
 * build` first, and check the module really took it - the pre-fix library has
 * no reference to @@TXRLSE at all:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstfprls.c \
 *           -o TSTFPRLS -flinker-output=iebcopy
 *     python3 -c "print(open('TSTFPRLS','rb').read().count(\
 *                       '@@TXRLSE'.encode('cp037')))"      # must be > 0
 *     ld370 --pack TSTFPRLS=TSTFPRLS.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.RLSSCR
 *
 * Then upload probe.xmit to IBMUSER.MBT.XMIT.IN, run jcl/recvfprl.jcl, run
 * jcl/tstfprls.jcl.
 *
 * MEASURED 2026-09-13 on mvsdev, JOB00229, CC 0000, TSTFPRLS PASSED.
 * Volume WORK00, 30 tracks per cylinder (a 3350, per the same mapping
 * @@listds.c uses).  Cross-check the arithmetic by hand: 005B0005 is
 * 91*30+5 = 2735, 005C0004 is 92*30+4 = 2764, and 2764-2735+1 = 30.
 *
 *     (1) __dsalcf() TRK(30,5)   extent 005B0005-005C0004   30 tracks
 *     (2) fopen "wb"             extent 005B0005-005C0004   30 tracks
 *     (3) fopen "wb,rlse"        extent 005B0005-005B0005    1 track
 *     (4) fopen NEW, no rlse     extent 005B0005-005C0004   30 tracks
 *     (5) fopen NEW, "rlse"      extent 005B0005-005B0005    1 track
 *
 * So SVC 99 DOES accept DALRLSE with DISP=OLD and no space keys - ftpd's
 * shape - and CLOSE released 29 of the 30 tracks.  No SVC 99 returned
 * nonzero in any case; every fopen() succeeded.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include "clibio.h"     /* __dsalcf(), __dsfree() */
#include "clibdscb.h"   /* __locate(), __dscbdv(), __dscbv() */

#define CREATE  "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(30,5)"

#define PRIMARY 30                      /* tracks asked for in CREATE       */

/* what __fpnew() is given for the DISP=NEW cases; (5) appends ",rlse" */
#define NEWMODE "wb,recfm=fb,lrecl=80,blksize=800,space=trk(30,5)"

static int  bad = 0;
static int  unmeasured = 0;     /* cases that HAD to measure and did not     */
static char dd[9];

/* ------------------------------------------------------------------------ *
 * Measurement: allocated tracks from the format-1 DSCB.
 *
 * OBTAIN (SVC 27, CAMLST SEARCH - what __dscbv()/__dscbdv() issue) returns the
 * 96-byte DATA portion of the DSCB: the 44-byte key is the search argument,
 * not part of the answer.  clibdscb.h models that for `struct dscb1`, which
 * starts at fmtid, and NOT for `struct dscb4`, which starts with `key[44]` -
 * so d4.dscb4.dstrk reads 44 bytes past the field and comes back 0.  (Measured
 * 2026-09-13, JOB00223: "dstrk is 0".)
 *
 * Rather than trust either model, this finds the format id byte and reads
 * relative to it, and prints the leading bytes so the run itself says which
 * layout it got.
 *
 * Returns the track count, or a negative value when it could not measure -
 * which is NOT a failure of the thing under test and must not be reported as
 * one.  The negative value is the step that failed, so the log says where.
 * ------------------------------------------------------------------------ */

/* offsets inside the DATA portion */
#define F4_DSTRK    20              /* DS4DSTRK - tracks per cylinder       */
#define F1_NOEPV    15              /* DS1NOEPV - extents on this volume    */
#define F1_EXT1     0x3D            /* DS1EXT1  - first of three extents    */
#define F1_LSTAR    0x36            /* DS1LSTAR - last used TTR             */
#define EXTLEN      10              /* flag, seq, lower CCHH, upper CCHH    */

static const unsigned char *dscb_data(const DSCB *d, unsigned char id,
                                      const char *what)
{
    const unsigned char *w = (const unsigned char *)d;
    int i;

    printf("      %s work area:", what);
    for (i = 0; i < 16; i++) printf(" %02X", w[i]);
    printf("\n");

    if (w[0]  == id) return w;          /* data portion only - what SEARCH   */
    if (w[44] == id) return w + 44;     /* ... or key + data, should it be   */

    printf("      no %02X format id in the OBTAIN work area\n", id);
    return NULL;
}

static long tracks_of(const char *dsn)
{
    LOCWORK             lw;
    DSCB                d1;
    DSCB                d4;
    const unsigned char *f1;
    const unsigned char *f4;
    char                dsn44[44];
    unsigned            trkcyl;
    unsigned            n;
    long                tracks = 0;
    int                 rc;
    int                 i;

    /* the OBTAIN services want a 44-byte blank-padded name */
    for (i = 0; i < 44; i++) {
        dsn44[i] = (dsn[i] && dsn[i] > ' ') ? dsn[i] : ' ';
        if (!dsn[i]) break;
    }
    for (; i < 44; i++) dsn44[i] = ' ';

    memset(&lw, 0, sizeof(lw));
    rc = __locate(dsn44, &lw);
    if (rc) { printf("      __locate rc=%d\n", rc); return -1; }

    rc = __dscbv(lw.volser, &d4);
    if (rc) { printf("      __dscbv rc=%d\n", rc); return -2; }
    f4 = dscb_data(&d4, 0xF4, "fmt4");
    if (!f4) return -2;

    trkcyl = ((unsigned)f4[F4_DSTRK] << 8) | f4[F4_DSTRK + 1];
    if (!trkcyl) { printf("      tracks per cylinder is 0\n"); return -3; }

    rc = __dscbdv(dsn44, lw.volser, &d1);
    if (rc) { printf("      __dscbdv rc=%d\n", rc); return -4; }
    f1 = dscb_data(&d1, 0xF1, "fmt1");
    if (!f1) return -4;

    n = f1[F1_NOEPV];
    if (n < 1 || n > 3) {
        /* the format-3 chain is not walked: 30 tracks in one request does
           not produce one, and a run that does has measured something else */
        printf("      %u extents - not the 1 to 3 the format-1 DSCB holds\n", n);
        return -5;
    }

    for (i = 0; i < (int)n; i++) {
        const unsigned char *e  = f1 + F1_EXT1 + (i * EXTLEN);
        unsigned             lo = (((unsigned)e[2] << 8 | e[3]) * trkcyl)
                                +  ((unsigned)e[4] << 8 | e[5]);
        unsigned             up = (((unsigned)e[6] << 8 | e[7]) * trkcyl)
                                +  ((unsigned)e[8] << 8 | e[9]);

        printf("      extent %d: %02X%02X%02X%02X - %02X%02X%02X%02X"
               " = %u track(s)\n", i,
               e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9],
               up - lo + 1);
        tracks += (long)(up - lo) + 1;
    }

    printf("      volser=%.6s extents=%u trk/cyl=%u lstar=%02X%02X%02X\n",
           lw.volser, n, trkcyl,
           f1[F1_LSTAR], f1[F1_LSTAR + 1], f1[F1_LSTAR + 2]);

    return tracks;
}

/* ------------------------------------------------------------------------ */
static int write_one(const char *dsn, const char *mode)
{
    char  fname[48];
    FILE *fp;

    sprintf(fname, "'%s'", dsn);

    fp = fopen(fname, mode);
    if (!fp) {
        printf("      fopen(\"%s\",\"%s\") returned NULL\n", fname, mode);
        return 1;
    }
    fprintf(fp, "TSTFPRLS - one record, the rest of the space is unused\n");
    fclose(fp);
    return 0;
}

static void drop(const char *dsn)
{
    if (__dsalcf(dd, "DSN=%s;DISP=(OLD,DELETE)", dsn) == 0) __dsfree(dd);
}

static int create(const char *dsn)
{
    int rc = __dsalcf(dd, CREATE, dsn);

    if (rc == 0) __dsfree(dd);          /* the free catalogs it - ftpd's step 1 */
    return rc;
}

/* ------------------------------------------------------------------------ *
 * One case, reported so a RED run is readable without the source.  Three
 * outcomes, never blurred into each other:
 *
 *   OPEN FAILED  fopen() returned NULL.  Nothing was written and nothing was
 *                measured - there is no number, and a missing number is not a
 *                wrong one.  For the cases that carry the point (a `critical`
 *                one) this IS the failure: it means SVC 99 would not take the
 *                request, and a caller spelling "rlse" loses the open.
 *   NO MEASURE   the open worked, the DSCB walk did not.  Says nothing about
 *                RLSE either way.  The negative code says which step gave up.
 *   MEASURED     a real track count, right or wrong.  Only this is a verdict.
 *
 * `want` > 0 means exactly that many tracks, < 0 means fewer than its
 * magnitude.  A non-critical case (the DISP=NEW control, which rides on the
 * system default unit) reports and counts nothing when it cannot measure.
 * ------------------------------------------------------------------------ */
#define OPENED_NOT  (-1000L)    /* distinct from tracks_of()'s -1 .. -5      */

static long run_case(const char *dsn, const char *mode)
{
    if (write_one(dsn, mode)) return OPENED_NOT;
    return tracks_of(dsn);
}

static void report(long t, long want, int critical)
{
    if (t == OPENED_NOT) {
        printf("    OPEN FAILED - fopen() returned NULL; nothing measured\n");
        if (critical) {
            printf("    *** FAIL - SVC 99 would not take the request\n");
            bad++;
        }
        else {
            printf("    not counted - see the note on the case above\n");
        }
        return;
    }

    if (t < 0) {
        printf("    NO MEASURE - the DSCB walk gave up at step %ld;"
               " nothing measured\n", -t);
        if (critical) unmeasured++;
        else printf("    not counted - see the note on the case above\n");
        return;
    }

    if (want > 0) {
        printf("    MEASURED %ld track(s), want %ld\n", t, want);
        if (t == want) { printf("    ok\n"); return; }
    }
    else {
        printf("    MEASURED %ld track(s), want fewer than %ld\n", t, -want);
        if (t > 0 && t < -want) { printf("    ok\n"); return; }
    }

    printf("    *** FAIL - measured, and the number is wrong\n");
    bad++;
}

/* ------------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    const char *dsn = "IBMUSER.TSTFPRLS.WORK";
    char        buf[45];
    long        t;
    int         rc;

    if (argc > 1 && argv[1] && argv[1][0] > ' ') {
        unsigned i;

        for (i = 0; i < sizeof(buf) - 1 && argv[1][i] > ' '; i++) {
            buf[i] = argv[1][i];
        }
        buf[i] = 0;
        dsn = buf;
    }

    printf("TSTFPRLS - libc370 #167 probe, work dsn '%s'\n\n", dsn);

    /* ---------------------------------------------------------------- */
    printf("(1) __dsalcf() TRK(%d,5), then measure\n", PRIMARY);
    rc = create(dsn);
    if (rc) {
        printf("    __dsalcf rc=%d - nothing to test against\n", rc);
        printf("\nTSTFPRLS COULD NOT RUN\n");
        return 8;
    }
    t = tracks_of(dsn);
    if (t < 0) {
        printf("    NO MEASURE - the DSCB walk gave up at step %ld\n", -t);
        printf("    the probe cannot measure this volume; no case below can\n"
               "    be read, so none of them runs\n");
        drop(dsn);
        printf("\nTSTFPRLS COULD NOT MEASURE\n");
        return 8;
    }
    printf("    MEASURED %ld track(s), want %d\n", t, PRIMARY);
    if (t != PRIMARY) {
        printf("    the measurement is not what it claims to be, so the\n"
               "    cases below cannot be read either\n");
        drop(dsn);
        printf("\nTSTFPRLS COULD NOT MEASURE\n");
        return 8;
    }
    printf("    ok\n");

    /* ---------------------------------------------------------------- */
    printf("\n(2) fopen(dsn,\"wb\") + one record + fclose: nothing released\n");
    report(run_case(dsn, "wb"), PRIMARY, 1);

    /* ---------------------------------------------------------------- */
    printf("\n(3) fopen(dsn,\"wb,rlse\") + one record + fclose: released\n");
    printf("    this is mvslovers/ftpd's shape: DISP=OLD, no space keys\n");
    report(run_case(dsn, "wb,rlse"), -PRIMARY, 1);

    drop(dsn);

    /* ---------------------------------------------------------------- */
    printf("\n(4) the DISP=NEW path, no \"rlse\": fopen() creates it\n");
    printf("    __fpnew() sends no UNIT text unit, so this rides on the\n"
           "    system default and a failure here need not be about RLSE:\n"
           "    the case is not counted either way, and (5) is skipped.\n");
    t = run_case(dsn, NEWMODE);
    report(t, PRIMARY, 0);

    if (t >= 0) {
        drop(dsn);

        /* ------------------------------------------------------------ */
        printf("\n(5) the DISP=NEW path with \"rlse\"\n");
        printf("    against (4) on the same path: the difference is the "
               "keyword\n");
        report(run_case(dsn, NEWMODE ",rlse"), -PRIMARY, 1);
        drop(dsn);
    }

    printf("\nTSTFPRLS %s\n", bad         ? "FAILED"
                            : unmeasured ? "COULD NOT MEASURE"
                                         : "PASSED");
    if (unmeasured) {
        printf("  %d case(s) that had to measure did not - the run proves\n"
               "  nothing about them, which is not the same as passing\n",
               unmeasured);
    }
    return (bad || unmeasured) ? 8 : 0;
}
