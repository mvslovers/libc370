/*
 * tstjesop.c - libc370 #108: jesopen() (src/jes/jesopen.c) must not hand back
 * a JES handle whose spool array never got allocated.
 *
 * ISSUE #108: line 54 of the pre-fix file called
 *
 *     arrayadd(&jes->js, js);
 *
 * with nothing in front of it.  Every other allocation in try_jesopen() is
 * checked and unwinds through jesclose(); this one is not.  When the array
 * cannot be allocated, arrayadd() returns -1, jes->js stays NULL, and the
 * function still stores a fully-formed-looking handle through *jespp - eye
 * catcher set, jes->cp populated.
 *
 * What the callers then do with it is the defect that reaches the operator.
 * Three of them index the array with no guard at all:
 *
 *     src/jes/jesjob.c:105    js = jes->js[0];
 *     src/jes/jesjob.c:497    HASPJS *js = jes->js[0];   (process_intxt)
 *     src/jes/jesprint.c:61   js = jes->js[0];
 *
 * On MVS that load SUCCEEDS.  Low-address protection stops stores into page
 * zero, not fetches, so jes->js[0] reads the PSA and hands back a value that
 * is not NULL.  __jsrd4() therefore walks straight past its own
 *
 *     if (!js) goto quit;
 *
 * and reaches "dcb = js->dcb" followed by "dcb->dcbblksi = buflen" - a store
 * through a pointer fetched out of the nucleus, from problem state, key 8.
 * That is where the S0C4 comes from: not from the failed allocation, from the
 * handle that was returned as if it had succeeded.  Same shape as #61 and
 * #80 - the allocation is not the problem, the unchecked result is.
 *
 * THIS TEST LINKS AND EXECUTES THE REAL jesopen.c, together with the real
 * jesclose.c and the real array code.  Nothing is mirrored, so there is no
 * copy to keep in sync.  What it does NOT link is MVS: __cpopen/__jsopen and
 * their close halves are host shims below, which is what makes jesopen()
 * testable at all - it is otherwise four MVS services in a trench coat.
 *
 * ====================================================================
 * WHAT THIS PINS - AND WHAT IT DOES NOT
 * --------------------------------------------------------------------
 * - The failure is injected at calloc, not at arrayadd, so the real
 *   @@aradd.c/@@arnew.c pair runs and really fails.  Stubbing arrayadd()
 *   itself would have pinned the caller against a mock and proved nothing
 *   about the library's own failure return.
 *
 * - The injection point is the SECOND calloc of the call.  Call 1 is
 *   try_jesopen's own calloc(1, sizeof(JES)); call 2 is arraynew(20) inside
 *   arrayadd().  Case (5) fails call 2 and nothing else - that is the #108
 *   path.  Case (2) fails call 1, which the pre-fix source already handled;
 *   it is here as a regression guard, not as a new check.
 *
 * - This is HOST semantics for a storage shortage.  On the target the same
 *   path is reached differently: calloc() -> malloc() -> __getm() issues
 *   GETMAIN RC since #81 (asm/@@getm.asm:66) and returns NULL, so the route
 *   exists - but a real shortage on MVS 3.8j is not reproducible from batch
 *   (LSQA is fenced), which is why the check lives here and not in
 *   test/mvs/.  What this proves is that jesopen() now REPORTS the failure
 *   instead of returning a half-built handle, not that MVS will take that
 *   route to get there.
 *
 * - try() is shimmed to a plain indirect call.  On the target it is an ESTAE
 *   wrapper (___try, src/clib/@@@try.c); here it only has to deliver the
 *   arguments, because no case in this file abends.  The ESTAE behaviour is
 *   #89's territory and is pinned by test/mvs/tstcrtlk.c.
 *
 * - The leak checks count __cpclos/__jsclos calls.  They pin that the failure
 *   path CLOSES what it opened; they do not pin that it frees every byte.
 *   ASAN does that half, on the build line below.
 * ====================================================================
 *
 * BUILD / RUN (host, from test/host).  Four defines carry the whole port and
 * none of them is guessable:
 *
 *   -D'__asm__(x)='   erases the file-scope S/370 assembler statements
 *                     (__asm__("\n&FUNC SETC 'try_jesopen'")) that the host
 *                     assembler rejects.  It does not touch the asm("@@ARADD")
 *                     labels in clibary.h - different spelling - so the host
 *                     symbols keep the library's names.  It does NOT reach
 *                     clibstr.h's inline memset() either, which is spelled
 *                     "__asm__ __volatile__(" - a function-like macro only
 *                     expands when the next token is an open paren.  Hence no
 *                     memset() anywhere in this file.
 *   -D__32BIT__       is what libc370's own stddef.h/stdlib.h key size_t off.
 *   -U__LP64__        libc370's time64.h is "#error Your time_t is already
 *                     64-bit" under __LP64__, and clibjes2.h pulls it in for
 *                     JESJOB.start_time64.  Undefining it selects the LP32
 *                     branch, which is the one the target compiles.  Nothing
 *                     in jesopen() touches a time value.
 *   -Dcalloc=...      the injection point.  It must reach jesopen.c and
 *                     @@arnew.c and NOTHING else, which is why those two are
 *                     compiled on their own lines.
 *
 *     R=../..
 *     cc -std=gnu99 -U__LP64__ -D'__asm__(x)=' -D__32BIT__ \
 *        -Dcalloc=tst_calloc -I $R/include \
 *        -c "$R/src/jes/jesopen.c" -o jesopen.o
 *     cc -std=gnu99 -U__LP64__ -D'__asm__(x)=' -D__32BIT__ \
 *        -Dcalloc=tst_calloc -I $R/include \
 *        -c "$R/src/clib/@@arnew.c" -o arnew.o
 *     cc -std=gnu99 -U__LP64__ -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(x)=' -D__32BIT__ -I $R/include \
 *        -o t tstjesop.c jesopen.o arnew.o \
 *        "$R/src/jes/jesclose.c" "$R/src/clib/@@aradd.c" \
 *        "$R/src/clib/@@arcou.c" "$R/src/clib/@@arget.c" \
 *        "$R/src/clib/@@arfre.c"
 *     ./t                                             # 18/18, rc 0
 *
 * ASAN here buys memory-error checking, not leak checking: LeakSanitizer is
 * not supported on macOS arm64 (detect_leaks=1 aborts).  Run it on Linux if
 * you want the leak side too.
 *
 * RED, against the pre-fix source:
 *
 *     git show <pre-fix-rev>:src/jes/jesopen.c > /tmp/oldjesopen.c
 *     ... same lines with /tmp/oldjesopen.c in place of $R/src/jes/jesopen.c,
 *         plus -Wno-error=implicit-function-declaration ...
 *     ./t                                             # 13/18, 5 failures
 *
 * The five that fail are cases (5) and (6): the pre-fix jesopen() returns a
 * non-NULL handle whose js array is NULL, leaves both data sets open, and
 * says nothing on the console.
 *
 * That extra flag is part of the story too.  The pre-fix file called wtof()
 * with no prototype in scope - it included neither clib.h nor clibwto.h - so
 * the compiler invented the signature, which on this target decides linkage
 * (#39).  The fix adds the include, so the fixed file needs no such flag:
 * one more of #39's translation units off the list for free.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "clibjes2.h"
#include "clibary.h"

/* --------------------------------------------------------------------------
 * Minimal mbtcheck.h-compatible harness, same inline copy as tsttxdsn.c.
 * ------------------------------------------------------------------------ */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define CHECK_EQ(got, want, msg)                                          \
    do {                                                                  \
        long g_ = (long)(got), w_ = (long)(want);                         \
        mbt_run++;                                                        \
        if (g_ == w_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }    \
        else { mbt_failed++;                                              \
               printf("  FAIL: %s (got %ld, want %ld)\n", (msg), g_, w_); }\
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * Host shims.
 *
 * The four MVS services jesopen()/jesclose() reach for, plus the console and
 * errno.  Each open half can be told to fail; each close half counts, so a
 * case can assert that the unwind actually released what it took.
 * ------------------------------------------------------------------------ */
static int cp_open_fails = 0;   /* next __cpopen() returns NULL             */
static int js_open_fails = 0;   /* next __jsopen() returns NULL             */
static int cp_closes     = 0;   /* __cpclos() call count                    */
static int js_closes     = 0;   /* __jsclos() call count                    */
static int wtos          = 0;   /* wtof() call count                        */

/* Storage for the two fakes.  Static, so a leaked handle is a counter that
 * does not come back to zero rather than a free() of something ASAN owns.
 * Static also means already zeroed - deliberately not memset(), because
 * libc370's clibstr.h inlines memset() as S/370 assembler written
 * "__asm__ __volatile__", which the -D'__asm__(x)=' erase does not reach
 * (the macro only expands when followed by an open paren). */
static HASPCP  fake_cp;
static HASPJS  fake_js;

HASPCP *__cpopen(const char *dataset)
{
    (void)dataset;
    if (cp_open_fails) return NULL;
    return &fake_cp;
}

int __cpclos(HASPCP *cp)
{
    if (cp) cp_closes++;
    return 0;
}

HASPJS *__jsopen(const char *dataset)
{
    (void)dataset;
    if (js_open_fails) return NULL;
    return &fake_js;
}

int __jsclos(HASPJS *js)
{
    if (js) js_closes++;
    return 0;
}

void wtof(const char *text, ...)
{
    (void)text;
    wtos++;
}

int *__errno(void)
{
    static int e;
    return &e;
}

/* try() on the host: deliver the one argument and call.  No ESTAE, no abend
 * handling - see the header note.  jesopen() is the only caller here and it
 * passes exactly one pointer. */
int ___try(void *func, ...)
{
    void      (*fn)(void *);
    void       *arg;
    va_list     ap;

    va_start(ap, func);
    arg = va_arg(ap, void *);
    va_end(ap);

    fn = (void (*)(void *))func;
    fn(arg);
    return 0;
}

/* calloc as seen by jesopen.c and @@arnew.c (-Dcalloc=tst_calloc).  fail_at
 * counts DOWN: 0 = pass everything through, n = fail the n-th call from now
 * on.  Call 1 of jesopen() is the JES handle, call 2 is arraynew(20). */
static int fail_at = 0;

void *tst_calloc(size_t nmemb, size_t size)
{
    if (fail_at > 0 && --fail_at == 0) return NULL;
    return calloc(nmemb, size);
}

/* --------------------------------------------------------------------------
 * Every case starts from the same state - counters zeroed, both opens armed
 * to succeed, no calloc failure pending.
 * ------------------------------------------------------------------------ */
static void reset(void)
{
    cp_open_fails = js_open_fails = 0;
    cp_closes = js_closes = wtos = 0;
    fail_at = 0;
}

int main(void)
{
    JES *jes;

    printf("=== tstjesop: jesopen() must not return a half-built handle "
           "(#108) ===\n\n");

    /* ------------------------------------------------------------------
     * (1) Happy path.  Establishes what a GOOD handle looks like, so the
     * later cases are asserting against something rather than nothing.
     * ---------------------------------------------------------------- */
    printf("(1) both data sets open:\n");
    reset();
    jes = jesopen();
    CHECK(jes != NULL,            "(1) handle returned");
    if (jes) {
        CHECK(jes->cp == &fake_cp,       "(1) checkpoint handle stored");
        CHECK(jes->js != NULL,           "(1) spool array allocated");
        CHECK_EQ(arraycount(&jes->js), 1, "(1) one spool handle in array");
        CHECK(arrayget(&jes->js, 1) == &fake_js,
                                         "(1) it is the one __jsopen gave");
        jesclose(&jes);
        CHECK(jes == NULL,               "(1) jesclose() nulls the handle");
    }

    /* ------------------------------------------------------------------
     * (2) The JES handle itself cannot be allocated.  Pre-fix source
     * already handled this one; it is a regression guard.
     * ---------------------------------------------------------------- */
    printf("\n(2) calloc fails on the JES handle:\n");
    reset();
    fail_at = 1;                        /* fail call 1: the JES handle     */
    jes = jesopen();
    CHECK(jes == NULL,          "(2) no handle returned");
    CHECK_EQ(cp_closes, 0,      "(2) checkpoint never opened, never closed");
    CHECK_EQ(js_closes, 0,      "(2) spool never opened, never closed");

    /* ------------------------------------------------------------------
     * (3) The checkpoint data set will not open.
     * ---------------------------------------------------------------- */
    printf("\n(3) checkpoint_open() fails:\n");
    reset();
    cp_open_fails = 1;
    jes = jesopen();
    CHECK(jes == NULL,          "(3) no handle returned");
    CHECK_EQ(js_closes, 0,      "(3) spool never opened, never closed");

    /* ------------------------------------------------------------------
     * (4) The spool data set will not open.  The checkpoint is already
     * open at that point and has to come back down.
     * ---------------------------------------------------------------- */
    printf("\n(4) spool_open() fails:\n");
    reset();
    js_open_fails = 1;
    jes = jesopen();
    CHECK(jes == NULL,          "(4) no handle returned");
    CHECK_EQ(cp_closes, 1,      "(4) the open checkpoint was closed");

    /* ------------------------------------------------------------------
     * (5) #108.  Both data sets open, then the spool ARRAY cannot be
     * allocated.  Pre-fix: a non-NULL handle with a NULL js array, and
     * both data sets left open.  This is the case that reaches the
     * operator as an S0C4 one jes->js[0] later.
     * ---------------------------------------------------------------- */
    printf("\n(5) arrayadd() cannot allocate the spool array (#108):\n");
    reset();
    fail_at = 2;                        /* fail call 2: arraynew(20)       */
    jes = jesopen();
    CHECK(jes == NULL,          "(5) no handle returned, NOT a NULL js array");
    CHECK_EQ(cp_closes, 1,      "(5) the open checkpoint was closed");
    CHECK_EQ(js_closes, 1,      "(5) the open spool data set was closed");

    /* ------------------------------------------------------------------
     * (6) The failure is reported, not swallowed.  jesopen()'s siblings
     * all wtof() before they unwind; #108's path must too, or the only
     * trace left of a storage shortage is a 500 with no cause.
     * ---------------------------------------------------------------- */
    printf("\n(6) the failure reaches the console:\n");
    reset();
    fail_at = 2;
    jes = jesopen();
    CHECK(wtos > 0,             "(6) wtof() was called on the #108 path");
    CHECK_EQ(jes, 0,            "(6) and the handle is still NULL");

    return mbt_test_summary("tstjesop");
}
