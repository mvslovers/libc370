/*
 * tstspan.c - libc370 #757: a record written across a block boundary of a
 * spanned data set keeps all of its segments, and the data set stays
 * readable.
 *
 * @@AWRITE (src/stdio/@@awrite.asm) splits a RECFM=V record into segments
 * when it does not fit the space left in the block of a spanned data set
 * (RECFM=VBS, or RECFM=VB with BLKSIZE < LRECL+4, which the OPEN exit makes
 * spanned).  The segmentation path had never run: a mis-sized branch took
 * every first segment for the last one, dropped the rest of the record and
 * reported success.  The data set was unreadable from that record on, for
 * libc370 (@@AREAD: BADBLOCK, ABEND U1234) and for IDCAMS (ABEND S002).
 *
 * GROUPS (PARM='<prefix> <group>', group ALL - the default - runs all of
 *         them in order)
 *   VBS    on <prefix>.VBS (VBS 255/100, 96 bytes of records per block),
 *          seven records placed so that every segmentation case occurs:
 *            r1  90 data  fits block 1, 2 bytes left (< 5: the next
 *                         record flushes the block first)
 *            r2  92 data  fills block 2 exactly (96 with its RDW)
 *            r3 247 data  the longest record (LRECL-4-4): first segment
 *                         96, middle segment 96, last segment 67
 *                         - a record over THREE blocks
 *            r4  30 data  split 29 + 9: the last segment opens block 6
 *            r5  84 data  split 87 + 5: a last segment of 1 data byte
 *            r6  86 data  fits, 1 byte left
 *            r7  10 data  block 8
 *          (1) every rwrite() answers 0
 *          (2) the read-back returns 7 records
 *          (3) every record has its length and every byte of its data
 *              (RED: ABEND U1234 in the read-back, or fewer records)
 *   FWRITE on <prefix>.FWR (VBS 255/100), the same seven records through
 *          fopen("wb,record") / fwrite(), read back with fread()
 *          (4) every fwrite() answers 1
 *          (5) the read-back returns 7 records
 *          (6) every record is intact
 *   VB     on <prefix>.VB (VB 255/255), the issue's own case: four records
 *          of 79, 80, 81 and 10 data bytes; the OPEN exit makes the data
 *          set spanned, the 81-byte record is split 83 + 6
 *          (7) every rwrite() answers 0
 *          (8) the read-back returns 4 records
 *          (9) every record is intact
 *              (RED: 2 records, then ABEND U1234)
 *   BIG    on <prefix>.BIG (VBS 255/6233), the issue's table: 100 records
 *          of 200 data bytes; the 31st is the first that does not fit
 *         (10) every rwrite() answers 0
 *         (11) the read-back returns 100 records
 *         (12) every record is intact
 *              (RED: 30 records, then ABEND U1234)
 *   VS     on <prefix>.VS (VS 255/100, unblocked spanned): the seven
 *          records of VBS again; one record or segment per block, so a
 *          last segment is written at once and the next record starts
 *          a block of its own: r4 and r5 are not split here, the seven
 *          records take 9 blocks (the IDCAMS PRINT of the raw blocks in
 *          jcl/tstspan.jcl shows it: no block holds two segments)
 *         (13) every rwrite() answers 0
 *         (14) the read-back returns 7 records
 *         (15) every record is intact
 *
 * Every record's bytes are a pattern of its number (pattern()), so a
 * segment stitched to the wrong record, a lost tail or a doubled byte at
 * a segment boundary all show as a FAIL in (3), (6), (9), (12) or (15).
 *
 * jcl/tstspan.jcl also hands every data set to IDCAMS PRINT (IBM's own
 * reader, not libc370's) and dumps the raw blocks of <prefix>.VBS and
 * <prefix>.VS as RECFM=U, so the SDWs can be read in the job output.
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING - it links the branch libc:
 *
 *     make build
 *     cc370 -O1 -Wall -Iinclude -L build/sdk test/mvs/tstspan.c \
 *           -o TSTSPAN -flinker-output=iebcopy
 *
 * RC: 0 = every check passed, 8 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/rfile.h>

#define MAXREC  256                 /* RDW + data of the longest record     */
#define NREC    100                 /* records of the BIG group             */

static int  bad = 0;

/* The seven record data lengths of the VBS and FWRITE groups, see above. */
static const int seven[7] = { 90, 92, 247, 30, 84, 86, 10 };

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTSPAN %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* The data of record number n (from 1), byte j: letters and digits in a
   cycle whose phase depends on n, so no two neighbouring records share
   the same byte at the same offset. */
static unsigned char pattern(int n, int j)
{
    static const char   alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    return (unsigned char)alpha[(n * 7 + j) % 36];
}

/* Build record n with len data bytes into r: RDW (length with RDW, bytes
   2-3 zero), then the pattern.  Returns the length with RDW. */
static int build(unsigned char *r, int n, int len)
{
    int     j;

    r[0] = (unsigned char)((len + 4) >> 8);
    r[1] = (unsigned char)((len + 4) & 0xFF);
    r[2] = 0;
    r[3] = 0;
    for (j = 0; j < len; j++) r[4 + j] = pattern(n, j);
    return len + 4;
}

/* Does the record read back as number n with len data bytes match?  got is
   the length rread()/fread() returned, r holds the RDW and the data. */
static int same(const unsigned char *r, size_t got, int n, int len)
{
    int     j;

    if (got != (size_t)(len + 4)) {
        printf("      record %d: length %u, expected %d\n", n, (unsigned)got,
               len + 4);
        return 0;
    }
    if (((r[0] << 8) | r[1]) != len + 4 || r[2] || r[3]) {
        printf("      record %d: RDW %02X%02X%02X%02X\n", n, r[0], r[1],
               r[2], r[3]);
        return 0;
    }
    for (j = 0; j < len; j++) {
        if (r[4 + j] != pattern(n, j)) {
            printf("      record %d: byte %d is %02X, expected %02X\n", n, j,
                   r[4 + j], pattern(n, j));
            return 0;
        }
    }
    return 1;
}

static RFILE *wopen(const char *dsn)
{
    RFILE   *rf = NULL;

    if (ropen(dsn, 1, &rf) || !rf) {
        printf("      ropen(%s, 1) failed errno=%d\n", dsn, errno);
        check(0, "open for output");
        return NULL;
    }
    printf("      ropen(%s, 1) recfm=%d lrecl=%d\n", dsn, rf->recfm, rf->lrecl);
    return rf;
}

/* Write records 1..count with the data lengths in lens[] (one length for
   all when lens is NULL) through rwrite().  Returns the number written. */
static int rput(const char *dsn, const int *lens, int fixed, int count)
{
    unsigned char   r[MAXREC];
    RFILE           *rf;
    int             n, len, ok = 0;

    if (!(rf = wopen(dsn))) return -1;
    for (n = 1; n <= count; n++) {
        len = lens ? lens[n - 1] : fixed;
        len = build(r, n, len);
        wtof("TSTSPAN rwrite %s record %d size %d", dsn, n, len);
        errno = 0;
        if (rwrite(rf, r, (size_t)len) != 0) {
            printf("      rwrite(%s, record %d, %d) failed errno=%d\n", dsn,
                   n, len, errno);
            break;
        }
        ok++;
    }
    if (rclose(rf) != 0) {
        printf("      rclose(%s) failed errno=%d\n", dsn, errno);
        return -1;
    }
    return ok;
}

/* Read the data set back through rread() and compare every record with
   its pattern.  Returns the number of records, and sets *intact to 1 when
   all of them matched. */
static int rget(const char *dsn, const int *lens, int fixed, int count,
                int *intact)
{
    RFILE           *rf = NULL;
    unsigned char   buf[512];
    size_t          got;
    int             n = 0;

    *intact = 1;
    wtof("TSTSPAN rread %s", dsn);
    if (ropen(dsn, 0, &rf) || !rf) {
        printf("      readback ropen(%s) failed errno=%d\n", dsn, errno);
        *intact = 0;
        return -1;
    }
    for (;;) {
        got = 0;
        if (rread(rf, buf, &got)) break;
        n++;
        if (n > count || !same(buf, got, n, lens ? lens[n - 1] : fixed))
            *intact = 0;
        if (n > 1000) break;
    }
    rclose(rf);
    printf("      readback %s: %d record(s)\n", dsn, n);
    wtof("TSTSPAN rread %s n=%d", dsn, n);
    return n;
}

static void group_vbs(const char *pfx)
{
    char    dsn[64];
    int     n, intact;

    sprintf(dsn, "%s.VBS", pfx);
    printf("--- VBS %s\n", dsn);
    n = rput(dsn, seven, 0, 7);
    check(n == 7, "(1) VBS: every rwrite() answers 0");
    n = rget(dsn, seven, 0, 7, &intact);
    check(n == 7, "(2) VBS: the read-back returns 7 records");
    check(n == 7 && intact,
          "(3) VBS: every record has its length and every byte of its data");
}

static void group_fwrite(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[512];
    FILE            *fp;
    int             n, len, ok = 0, intact = 1;

    sprintf(dsn, "%s.FWR", pfx);
    printf("--- FWRITE %s\n", dsn);
    if ((fp = fopen(dsn, "wb,record")) == NULL) {
        printf("      fopen(%s, \"wb,record\") failed errno=%d\n", dsn, errno);
        check(0, "open for output");
        return;
    }
    for (n = 1; n <= 7; n++) {
        len = build(r, n, seven[n - 1]);
        wtof("TSTSPAN fwrite %s record %d size %d", dsn, n, len);
        errno = 0;
        if (fwrite(r, (size_t)len, 1, fp) != 1) {
            printf("      fwrite(%s, record %d, %d) failed errno=%d\n", dsn,
                   n, len, errno);
            break;
        }
        ok++;
    }
    if (fclose(fp) != 0) printf("      fclose(%s) failed errno=%d\n", dsn, errno);
    check(ok == 7, "(4) FWRITE: every fwrite() answers 1");

    wtof("TSTSPAN fread %s", dsn);
    if ((fp = fopen(dsn, "rb,record")) == NULL) {
        printf("      readback fopen(%s) failed errno=%d\n", dsn, errno);
        check(0, "(5) FWRITE: the read-back opens");
        return;
    }
    for (n = 0;;) {
        size_t  got;

        memset(r, 0, sizeof(r));
        if ((got = fread(r, 1, sizeof(r), fp)) == 0) break;
        n++;
        if (n > 7 || !same(r, got, n, seven[n - 1])) intact = 0;
        if (n > 1000) break;
    }
    fclose(fp);
    printf("      readback %s: %d record(s)\n", dsn, n);
    wtof("TSTSPAN fread %s n=%d", dsn, n);
    check(n == 7, "(5) FWRITE: the read-back returns 7 records");
    check(n == 7 && intact, "(6) FWRITE: every record is intact");
}

static void group_vb(const char *pfx)
{
    static const int    four[4] = { 79, 80, 81, 10 };
    char                dsn[64];
    int                 n, intact;

    sprintf(dsn, "%s.VB", pfx);
    printf("--- VB %s\n", dsn);
    n = rput(dsn, four, 0, 4);
    check(n == 4, "(7) VB: every rwrite() answers 0");
    n = rget(dsn, four, 0, 4, &intact);
    check(n == 4, "(8) VB: the read-back returns 4 records");
    check(n == 4 && intact, "(9) VB: every record is intact");
}

static void group_big(const char *pfx)
{
    char    dsn[64];
    int     n, intact;

    sprintf(dsn, "%s.BIG", pfx);
    printf("--- BIG %s\n", dsn);
    n = rput(dsn, NULL, 200, NREC);
    check(n == NREC, "(10) BIG: every rwrite() answers 0");
    n = rget(dsn, NULL, 200, NREC, &intact);
    check(n == NREC, "(11) BIG: the read-back returns 100 records");
    check(n == NREC && intact, "(12) BIG: every record is intact");
}

static void group_vs(const char *pfx)
{
    char    dsn[64];
    int     n, intact;

    sprintf(dsn, "%s.VS", pfx);
    printf("--- VS %s\n", dsn);
    n = rput(dsn, seven, 0, 7);
    check(n == 7, "(13) VS: every rwrite() answers 0");
    n = rget(dsn, seven, 0, 7, &intact);
    check(n == 7, "(14) VS: the read-back returns 7 records");
    check(n == 7 && intact, "(15) VS: every record is intact");
}

int main(int argc, char **argv)
{
    const char  *pfx   = argc > 1 ? argv[1] : "IBMUSER.TSTSPAN";
    const char  *group = argc > 2 ? argv[2] : "ALL";
    int         all    = strcmp(group, "ALL") == 0;

    printf("=== tstspan: #757 on %s, group %s ===\n", pfx, group);
    wtof("TSTSPAN start %s %s", pfx, group);

    if (all || strcmp(group, "VBS") == 0)    group_vbs(pfx);
    if (all || strcmp(group, "FWRITE") == 0) group_fwrite(pfx);
    if (all || strcmp(group, "VB") == 0)     group_vb(pfx);
    if (all || strcmp(group, "BIG") == 0)    group_big(pfx);
    if (all || strcmp(group, "VS") == 0)     group_vs(pfx);

    printf("\n=== tstspan: %d check(s) failed ===\n", bad);
    wtof("TSTSPAN VERDICT %s failed=%d", group, bad);

    return bad ? 8 : 0;
}
