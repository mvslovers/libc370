/*
 * tstlstnm.c - libc370 #61, #157, #158 on MVS: the list builders under a
 * storage shortage.  Convention (#61): on any allocation failure a list
 * builder frees what it built and returns NULL with errno ENOMEM - never a
 * short list that reads as complete - and clears errno on entry.
 *
 * __listvl() walks the UCBs and __listal() the DSABs, so neither runs on a
 * host; __listds() is pinned on the host too (test/host/tstlstds.c (11)).
 *
 * HOW THE SHORTAGE IS MADE: this program defines calloc() itself.  ld370
 * autocalls only what is still unresolved, so libc370's calloc.o is never
 * linked and every calloc() in the library lands here.  It counts only while
 * armed and fails the n-th call.  For each builder: one armed run without a
 * failure counts the callocs and the records, then n = 1 .. count + 1 fails
 * each call in turn.  Each result is one of
 *     FULL      the whole list (n lies past the builder's callocs)
 *     ENOMEM    NULL, errno ENOMEM
 *     NULL/e    NULL, another errno (a failure before the scan, e.g. in
 *               __listc()'s fopen() - a failure all the same)
 *     SHORT     a list with fewer records: the defect
 *     NULL/0    NULL with errno 0: a failure that reads as "empty"
 * and the last two fail the check.  Then an empty result with a stale errno:
 * NULL, errno 0.
 *
 * Built twice from this source: TSTLNM against this tree's libc.a, TSTLNMR
 * against the installed sysroot libc.a (before #61) - the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstlstnm.c -o TSTLNM -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstlstnm.c -o TSTLNMR -flinker-output=iebcopy
 *          ld370 --pack TSTLNM=TSTLNM.iebcopy TSTLNMR=TSTLNMR.iebcopy \
 *                -o tstlstnm -xmit --dsn IBMUSER.LIBC370.LNMSCR
 * Install: jcl/recvlnm.jcl.   Run: jcl/tstlstnm.jcl.
 *
 * mvsdev JOB01347, 2026-10-04 (RECEIVE JOB01346): GREEN CC 0000, 12/12 -
 * __listvl 16 volumes / 17 callocs, __listal 6 DDs / 19, __listds SYS2 11
 * records / 28, no SHORT and no NULL/0 at any failure point.  RED CC 0001,
 * 5 of 12 failed: 42 short lists (__listvl 15, __listal 15, __listds 12)
 * and a stale errno on an empty __listvl() and __listal().
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ext/array.h"
#include "mvs/dslist.h"

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

/* ---- the injected calloc ------------------------------------------------ */
static int armed;       /* count calls only while a builder runs */
static int calls;       /* callocs since armed */
static int fail_at;     /* fail this one (0: none) */

void *calloc(size_t n, size_t s)
{
    void *p;

    if (armed) {
        calls++;
        if (fail_at && calls == fail_at) {
            errno = ENOMEM;
            return NULL;
        }
    }
    if (s && n > 0xFFFFFFF8u / s) {
        errno = ENOMEM;
        return NULL;
    }
    p = malloc(n * s);
    if (p) memset(p, 0, n * s);
    return p;
}

/* ---- one builder behind a common shape ---------------------------------- */
typedef struct {
    const char *name;
    void      **(*make)(void);
    void       (*drop)(void ***);
} BUILDER;

static void **mk_vl(void)  { return (void **)__listvl(NULL, 0, NULL); }
static void   dr_vl(void ***l) { __freevl((VOLLIST ***)l); }
static void **mk_al(void)  { return (void **)__listal(NULL, NULL, 0); }
static void   dr_al(void ***l) { __freeal((ALCLIST ***)l); }
static void **mk_ds(void)  { return (void **)__listds("SYS2", "NONVSAM VOLUME", NULL); }
static void   dr_ds(void ***l) { __freeds((DSLIST ***)l); }

static void **run(const BUILDER *b, int n, int *count)
{
    void **l;

    fail_at = n;
    calls   = 0;
    errno   = EIO;          /* stale: the builder must set its own */
    armed   = 1;
    l = b->make();
    armed   = 0;
    fail_at = 0;
    *count  = l ? (int)arraycount(&l) : 0;
    return l;
}

static void one(const BUILDER *b)
{
    void **l;
    int    full, total, n, cnt, err;
    int    nfull = 0, nnomem = 0, nother = 0, nshort = 0, nzero = 0;
    char   msg[96];

    printf("%s\n", b->name);
    l = run(b, 0, &full);
    total = calls;
    if (l) b->drop(&l);
    printf("    baseline: %d records, %d callocs\n", full, total);
    sprintf(msg, "%s: a baseline to measure against", b->name);
    CHECK(full > 0 && total > 0, msg);
    if (full <= 0 || total <= 0) return;

    for (n = 1; n <= total + 1; n++) {
        l = run(b, n, &cnt);
        err = errno;
        if (l && cnt == full)  nfull++;
        else if (l) {
            nshort++;
            printf("    calloc %d failing: SHORT, %d of %d records, errno %d\n",
                   n, cnt, full, err);
        }
        else if (err == ENOMEM) nnomem++;
        else if (err == 0) {
            nzero++;
            printf("    calloc %d failing: NULL with errno 0\n", n);
        }
        else nother++;
        if (l) b->drop(&l);
    }
    printf("    %d failure points: FULL %d, ENOMEM %d, NULL/e %d, SHORT %d, "
           "NULL/0 %d\n", total + 1, nfull, nnomem, nother, nshort, nzero);
    sprintf(msg, "%s: never a short list", b->name);
    CHECK(nshort == 0, msg);
    sprintf(msg, "%s: never NULL with errno 0 on a failure", b->name);
    CHECK(nzero == 0, msg);
}

int main(void)
{
    static const BUILDER b[] = {
        { "__listvl(NULL, 0, NULL)",                 mk_vl, dr_vl },
        { "__listal(NULL, NULL, 0)",                 mk_al, dr_al },
        { "__listds(\"SYS2\", \"NONVSAM VOLUME\")",  mk_ds, dr_ds },
    };
    unsigned i;
    void   **l;

    printf("=== tstlstnm: list builders under a storage shortage "
           "(#61, #157, #158) ===\n\n");
    for (i = 0; i < sizeof(b) / sizeof(b[0]); i++) one(&b[i]);

    printf("\nempty results\n");
    errno = EIO;
    l = (void **)__listvl("ZZZZZ9", 0, NULL);
    CHECK(l == NULL && errno == 0, "__listvl() of no volume: NULL, errno 0");
    if (l) __freevl((VOLLIST ***)&l);
    errno = EIO;
    l = (void **)__listal(NULL, "NOSUCHDD", 0);
    CHECK(l == NULL && errno == 0, "__listal() of no DD: NULL, errno 0");
    if (l) __freeal((ALCLIST ***)&l);
    errno = EIO;
    l = (void **)__listpd("SYS1.PARMLIB", "ZZZZZZZ9");
    CHECK(l == NULL && errno == 0, "__listpd() of no member: NULL, errno 0");
    if (l) __freepd((PDSLIST ***)&l);

    printf("\n=== tstlstnm: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
