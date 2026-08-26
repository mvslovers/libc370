/*
 * tstfcls.c - libc370 #147 item 1: fclose() runs its whole teardown
 * under the FILE lock.
 *
 * ISSUE #147: fclose.c flushed, __aclose()'d the DCB and freed the
 * buffer and the FILE with NO hold on the FILE lock - only the
 * grt->grtfile array removal was serialized.  Since #146 the flush
 * itself is atomic, but the sequence around it is not: a concurrent
 * vfprintf() on the same FILE can acquire the lock between the flush
 * and __aclose()/free() and then PUT on a closed DCB or write through
 * a freed buffer pointer.  The fix takes the lock at the top (#146's
 * owned pattern, calling __fflush() directly) and releases it after
 * the FILE has left grtfile - unlock() after free(fp) is safe because
 * the lock rname is built from the pointer VALUE, nothing dereferences
 * it.  A waiter that acquires after the close and then uses the freed
 * FILE remains a caller error; no lock can fix that.
 *
 * This test compiles the REAL fclose.c (and fflush.c, which the
 * pre-fix path calls) with the real array TUs, a two-resource lock
 * model (the FILE and &grt->grtfile), and shims for the MVS services
 * that record whether the FILE lock was held at the moment each
 * teardown step ran:
 *
 *   1. the flush runs under the FILE hold (invariant: red gets it
 *      from public fflush(), green from fclose's own hold);
 *   2. __aclose() runs under the FILE hold        - RED: it does not;
 *   3. free() of the buffer runs under the hold   - RED: it does not;
 *   4. __fpfree() runs under the hold             - RED: it does not;
 *   5. free() of the FILE itself runs under the hold - RED: it does not;
 *   6. the FILE leaves grtfile, the grtfile lock is taken exactly
 *      once, and at the end no lock is left held.
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tstvsnp.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tstfcls.c \
 *        "$R/src/clib/@@aradd.c" "$R/src/clib/@@arnew.c" \
 *        "$R/src/clib/@@arcou.c" "$R/src/clib/@@ardel.c" \
 *        "$R/src/clib/@@arfre.c" "$R/src/clib/@@arget.c" && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clibcrt.h"
#include "cliblock.h"

/* ---- two-resource lock model (ENQ/DEQ RET=HAVE, per address) --------- */

#define MAXLK   8

/* the hold stack: what is locked right now */
static void     *hold[MAXLK];
static int      hold_n;

/* the acquire ledger: how often each resource was ever locked */
static void     *aq_thing[MAXLK];
static int      aq_count[MAXLK];
static int      aq_n;

static int lk_held(void *thing)
{
    int     i;

    for (i = 0; i < hold_n; i++) {
        if (hold[i] == thing) return 1;
    }
    return 0;
}

static int lk_acquires(void *thing)
{
    int     i;

    for (i = 0; i < aq_n; i++) {
        if (aq_thing[i] == thing) return aq_count[i];
    }
    return 0;
}

int lock(void *thing, int read)
{
    int     i;

    (void)read;
    if (lk_held(thing)) return 8;   /* you already have the lock         */
    if (hold_n < MAXLK) hold[hold_n++] = thing;
    for (i = 0; i < aq_n; i++) {
        if (aq_thing[i] == thing) { aq_count[i]++; return 0; }
    }
    if (aq_n < MAXLK) { aq_thing[aq_n] = thing; aq_count[aq_n] = 1; aq_n++; }
    return 0;
}

int unlock(void *thing, int read)
{
    int     i;

    (void)read;
    for (i = 0; i < hold_n; i++) {
        if (hold[i] == thing) {
            hold[i] = hold[--hold_n];
            return 0;
        }
    }
    return 8;                       /* you didn't have the lock          */
}

/* ---- recording shims -------------------------------------------------- */

static FILE     *watchfp;           /* the FILE under close              */
static int      held_at_fflush = -1;
static int      held_at_aclose = -1;
static int      held_at_freebuf = -1;
static int      held_at_fpfree = -1;
static int      held_at_freefp = -1;

int __fflush(FILE *fp)
{
    held_at_fflush = lk_held(fp);
    return 0;
}

void __aclose(void *handle)
{
    (void)handle;
    held_at_aclose = lk_held(watchfp);
}

int __fpfree(FILE *fp)
{
    held_at_fpfree = lk_held(fp);
    return 0;
}

static CLIBGRT  fakegrt;

CLIBGRT *__grtget(void)
{
    return &fakegrt;
}

int *__errno(void) { static int e; return &e; }

/* free() inside fclose.c only: record what it frees and under what */
static void tst_free(void *p)
{
    if (p == watchfp) held_at_freefp = lk_held(watchfp);
    else if (watchfp && p == watchfp->buf) held_at_freebuf = lk_held(watchfp);
    free(p);
}

#define free tst_free
#include "../../src/clib/fclose.c"
#undef free

#include "../../src/clib/fflush.c"

/* ---- harness ---------------------------------------------------------- */

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK_EQ(got, want, msg)                                           \
    do {                                                                   \
        mbt_run++;                                                         \
        if ((got) == (want)) { mbt_passed++; printf("  PASS: %s\n", (msg)); } \
        else { mbt_failed++;                                               \
               printf("  FAIL: %s (got %d, want %d)\n",                    \
                      (msg), (int)(got), (int)(want)); }                   \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

int main(void)
{
    FILE        *fp;
    static char dummydcb[8];
    int         i;

    printf("=== tstfcls: fclose() teardown under the FILE lock "
           "(#147 item 1) ===\n\n");

    /* a FILE the way fopen() leaves it: open, writable, dynamic,
       registered in grt->grtfile */
    fp = calloc(1, sizeof(_FILE));
    for (i = 0; _FILE_EYE[i]; i++) fp->eye[i] = _FILE_EYE[i];
    fp->flags  = _FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_DYNAMIC;
    fp->dcb    = dummydcb;
    fp->asmbuf = dummydcb;
    fp->buf    = malloc(64);
    fp->upto   = fp->buf;
    fp->endbuf = fp->buf + 64;
    arrayadd(&fakegrt.grtfile, fp);
    watchfp = fp;

    CHECK_EQ((int)arraycount(&fakegrt.grtfile), 1, "FILE registered");

    fclose(fp);

    CHECK_EQ(held_at_fflush, 1,  "(1) flush under the FILE hold");
    CHECK_EQ(held_at_aclose, 1,  "(2) __aclose under the FILE hold");
    CHECK_EQ(held_at_freebuf, 1, "(3) buffer free under the FILE hold");
    CHECK_EQ(held_at_fpfree, 1,  "(4) __fpfree under the FILE hold");
    CHECK_EQ(held_at_freefp, 1,  "(5) FILE free under the FILE hold");

    CHECK_EQ((int)arraycount(&fakegrt.grtfile), 0, "(6) FILE left grtfile");
    CHECK_EQ(lk_acquires(&fakegrt.grtfile), 1, "(6) grtfile lock taken once");
    CHECK_EQ(hold_n, 0, "(6) no lock left held at the end");

    return mbt_test_summary("tstfcls");
}
