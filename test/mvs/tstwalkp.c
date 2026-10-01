/*
 * tstwalkp.c - libc370 #80: __walkpd() on real PDS directories (MVS, batch).
 *
 * The host test (test/host/tstlspd.c) drives __walkpd() and __listpd() over
 * synthetic directory blocks.  This reads real ones, and only reads:
 *
 *   1. SYS1.MACLIB through __walkpd() and through __listpd(): the same
 *      number of members, the same first and last name
 *   2. a callback that stops after 10: __walkpd() returns 10
 *   3. a filter ("IEF*") through both: the same number
 *   4. a data set that does not exist: __walkpd() returns -1
 *   5. SYS1.SMPCDS, the directory #80 measured at 22982 members, through
 *      __walkpd() only: it allocates nothing, so the count arrives whatever
 *      the region -- __listpd() would calloc() one record per member
 *
 * Pass: every check below.  The counts are the measurement; keep the output.
 *
 * Measured on mvsdev, 2026-10-01 (JOB01076, CC 0): SYS1.MACLIB 742 members,
 * ABEND to XLATE, by walk and by list alike; 41 with "IEF*"; SYS1.SMPCDS
 * 23018 members walked in 1.4 s CPU without an allocation.  JOB01074 before
 * it found fopen() leaving errno 0 for a missing data set.
 *
 * BUILD (host):
 *     python3 sdk/mklibc.py build
 *     cc370 -O1 -std=gnu99 -Iinclude -L build/sdk test/mvs/tstwalkp.c \
 *           -flinker-output=iebcopy -o TSTWALKP
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ext/array.h>
#include <mvs/dslist.h>

static int fails;
#define CHECK(cond, msg) \
    do { if (cond) printf("  ok   %s\n", msg); \
         else { fails++; printf("  FAIL %s\n", msg); } } while (0)

typedef struct {
    int     seen;
    int     stop_at;
    char    first[9];
    char    last[9];
} WALK;

static void name_of(const PDSLIST *e, char out[9])
{
    int i;
    for (i = 0; i < 8 && e->name[i] != ' '; i++) out[i] = e->name[i];
    out[i] = 0;
}

static int walker(void *arg, const PDSLIST *entry)
{
    WALK *w = arg;
    if (!w->seen) name_of(entry, w->first);
    name_of(entry, w->last);
    w->seen++;
    return w->stop_at && w->seen >= w->stop_at;
}

static void both(const char *dsn, const char *filter)
{
    WALK        w   = { 0, 0, "", "" };
    PDSLIST     **pd;
    unsigned    n   = 0;
    char        first[9] = "", last[9] = "";
    int         rc;
    char        msg[80];

    printf("%s, filter %s\n", dsn, filter ? filter : "(none)");
    rc = __walkpd(dsn, filter, walker, &w);
    printf("  __walkpd: rc=%d, %d seen, first %s, last %s\n", rc, w.seen, w.first, w.last);
    pd = __listpd(dsn, filter);
    if (pd) {
        n = arraycount(&pd);
        if (n) { name_of(pd[0], first); name_of(pd[n-1], last); }
    }
    printf("  __listpd: %u records, first %s, last %s%s\n", n, first, last,
           pd ? "" : " (NULL)");
    snprintf(msg, sizeof msg, "%s: walk and list agree on the count", dsn);
    CHECK(rc >= 0 && (unsigned) rc == n && w.seen == rc, msg);
    snprintf(msg, sizeof msg, "%s: and on the first and last member", dsn);
    CHECK(strcmp(w.first, first) == 0 && strcmp(w.last, last) == 0, msg);
    if (pd) __freepd(&pd);
}

int main(void)
{
    WALK    w;
    int     rc;

    printf("TSTWALKP: __walkpd() on real directories (#80)\n");

    both("SYS1.MACLIB", NULL);
    both("SYS1.MACLIB", "IEF*");

    memset(&w, 0, sizeof w);
    w.stop_at = 10;
    rc = __walkpd("SYS1.MACLIB", NULL, walker, &w);
    printf("stop after 10: rc=%d, seen %d, last %s\n", rc, w.seen, w.last);
    CHECK(rc == 10 && w.seen == 10, "the callback stops the walk at 10");

    errno = 0;
    rc = __walkpd("IBMUSER.TSTWALKP.NOSUCH", NULL, walker, &w);
    printf("missing data set: rc=%d errno=%d\n", rc, errno);
    CHECK(rc == -1 && errno != 0, "a data set that does not exist: -1, errno set");

    memset(&w, 0, sizeof w);
    rc = __walkpd("SYS1.SMPCDS", NULL, walker, &w);
    printf("SYS1.SMPCDS: rc=%d, first %s, last %s\n", rc, w.first, w.last);
    CHECK(rc > 1000 && w.seen == rc, "SYS1.SMPCDS walked without storage");

    printf("TSTWALKP: %s\n", fails ? "FAILED" : "passed");
    return fails ? 8 : 0;
}
