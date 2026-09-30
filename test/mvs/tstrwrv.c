/*
 * tstrwrv.c - libc370 #232: rwrite() refuses a record @@AWRITE cannot take.
 *
 * On RECFM=V @@AWRITE wants the record with its RDW and abends 002
 * (WRITEBAD) when the RDW does not match the length, or when the record
 * does not fit: size+4 > LRECL on a spanned data set, size+4 > BLKSIZE on
 * any other.  rwrite() passed the caller's buffer through, and copied it
 * into asmbuf - LRECL bytes - on every RECFM without a bound.  Plain
 * records on VB killed the task (U0002, mvsdev JOB00767).
 *
 * The contract (include/rfile.h): the RDW is the caller's, as rread()
 * returns it.  rwrite() answers 1 + EINVAL for a record it cannot take and
 * writes nothing; an __awrite() failure sets ENOSPC or EIO.
 *
 * GROUPS (PARM='<prefix> <group>', group ALL - the default - runs all of
 *         them in order)
 *   VB     on <prefix>.VB (VB 255/6233)
 *          (1) a record with RDW is written
 *          (2) a record of LRECL (255) is written
 *          (3) a record of LRECL+1 is refused with EINVAL   - THE FIX
 *          (4) the read-back holds (1) and (2), nothing else
 *              (RED: ABEND U1234 in the read-back - (3) is on disk)
 *   VBBAD  on <prefix>.VB, rewritten
 *          (5) size 3 is refused with EINVAL                - THE FIX
 *          (6) an RDW with bytes 2-3 set is refused         - THE FIX
 *          (7) a plain record - no RDW - is refused         - THE FIX
 *              (RED: ABEND U0002, the step dies here)
 *          (8) the read-back is empty: nothing was written
 *   FB     on <prefix>.FB (FB 80/800)
 *          (9) a short record is written, (10) one of LRECL
 *         (11) a record of LRECL+1 is refused with EINVAL   - THE FIX
 *         (12) the read-back holds (9) padded and (10)
 *   VBS    on <prefix>.VBS (VBS 255/6233)
 *         (13) a record of LRECL-4 (251) is written
 *         (14) the read-back holds it
 *         (15) a record of LRECL-4+1 is refused             - THE FIX
 *              (RED: ABEND U0002)
 *   NOSPC  on <prefix>.SML (FB 80/800, TRK(1,0))
 *         (16) writing until rwrite() fails ends in ENOSPC  - THE FIX
 *
 * RED before the fix (installed sysroot), mvsdev JOB00800, one group per
 * step:
 *   VB     (3) FAILs: the 256-byte record is accepted, rc=0, and written
 *          to a data set of LRECL 255; the read-back of (4) then abends
 *          U1234 in @@AREAD (BADBLOCK, "problem processing RECFM=V(bs)
 *          file") - one oversized record poisons the data set for every
 *          reader.
 *   VBBAD  (5) and (6) FAIL, rc=0; (7) ABEND U0002.
 *   FB     (11) FAILs and (12) reads 3 records: the 81-byte one is
 *          written, cut to LRECL.
 *   VBS    (13)/(14) pass; (15) ABEND U0002.
 *   NOSPC  (16) FAILs: rc=1 after 199 records with errno 0.
 * GREEN, the same job: 16/16.
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING - it links the branch libc:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstrwrv.c \
 *           -o TSTRWRV -flinker-output=iebcopy
 *
 * jcl/tstrwrv.jcl creates the data sets, RECEIVEs the probe and runs it.
 *
 * RC: 0 = every check passed, 8 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/rfile.h>

#define TEXT    "TSTRWRV #232 RECORD"

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTRWRV %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* A V record of total length len (RDW included) carrying TEXT. */
static void vrec(unsigned char *r, int len)
{
    memset(r, 'V', len);
    r[0] = (unsigned char)(len >> 8);
    r[1] = (unsigned char)(len & 0xFF);
    r[2] = 0;
    r[3] = 0;
    if (len > 4) memcpy(r + 4, TEXT, len - 4 < (int)strlen(TEXT)
                                     ? len - 4 : (int)strlen(TEXT));
}

/* rwrite() of one record; wtof() first, the RED case may not come back */
static int put(RFILE *rf, const unsigned char *r, int len, const char *tag)
{
    int rc;

    wtof("TSTRWRV put %s size=%d", tag, len);
    errno = 0;
    rc = rwrite(rf, r, (size_t)len);
    printf("      rwrite(%s, %d) rc=%d errno=%d\n", tag, len, rc, errno);
    wtof("TSTRWRV put %s rc=%d errno=%d", tag, rc, errno);
    return rc;
}

/* Read the data set back: record count, and each record's length and
   first bytes into lens[]/recs[] (up to 4 records). */
static int readback(const char *dsn, size_t lens[4], unsigned char recs[4][256])
{
    RFILE           *rf = NULL;
    unsigned char   buf[512];
    size_t          got;
    int             n = 0;

    if (ropen(dsn, 0, &rf) || !rf) {
        printf("      readback ropen(%s) failed errno=%d\n", dsn, errno);
        return -1;
    }
    for (;;) {
        got = 0;
        if (rread(rf, buf, &got)) break;
        if (n < 4) {
            lens[n] = got;
            memcpy(recs[n], buf, got > 256 ? 256 : got);
        }
        n++;
        if (n > 1000) break;
    }
    rclose(rf);
    printf("      readback %s: %d record(s)\n", dsn, n);
    wtof("TSTRWRV readback %s n=%d", dsn, n);
    return n;
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

static void group_vb(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[300];
    size_t          lens[4];
    unsigned char   recs[4][256];
    RFILE           *rf;
    int             n;

    sprintf(dsn, "%s.VB", pfx);
    printf("--- VB %s\n", dsn);
    if (!(rf = wopen(dsn))) return;

    vrec(r, 4 + (int)strlen(TEXT));
    check(put(rf, r, 4 + (int)strlen(TEXT), "VB rdw") == 0,
          "(1) VB: a record with RDW is written");
    vrec(r, 255);
    check(put(rf, r, 255, "VB lrecl") == 0,
          "(2) VB: a record of LRECL is written");
    vrec(r, 256);
    n = put(rf, r, 256, "VB lrecl+1");
    check(n != 0 && errno == EINVAL,
          "(3) VB: a record of LRECL+1 is refused with EINVAL");
    rclose(rf);

    n = readback(dsn, lens, recs);
    check(n == 2 && lens[0] == 4 + strlen(TEXT) && lens[1] == 255
          && recs[0][0] == 0 && recs[0][1] == 4 + strlen(TEXT)
          && memcmp(recs[0] + 4, TEXT, strlen(TEXT)) == 0
          && recs[1][0] == 0 && recs[1][1] == 255,
          "(4) VB: the read-back holds (1) and (2), RDWs included, nothing else");
}

static void group_vbbad(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[300];
    size_t          lens[4];
    unsigned char   recs[4][256];
    RFILE           *rf;
    int             n;

    sprintf(dsn, "%s.VB", pfx);
    printf("--- VBBAD %s\n", dsn);
    if (!(rf = wopen(dsn))) return;

    vrec(r, 3);
    n = put(rf, r, 3, "VB size3");
    check(n != 0 && errno == EINVAL, "(5) VB: size 3 is refused with EINVAL");
    vrec(r, 24);
    r[2] = 0x80;
    n = put(rf, r, 24, "VB flags");
    check(n != 0 && errno == EINVAL,
          "(6) VB: an RDW with bytes 2-3 set is refused with EINVAL");
    memset(r, ' ', sizeof(r));
    memcpy(r, TEXT, strlen(TEXT));
    n = put(rf, r, (int)strlen(TEXT), "VB plain");
    check(n != 0 && errno == EINVAL,
          "(7) VB: a plain record is refused with EINVAL, no abend");
    rclose(rf);

    n = readback(dsn, lens, recs);
    check(n == 0, "(8) VB: the read-back is empty, nothing was written");
}

static void group_fb(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[100];
    unsigned char   want[80];
    size_t          lens[4];
    unsigned char   recs[4][256];
    RFILE           *rf;
    int             n;

    sprintf(dsn, "%s.FB", pfx);
    printf("--- FB %s\n", dsn);
    if (!(rf = wopen(dsn))) return;

    memcpy(r, TEXT, strlen(TEXT));
    check(put(rf, r, (int)strlen(TEXT), "FB short") == 0,
          "(9) FB: a short record is written");
    memset(r, 'F', sizeof(r));
    check(put(rf, r, 80, "FB lrecl") == 0,
          "(10) FB: a record of LRECL is written");
    n = put(rf, r, 81, "FB lrecl+1");
    check(n != 0 && errno == EINVAL,
          "(11) FB: a record of LRECL+1 is refused with EINVAL");
    rclose(rf);

    memset(want, ' ', sizeof(want));
    memcpy(want, TEXT, strlen(TEXT));
    n = readback(dsn, lens, recs);
    check(n == 2 && lens[0] == 80 && lens[1] == 80
          && memcmp(recs[0], want, 80) == 0
          && recs[1][0] == 'F' && recs[1][79] == 'F',
          "(12) FB: the read-back holds (9) padded and (10), nothing else");
}

static void group_vbs(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[300];
    size_t          lens[4];
    unsigned char   recs[4][256];
    RFILE           *rf;
    int             n;

    sprintf(dsn, "%s.VBS", pfx);
    printf("--- VBS %s\n", dsn);
    if (!(rf = wopen(dsn))) return;

    vrec(r, 251);
    check(put(rf, r, 251, "VBS lrecl-4") == 0,
          "(13) VBS: a record of LRECL-4 is written");
    rclose(rf);
    n = readback(dsn, lens, recs);
    check(n == 1 && lens[0] == 251 && recs[0][0] == 0 && recs[0][1] == 251
          && memcmp(recs[0] + 4, TEXT, strlen(TEXT)) == 0,
          "(14) VBS: the read-back holds it");

    if (!(rf = wopen(dsn))) return;
    vrec(r, 252);
    n = put(rf, r, 252, "VBS lrecl-3");
    check(n != 0 && errno == EINVAL,
          "(15) VBS: a record of LRECL-3 is refused with EINVAL, no abend");
    rclose(rf);
}

static void group_nospc(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[80];
    RFILE           *rf;
    int             i, rc = 0, err = 0;

    sprintf(dsn, "%s.SML", pfx);
    printf("--- NOSPC %s\n", dsn);
    if (!(rf = wopen(dsn))) return;

    memset(r, 'S', sizeof(r));
    for (i = 0; i < 20000; i++) {
        errno = 0;
        if ((rc = rwrite(rf, r, sizeof(r))) != 0) {
            err = errno;
            break;
        }
    }
    printf("      %d records written, then rc=%d errno=%d\n", i, rc, err);
    wtof("TSTRWRV nospc written=%d rc=%d errno=%d", i, rc, err);
    check(rc != 0 && err == ENOSPC,
          "(16) NOSPC: writing until rwrite() fails ends in ENOSPC");
    rclose(rf);
}

int main(int argc, char **argv)
{
    const char  *pfx   = argc > 1 ? argv[1] : "IBMUSER.TSTRWRV";
    const char  *group = argc > 2 ? argv[2] : "ALL";
    int         all    = strcmp(group, "ALL") == 0;

    printf("=== tstrwrv: #232 on %s, group %s ===\n", pfx, group);
    wtof("TSTRWRV start %s %s", pfx, group);

    if (all || strcmp(group, "VB") == 0)    group_vb(pfx);
    if (all || strcmp(group, "VBBAD") == 0) group_vbbad(pfx);
    if (all || strcmp(group, "FB") == 0)    group_fb(pfx);
    if (all || strcmp(group, "VBS") == 0)   group_vbs(pfx);
    if (all || strcmp(group, "NOSPC") == 0) group_nospc(pfx);

    printf("\n=== tstrwrv: %d check(s) failed ===\n", bad);
    wtof("TSTRWRV VERDICT %s failed=%d", group, bad);

    return bad ? 8 : 0;
}
