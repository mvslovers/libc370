/*
 * tstfwrec.c - libc370 #236: fwrite() in record mode refuses a record
 * @@AWRITE cannot take.
 *
 * The record path of @@fwrite.c copied size*nmemb bytes into asmbuf -
 * LRECL bytes - and handed them to __awrite() unchecked: the path #232
 * fixed in rwrite() (mvsdev JOB00800 measured it there).  On RECFM=V
 * @@AWRITE abends 002 when the RDW does not equal the length or a spanned
 * record exceeds LRECL-4; a record longer than LRECL overruns asmbuf and
 * is written as it is.  And size or nmemb 0 sent a record of length 0.
 *
 * The contract (include/clibio.h, _FILE_FLAG_RECORD): the RDW is the
 * caller's.  fwrite() answers 0 with errno EINVAL and writes nothing for a
 * record it cannot take, without setting the stream's error flag; size or
 * nmemb 0 answers 0 and writes nothing (C99 7.19.8.2).
 *
 * GROUPS (PARM='<prefix> <group>', ALL - the default - runs all in order)
 *   VB     on <prefix>.VB (VB 255/6233)
 *          (1) a record with RDW is written
 *          (2) a record of LRECL (255) is written
 *          (3) a record of LRECL+1 is refused with EINVAL   - THE FIX
 *          (4) the refusal leaves ferror() clear
 *          (5) the read-back holds (1) and (2), nothing else
 *   VBBAD  on <prefix>.VB, rewritten
 *          (6) size 3 is refused with EINVAL                - THE FIX
 *          (7) an RDW with bytes 2-3 set is refused         - THE FIX
 *          (8) a plain record - no RDW - is refused         - THE FIX
 *          (9) a valid record after the refusals is written
 *         (10) the read-back holds only (9)
 *   FB     on <prefix>.FB (FB 80/800)
 *         (11) a record of LRECL is written
 *         (12) a record of LRECL+1 is refused with EINVAL   - THE FIX
 *         (13) fwrite(p, 0, 1) answers 0                    - THE FIX
 *         (14) fwrite(p, 1, 0) answers 0                    - THE FIX
 *         (15) size*nmemb wrapping to 0 is refused          - THE FIX
 *         (16) the read-back holds only (11)
 *   VBS    on <prefix>.VBS (VBS 255/6233)
 *         (17) a record of LRECL-4 is written, (18) read back
 *         (19) a record of LRECL-3 is refused with EINVAL   - THE FIX
 *
 * Each put is announced by wtof() first, because before the fix three
 * groups die.  mvsdev JOB00805, one group per step against the installed
 * sysroot:
 *   VB     (3) FAILs - the 256-byte record is written - and the read-back
 *          of (5) abends U1234 in @@AREAD (BADBLOCK).
 *   VBBAD  (6) and (7) FAIL, fwrite() answers 1; (8) ABEND U0002.
 *   FB     (12)-(15) FAIL, each answers 1, and (16) reads 5 records: the
 *          81-byte one cut to LRECL and three records of length 0, written
 *          as blanks - size 0, nmemb 0, and 65536*65536 wrapped to 0.
 *   VBS    (17)/(18) pass; (19) ABEND U0002.
 * GREEN, the same job: 19/19.
 *
 * BUILD (host).  -L build/sdk is LOAD-BEARING - it links the branch libc:
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstfwrec.c \
 *           -o TSTFWREC -flinker-output=iebcopy
 *
 * jcl/tstfwrec.jcl creates the data sets, RECEIVEs the probe and runs it.
 *
 * RC: 0 = every check passed, 8 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>

#define TEXT    "TSTFWREC #236 RECORD"

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTFWREC %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* A V record of total length len (RDW included) carrying TEXT. */
static void vrec(unsigned char *r, int len)
{
    int t = (int)strlen(TEXT);

    memset(r, 'V', len);
    r[0] = (unsigned char)(len >> 8);
    r[1] = (unsigned char)(len & 0xFF);
    r[2] = 0;
    r[3] = 0;
    if (len > 4) memcpy(r + 4, TEXT, len - 4 < t ? len - 4 : t);
}

/* fwrite() of one record; wtof() first, the RED case may not come back */
static size_t put(FILE *fp, const void *r, size_t size, size_t nmemb,
                  const char *tag)
{
    size_t n;

    wtof("TSTFWREC put %s size=%u nmemb=%u", tag,
         (unsigned)size, (unsigned)nmemb);
    errno = 0;
    n = fwrite(r, size, nmemb, fp);
    printf("      fwrite(%s, %u, %u) = %u errno=%d ferror=%d\n", tag,
           (unsigned)size, (unsigned)nmemb, (unsigned)n, errno,
           ferror(fp) != 0);
    wtof("TSTFWREC put %s n=%u errno=%d", tag, (unsigned)n, errno);
    return n;
}

/* Read the data set back in record mode: the number of records, and the
   first up to 4 records' lengths (V: from the RDW, F: LRECL) and bytes. */
static int readback(const char *dsn, int v, size_t lens[4],
                    unsigned char recs[4][256])
{
    FILE            *fp;
    unsigned char   buf[512];
    int             n = 0;

    wtof("TSTFWREC readback %s", dsn);
    if ((fp = fopen(dsn, "rb,record")) == NULL) {
        printf("      readback fopen(%s) failed errno=%d\n", dsn, errno);
        return -1;
    }
    for (;;) {
        memset(buf, 0, sizeof(buf));
        if (fread(buf, 1, sizeof(buf), fp) == 0) break;
        if (n < 4) {
            lens[n] = v ? (size_t)((buf[0] << 8) | buf[1]) : 80;
            memcpy(recs[n], buf, 256);
        }
        if (++n > 1000) break;
    }
    fclose(fp);
    printf("      readback %s: %d record(s)\n", dsn, n);
    wtof("TSTFWREC readback %s n=%d", dsn, n);
    return n;
}

static FILE *wopen(const char *dsn)
{
    FILE    *fp = fopen(dsn, "wb,record");

    if (!fp) {
        printf("      fopen(%s, \"wb,record\") failed errno=%d\n", dsn, errno);
        check(0, "open for output");
    }
    return fp;
}

static void group_vb(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[300];
    size_t          lens[4];
    unsigned char   recs[4][256];
    size_t          t = strlen(TEXT);
    FILE            *fp;
    int             n;

    sprintf(dsn, "%s.VB", pfx);
    printf("--- VB %s\n", dsn);
    if (!(fp = wopen(dsn))) return;

    vrec(r, 4 + (int)t);
    check(put(fp, r, 1, 4 + t, "VB rdw") == 1,
          "(1) VB: a record with RDW is written");
    vrec(r, 255);
    check(put(fp, r, 1, 255, "VB lrecl") == 1,
          "(2) VB: a record of LRECL is written");
    vrec(r, 256);
    check(put(fp, r, 1, 256, "VB lrecl+1") == 0 && errno == EINVAL,
          "(3) VB: a record of LRECL+1 is refused with EINVAL");
    check(!ferror(fp), "(4) VB: the refusal leaves ferror() clear");
    fclose(fp);

    n = readback(dsn, 1, lens, recs);
    check(n == 2 && lens[0] == 4 + t && lens[1] == 255
          && memcmp(recs[0] + 4, TEXT, t) == 0,
          "(5) VB: the read-back holds (1) and (2), nothing else");
}

static void group_vbbad(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[300];
    size_t          lens[4];
    unsigned char   recs[4][256];
    size_t          t = strlen(TEXT);
    FILE            *fp;
    int             n;

    sprintf(dsn, "%s.VB", pfx);
    printf("--- VBBAD %s\n", dsn);
    if (!(fp = wopen(dsn))) return;

    vrec(r, 3);
    check(put(fp, r, 3, 1, "VB size3") == 0 && errno == EINVAL,
          "(6) VB: size 3 is refused with EINVAL");
    vrec(r, 24);
    r[2] = 0x80;
    check(put(fp, r, 24, 1, "VB flags") == 0 && errno == EINVAL,
          "(7) VB: an RDW with bytes 2-3 set is refused with EINVAL");
    memset(r, ' ', sizeof(r));
    memcpy(r, TEXT, t);
    check(put(fp, r, t, 1, "VB plain") == 0 && errno == EINVAL,
          "(8) VB: a plain record is refused with EINVAL, no abend");
    vrec(r, 4 + (int)t);
    check(put(fp, r, 4 + t, 1, "VB after") == 1,
          "(9) VB: a valid record after the refusals is written");
    fclose(fp);

    n = readback(dsn, 1, lens, recs);
    check(n == 1 && lens[0] == 4 + t && memcmp(recs[0] + 4, TEXT, t) == 0,
          "(10) VB: the read-back holds only (9)");
}

static void group_fb(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[100];
    size_t          lens[4];
    unsigned char   recs[4][256];
    FILE            *fp;
    int             n;

    sprintf(dsn, "%s.FB", pfx);
    printf("--- FB %s\n", dsn);
    if (!(fp = wopen(dsn))) return;

    memset(r, 'F', sizeof(r));
    check(put(fp, r, 80, 1, "FB lrecl") == 1,
          "(11) FB: a record of LRECL is written");
    check(put(fp, r, 81, 1, "FB lrecl+1") == 0 && errno == EINVAL,
          "(12) FB: a record of LRECL+1 is refused with EINVAL");
    check(put(fp, r, 0, 1, "FB size0") == 0,
          "(13) FB: fwrite(p, 0, 1) answers 0");
    check(put(fp, r, 1, 0, "FB nmemb0") == 0,
          "(14) FB: fwrite(p, 1, 0) answers 0");
    check(put(fp, r, 65536, 65536, "FB wrap") == 0 && errno == EINVAL,
          "(15) FB: size*nmemb wrapping to 0 is refused with EINVAL");
    fclose(fp);

    n = readback(dsn, 0, lens, recs);
    check(n == 1 && recs[0][0] == 'F' && recs[0][79] == 'F',
          "(16) FB: the read-back holds only (11)");
}

static void group_vbs(const char *pfx)
{
    char            dsn[64];
    unsigned char   r[300];
    size_t          lens[4];
    unsigned char   recs[4][256];
    FILE            *fp;
    int             n;

    sprintf(dsn, "%s.VBS", pfx);
    printf("--- VBS %s\n", dsn);
    if (!(fp = wopen(dsn))) return;

    vrec(r, 251);
    check(put(fp, r, 251, 1, "VBS lrecl-4") == 1,
          "(17) VBS: a record of LRECL-4 is written");
    fclose(fp);
    n = readback(dsn, 1, lens, recs);
    check(n == 1 && lens[0] == 251
          && memcmp(recs[0] + 4, TEXT, strlen(TEXT)) == 0,
          "(18) VBS: the read-back holds it");

    if (!(fp = wopen(dsn))) return;
    vrec(r, 252);
    check(put(fp, r, 252, 1, "VBS lrecl-3") == 0 && errno == EINVAL,
          "(19) VBS: a record of LRECL-3 is refused with EINVAL, no abend");
    fclose(fp);
}

int main(int argc, char **argv)
{
    const char  *pfx   = argc > 1 ? argv[1] : "IBMUSER.TSTFWREC";
    const char  *group = argc > 2 ? argv[2] : "ALL";
    int         all    = strcmp(group, "ALL") == 0;

    printf("=== tstfwrec: #236 on %s, group %s ===\n", pfx, group);
    wtof("TSTFWREC start %s %s", pfx, group);

    if (all || strcmp(group, "VB") == 0)    group_vb(pfx);
    if (all || strcmp(group, "VBBAD") == 0) group_vbbad(pfx);
    if (all || strcmp(group, "FB") == 0)    group_fb(pfx);
    if (all || strcmp(group, "VBS") == 0)   group_vbs(pfx);

    printf("\n=== tstfwrec: %d check(s) failed ===\n", bad);
    wtof("TSTFWREC VERDICT %s failed=%d", group, bad);

    return bad ? 8 : 0;
}
