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
 *   (4) the DISP=NEW path: fopen() on a data set that does not exist, with
 *       space and "rlse" in the mode string.  Note that __fpnew() sends no
 *       UNIT text unit, so this allocation rides on the system default - if
 *       the fopen() itself fails, the case reports that it could not measure
 *       and does NOT fail the run.  It says nothing about RLSE either way.
 *
 * SETUP: none.  The work data set is created by (1) and deleted at the end;
 * it must NOT exist when the test starts.
 *
 * PARM='<dsn>'   (default IBMUSER.TSTFPRLS.WORK)
 *
 * UNIT=SYSDA below is the esoteric name on the reference system - change it
 * if yours differs.
 *
 * BUILD (host):
 *     cc370 -O1 -Iinclude test/mvs/tstfprls.c -o TSTFPRLS \
 *           -flinker-output=xmit
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tstfprls.jcl.
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

static int  bad = 0;
static char dd[9];

/* ------------------------------------------------------------------------ *
 * Measurement: allocated tracks from the format-1 DSCB.
 *
 * Returns the track count, or a negative value when it could not measure -
 * which is NOT a failure of the thing under test and must not be reported as
 * one.  The negative value is the step that failed, so the log says where.
 * ------------------------------------------------------------------------ */
static long tracks_of(const char *dsn)
{
    LOCWORK  lw;
    DSCB     d1;
    DSCB     d4;
    char     dsn44[44];
    unsigned trkcyl;
    unsigned n;
    long     tracks = 0;
    int      rc;
    int      i;

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
    trkcyl = d4.dscb4.dstrk;
    if (!trkcyl) { printf("      dstrk is 0\n"); return -3; }

    rc = __dscbdv(dsn44, lw.volser, &d1);
    if (rc) { printf("      __dscbdv rc=%d\n", rc); return -4; }

    n = d1.dscb1.noepv;
    if (n > 3) {
        /* the format-3 chain is not walked: 30 tracks in one request does
           not produce one, and a run that does has measured something else */
        printf("      %u extents - more than the 3 in the format-1 DSCB\n", n);
        return -5;
    }

    for (i = 0; i < (int)n; i++) {
        EXTENT   *e = &d1.dscb1.extent[i];
        unsigned lo = ((unsigned)e->lower[0] << 8 | e->lower[1]) * trkcyl
                    + ((unsigned)e->lower[2] << 8 | e->lower[3]);
        unsigned up = ((unsigned)e->upper[0] << 8 | e->upper[1]) * trkcyl
                    + ((unsigned)e->upper[2] << 8 | e->upper[3]);

        tracks += (long)(up - lo) + 1;
    }

    printf("      volser=%.6s extents=%u trk/cyl=%u lstar=%02X%02X%02X\n",
           lw.volser, n, trkcyl,
           d1.dscb1.lstar[0], d1.dscb1.lstar[1], d1.dscb1.lstar[2]);

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
    printf("    allocated: %ld track(s)\n", t);
    if (t < 0) {
        printf("    *** the probe cannot measure this volume - no verdict\n");
        drop(dsn);
        printf("\nTSTFPRLS COULD NOT MEASURE\n");
        return 8;
    }
    if (t != PRIMARY) {
        printf("    *** expected %d - the measurement is not what it claims,\n"
               "        so the cases below cannot be read\n", PRIMARY);
        drop(dsn);
        printf("\nTSTFPRLS COULD NOT MEASURE\n");
        return 8;
    }
    printf("    ok\n");

    /* ---------------------------------------------------------------- */
    printf("\n(2) fopen(dsn,\"wb\") + one record + fclose: nothing released\n");
    if (write_one(dsn, "wb")) {
        printf("    *** FAIL - plain fopen() for output failed\n");
        bad++;
    }
    else {
        t = tracks_of(dsn);
        printf("    allocated: %ld track(s), want %d\n", t, PRIMARY);
        if (t == PRIMARY) printf("    ok\n");
        else { printf("    *** FAIL - space moved without RLSE\n"); bad++; }
    }

    /* ---------------------------------------------------------------- */
    printf("\n(3) fopen(dsn,\"wb,rlse\") + one record + fclose: released\n");
    printf("    this is mvslovers/ftpd's shape: DISP=OLD, no space keys\n");
    if (write_one(dsn, "wb,rlse")) {
        printf("    *** FAIL - SVC 99 would not take DALRLSE with DISP=OLD\n");
        bad++;
    }
    else {
        t = tracks_of(dsn);
        printf("    allocated: %ld track(s), want fewer than %d\n",
               t, PRIMARY);
        if (t > 0 && t < PRIMARY) printf("    ok\n");
        else { printf("    *** FAIL - nothing was released at CLOSE\n"); bad++; }
    }

    drop(dsn);

    /* ---------------------------------------------------------------- */
    printf("\n(4) the DISP=NEW path: fopen() creates it, with \"rlse\"\n");
    if (write_one(dsn, "wb,recfm=fb,lrecl=80,blksize=800,"
                       "space=trk(30,5),rlse")) {
        printf("    fopen() failed - __fpnew() sends no UNIT text unit, so\n"
               "    this may be the system default unit and nothing to do\n"
               "    with RLSE.  NOT counted as a failure; the case simply\n"
               "    did not measure anything.\n");
    }
    else {
        t = tracks_of(dsn);
        printf("    allocated: %ld track(s), want fewer than %d\n",
               t, PRIMARY);
        if (t > 0 && t < PRIMARY) printf("    ok\n");
        else { printf("    *** FAIL - nothing was released at CLOSE\n"); bad++; }
        drop(dsn);
    }

    printf("\nTSTFPRLS %s\n", bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
