/*
 * tstjesop.c - libc370 #108: jesopen() (src/mvs/jes2/jesopen.c) must not hand back
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
 *     src/mvs/jes2/jesjob.c:105    js = jes->js[0];
 *     src/mvs/jes2/jesjob.c:497    HASPJS *js = jes->js[0];   (process_intxt)
 *     src/mvs/jes2/jesprint.c:61   js = jes->js[0];
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
 * jesclose.c, jesjobfr.c, jesjobf1.c and the real array code.  Nothing is
 * mirrored, so there is no copy to keep in sync.  What it does NOT link is
 * MVS: __cpopen/__jsopen and their close halves are host shims below, which
 * is what makes jesopen() testable at all - it is otherwise four MVS
 * services in a trench coat.
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
 *   GETMAIN RC since #81 (src/mvs/storage/@@getm.asm:66) and returns NULL, so the route
 *   exists - but a real shortage on MVS 3.8j is not reproducible from batch
 *   (LSQA is fenced), which is why the check lives here and not in
 *   test/mvs/.  What this proves is that jesopen() now REPORTS the failure
 *   instead of returning a half-built handle, not that MVS will take that
 *   route to get there.
 *
 * - try() is shimmed to a plain indirect call.  On the target it is an ESTAE
 *   wrapper (___try, src/mvs/recovery/@@@try.c); here it only has to deliver the
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
 *                     labels in ext/array.h - different spelling - so the host
 *                     symbols keep the library's names.  It does NOT reach
 *                     string.h's inline memset() either, which is spelled
 *                     "__asm__ __volatile__(" - a function-like macro only
 *                     expands when the next token is an open paren.  Hence no
 *                     memset() anywhere in this file.
 *   -D__32BIT__       is what libc370's own stddef.h/stdlib.h key size_t off.
 *   -U__LP64__        libc370's ext/time64.h is "#error Your time_t is already
 *                     64-bit" under __LP64__, and mvs/jes2.h pulls it in for
 *                     JESJOB.start_time64.  Undefining it selects the LP32
 *                     branch, which is the one the target compiles.  Nothing
 *                     in jesopen() touches a time value.
 *   -Dcalloc=...      the injection point.  It must reach jesopen.c and
 *                     @@arnew.c and NOTHING else, which is why those two are
 *                     compiled on their own lines.
 *   -Dfree=...        the counter for case (7), on jesjobf1.c alone: the
 *                     frees it makes are the job list jesclose() releases
 *                     through jesjobfr() since #126.
 *
 *     R=../..
 *     cc -std=gnu99 -U__LP64__ -D'__asm__(x)=' -D__32BIT__ \
 *        -Dcalloc=tst_calloc -I $R/include -I $R/src/internal \
 *        -c "$R/src/mvs/jes2/jesopen.c" -o jesopen.o
 *     cc -std=gnu99 -U__LP64__ -D'__asm__(x)=' -D__32BIT__ \
 *        -Dcalloc=tst_calloc -I $R/include -I $R/src/internal \
 *        -c "$R/src/ext/array/@@arnew.c" -o arnew.o
 *     cc -std=gnu99 -U__LP64__ -D'__asm__(x)=' -D__32BIT__ \
 *        -Dfree=tst_free -I $R/include -I $R/src/internal \
 *        -c "$R/src/mvs/jes2/jesjobf1.c" -o jesjobf1.o
 *     cc -std=gnu99 -U__LP64__ -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(x)=' -D__32BIT__ -I $R/include -I $R/src/internal \
 *        -o t tstjesop.c jesopen.o arnew.o jesjobf1.o \
 *        "$R/src/mvs/jes2/jesclose.c" "$R/src/mvs/jes2/jesjobfr.c" \
 *        "$R/src/ext/array/@@aradd.c" "$R/src/ext/array/@@arcou.c" \
 *        "$R/src/ext/array/@@arget.c" "$R/src/ext/array/@@arfre.c"
 *     ./t                                             # 20/20, rc 0
 *
 * ASAN here buys memory-error checking, not leak checking: LeakSanitizer is
 * not supported on macOS arm64 (detect_leaks=1 aborts).  Run it on Linux if
 * you want the leak side too.
 *
 * RED, against the pre-fix source:
 *
 *     git show <pre-fix-rev>:src/mvs/jes2/jesopen.c > /tmp/oldjesopen.c
 *     ... same lines with /tmp/oldjesopen.c in place of $R/src/mvs/jes2/jesopen.c,
 *         plus -Wno-error=implicit-function-declaration ...
 *     ./t                                             # 15/20, 5 failures
 *
 * The five that fail are cases (5) and (6): the pre-fix jesopen() returns a
 * non-NULL handle whose js array is NULL, leaves both data sets open, and
 * says nothing on the console.
 *
 * That extra flag is part of the story too.  The pre-fix file called wtof()
 * with no prototype in scope - it included neither clib.h nor mvs/wto.h - so
 * the compiler invented the signature, which on this target decides linkage
 * (#39).  The fix adds the include, so the fixed file needs no such flag:
 * one more of #39's translation units off the list for free.
 *
 * #142, cases (8)-(13): where the data sets come from.  get_dsab(),
 * __dsalcf() and __jesvol() are shims that record what they were asked, so
 * the cases pin the order: the DDs of the JCL when the step has them, else
 * SYS1.HASPCKPT through the catalog, else on the volume the OBTAIN scan
 * found; the spool as PREFIX.HASPACE on the volume the checkpoint names;
 * jesopen_ex() with a name uses no DD.  RED, the same build with the
 * pre-#142 jesopen.c plus a jesopen_ex() that calls jesopen(): 31/46, the
 * 15 checks of (9)-(13) fail.  What MVS does with these requests is
 * test/mvs/tstjsop.c's part.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "mvs/jes2.h"
#include "mvs/dd.h"
#include "ext/array.h"
#include "src/mvs/jes2/jesvol.h"

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
 * libc370's string.h inlines memset() as S/370 assembler written
 * "__asm__ __volatile__", which the -D'__asm__(x)=' erase does not reach
 * (the macro only expands when followed by an open paren). */
static HASPCP  fake_cp;
static HASPJS  fake_js;

/* What the opens and allocations were asked for (#142).  A case checks
 * the names, which is where the discovery order shows. */
static char cp_arg[64];         /* last __cpopen() argument                 */
static char js_arg[64];         /* last __jsopen() argument                 */
static char alc[4][96];         /* __dsalcf() requests, formatted           */
static int  nalc          = 0;
static int  cat_fails     = 0;  /* __dsalcf() without volser= fails         */
static int  vol_fails     = 0;  /* __dsalcf() with volser= fails            */
static int  scans         = 0;  /* __jesvol() call count                    */
static const char *scan_hit = NULL; /* volume __jesvol() finds, NULL none   */
static int  dd_ckpt       = 0;  /* the step has a HASPCKPT DD               */
static int  dd_spool      = 0;  /* the step has a HASPACE1 DD               */
static DSAB fake_dsab;

HASPCP *__cpopen(const char *dataset)
{
    snprintf(cp_arg, sizeof(cp_arg), "%s", dataset);
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
    snprintf(js_arg, sizeof(js_arg), "%s", dataset);
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

DSAB *get_dsab(void *tcbptr, const char *ddname)
{
    (void)tcbptr;
    if (dd_ckpt  && strcmp(ddname, "HASPCKPT") == 0) return &fake_dsab;
    if (dd_spool && strcmp(ddname, "HASPACE1") == 0) return &fake_dsab;
    return NULL;
}

int __dsalcf(char *ddname, const char *opts, ...)
{
    va_list ap;
    char   *req = alc[nalc < 4 ? nalc : 3];

    va_start(ap, opts);
    vsnprintf(req, sizeof(alc[0]), opts, ap);
    va_end(ap);
    nalc++;
    if (strstr(req, "volser=") ? vol_fails : cat_fails) return 4;
    snprintf(ddname, 9, "SYS%05d", nalc);
    return 0;
}

int __jesvol(const char *dsn, char volser[7], unsigned *scanned)
{
    (void)dsn;
    scans++;
    if (scanned) *scanned = 3;
    if (!scan_hit) return 1;
    snprintf(volser, 7, "%s", scan_hit);
    return 0;
}

/* toupper() of libc370's ctype.h indexes this table: the host one */
static short up[257];
short *__toup = &up[1];

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

/* free as seen by jesjobf1.c (-Dfree=tst_free): counts what the job-list
 * teardown releases, case (7). */
static int job_frees = 0;

void tst_free(void *p)
{
    job_frees++;
    free(p);
}

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
    job_frees = 0;
    cp_arg[0] = js_arg[0] = 0;
    nalc = cat_fails = vol_fails = scans = 0;
    scan_hit = NULL;
    dd_ckpt = dd_spool = 1;     /* (1)-(7): the DDs of the JCL, as before */
    memcpy(fake_cp.hct._SPOOL, "SPOOL1", 6);
}

/* (8)-(13) - #142: where the data sets come from */
static void discovery(void)
{
    JES *jes;
    int  c;

    for (c = 0; c < 256; c++)
        up[c + 1] = (short)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);

    printf("\n(8) both DDs in the JCL: they are used, nothing allocated:\n");
    reset();
    jes = jesopen();
    CHECK(jes != NULL,                              "(8) handle returned");
    CHECK(strcmp(cp_arg, "DD:HASPCKPT") == 0,       "(8) checkpoint from DD");
    CHECK(strcmp(js_arg, "DD:HASPACE1") == 0,       "(8) spool from DD");
    CHECK_EQ(nalc, 0,                               "(8) no allocation");
    jesclose(&jes);

    printf("\n(9) no DD, SYS1.HASPCKPT cataloged:\n");
    reset();
    dd_ckpt = dd_spool = 0;
    jes = jesopen();
    CHECK(jes != NULL,                              "(9) handle returned");
    CHECK(strcmp(alc[0], "dsname=SYS1.HASPCKPT,disp=shr") == 0,
                                                    "(9) catalog first");
    CHECK(strcmp(cp_arg, "DD:SYS00001") == 0,       "(9) opened by its DD");
    CHECK_EQ(scans, 0,                              "(9) no volume scan");
    CHECK(strcmp(alc[1], "dsname=SYS1.HASPACE,disp=shr,volser=SPOOL1") == 0,
                                                    "(9) spool on _SPOOL");
    CHECK(strcmp(js_arg, "DD:SYS00002") == 0,       "(9) spool by its DD");
    jesclose(&jes);

    printf("\n(10) no DD, not cataloged: the OBTAIN scan finds it:\n");
    reset();
    dd_ckpt = dd_spool = 0;
    cat_fails = 1;
    scan_hit = "MVS000";
    jes = jesopen();
    CHECK(jes != NULL,                              "(10) handle returned");
    CHECK_EQ(scans, 1,                              "(10) one volume scan");
    CHECK(strcmp(alc[1], "dsname=SYS1.HASPCKPT,disp=shr,volser=MVS000") == 0,
                                                    "(10) allocated on it");
    CHECK(strcmp(cp_arg, "DD:SYS00002") == 0,       "(10) opened by its DD");
    jesclose(&jes);

    printf("\n(11) no DD, found nowhere: NULL and both causes said:\n");
    reset();
    dd_ckpt = dd_spool = 0;
    cat_fails = 1;
    jes = jesopen();
    CHECK(jes == NULL,                              "(11) no handle");
    CHECK(cp_arg[0] == 0,                           "(11) nothing opened");
    CHECK_EQ(wtos, 3,                       "(11) not found + two causes");

    printf("\n(12) jesopen_ex() with a name: the DDs are not used:\n");
    reset();
    jes = jesopen_ex("jes2b.haspckpt");
    CHECK(jes != NULL,                              "(12) handle returned");
    CHECK(strcmp(alc[0], "dsname=JES2B.HASPCKPT,disp=shr") == 0,
                                                    "(12) the name, upper");
    CHECK(strcmp(alc[1], "dsname=JES2B.HASPACE,disp=shr,volser=SPOOL1") == 0,
                                                    "(12) spool, same prefix");
    CHECK(strcmp(js_arg, "DD:SYS00002") == 0,       "(12) not DD:HASPACE1");
    jesclose(&jes);
    reset();
    jes = jesopen_ex(NULL);
    CHECK(strcmp(cp_arg, "DD:HASPCKPT") == 0,       "(12) NULL = jesopen()");
    jesclose(&jes);
    reset();
    jes = jesopen_ex("");
    CHECK(jes == NULL && cp_arg[0] == 0,            "(12) \"\" refused");

    printf("\n(13) HASPCKPT DD only: spool SYS1.HASPACE on _SPOOL:\n");
    reset();
    dd_spool = 0;
    jes = jesopen();
    CHECK(jes != NULL,                              "(13) handle returned");
    CHECK(strcmp(cp_arg, "DD:HASPCKPT") == 0,       "(13) checkpoint from DD");
    CHECK(strcmp(alc[0], "dsname=SYS1.HASPACE,disp=shr,volser=SPOOL1") == 0,
                                                    "(13) spool allocated");
    jesclose(&jes);
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

    /* ------------------------------------------------------------------
     * (7) #126.  A jesjob() walk that ended in an abend leaves its job
     * list anchored in the handle, and recovery has nothing but the
     * handle.  jesclose() must release it through jesjobfr().
     * ---------------------------------------------------------------- */
    printf("\n(7) jesclose() frees a job list left in the handle (#126):\n");
    reset();
    jes = jesopen();
    CHECK(jes != NULL,          "(7) handle returned");
    if (jes) {
        JESJOB *job = calloc(1, sizeof(JESJOB));
        JESDD  *dd  = calloc(1, sizeof(JESDD));

        arrayadd(&job->jesdd, dd);
        arrayadd(&jes->injobs, job);
        jesclose(&jes);
        CHECK_EQ(job_frees, 2,  "(7) the job and its one DD freed");
    }

    discovery();

    return mbt_test_summary("tstjesop");
}
