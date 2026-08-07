/*
 * tsttxdsn.c - libc370 #60: __txdsn() (src/clib/@@txdsn.c) must build its
 * text units without writing anything to the operator console, and must not
 * hand a NULL text unit to arrayadd().
 *
 * ISSUE #60: line 30 of the pre-fix file called
 *
 *     wtodumpf(txmem, sizeof(TXT99)+len, "%s: DALMEMBR", __func__);
 *
 * with no `if` in front of it, on the ORDINARY path.  Every allocation of a
 * data set name carrying a member - __dsalc("dsn=SYS1.MACLIB(IEFZB4D0);..."),
 * and so every fopen("DD:x(member)") that goes through dynamic allocation -
 * put a hex dump of the DALMEMBR text unit on the console.  Not on failure;
 * on success.  On MVS 3.8j the console IS the SYSLOG (#4).  Same class as
 * #43, but with no judgement call attached: there is no reading of "the
 * caller decides what to report" in which the library dumps a control block
 * on a path where nothing went wrong.
 *
 * The second defect on the same line: txmem was dumped BEFORE it was checked,
 * and then passed to arrayadd() unchecked.  With a failed calloc that is
 * wtodumpf(NULL, ...) - a dump of low storage that reads like a plausible
 * control block - followed by a NULL element stored in the text unit array.
 * __dsalc() ORs the high-order bit into the last element and hands the array
 * to SVC 99 (@@dsalc.c:236), so a NULL element is a text unit at address 0.
 * The DALDSNAM unit on the old line 35 was passed to arrayadd() unchecked in
 * exactly the same way; both are checked now.
 *
 * THIS TEST LINKS AND EXECUTES THE REAL @@txdsn.c, together with the real
 * @@nwtx99.c and the real array code - no mirror to keep in sync (unlike
 * test/host/tstcmtt.c, and for the same reason test/host/tstjesprb.c can do
 * it: __txdsn() is string in, text units out.  No SVC 99, no MVS service).
 *
 * ====================================================================
 * WHAT THIS PINS - AND WHAT IT DOES NOT
 * --------------------------------------------------------------------
 * - The console check is "wtodumpf() is not called".  It works because this
 *   file DEFINES wtodumpf() and counts the calls; the pre-fix source calls it
 *   once per member DSN, the fixed source never links it at all.  That is a
 *   red->green gate for THIS defect and nothing wider: a future wtof() in
 *   __txdsn() would sail straight past it.  The durable target-side check is
 *   the one PR #57 used for #43 - build and confirm the generated
 *   src/clib/@@txdsn.s no longer references WTODUMPF.
 *
 * - Cases (6) and (7) inject a calloc failure into @@nwtx99.c (see the build
 *   line).  On the TARGET that path is much harder to reach than it looks:
 *   calloc() -> malloc() -> __getm() issues GETMAIN RU (asm/@@getm.asm:28),
 *   which abends S80A rather than returning when storage is short.  So the
 *   two cases exercise a defensive path with host semantics; what they prove
 *   is that the function now REPORTS the failure instead of storing a NULL
 *   text unit, not that MVS will take that route to get there.
 *
 * - Text unit ORDER is asserted: DALMEMBR first, DALDSNAM second.  That is
 *   the order __dsalc() passes to SVC 99, and it is what the pre-fix code
 *   produced, so the fix must not quietly reverse it.
 * ====================================================================
 *
 * BUILD / RUN (host, from test/host).  Three defines carry the whole port and
 * none of them is guessable:
 *
 *   -D'__asm__(x)='   erases the file-scope S/370 assembler statements in the
 *                     array TUs (__asm__("\n&FUNC SETC 'arrayadd'")), which
 *                     the host assembler rejects.  It does not touch the
 *                     asm("@@ARADD") labels in clibary.h - different spelling
 *                     - so the host symbols keep the library's names.
 *   -D__32BIT__       is what libc370's own stddef.h/stdlib.h key size_t off.
 *                     None of the macros they test is defined on a macOS or
 *                     Linux host, so without it size_t is an unknown type.
 *   -Dcalloc=...      the injection point for cases (6) and (7).  It must
 *                     reach @@nwtx99.c and NOTHING else, which is why that
 *                     one file is compiled on its own line.
 *
 *     R=../..
 *     cc -std=gnu99 -D'__asm__(x)=' -D__32BIT__ -Dcalloc=tst_calloc \
 *        -I $R/include -c "$R/src/clib/@@nwtx99.c" -o nwtx99.o
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(x)=' -D__32BIT__ -I $R/include -o t tsttxdsn.c nwtx99.o \
 *        "$R/src/clib/@@txdsn.c" "$R/src/clib/@@aradd.c" \
 *        "$R/src/clib/@@arnew.c" "$R/src/clib/@@arcou.c" \
 *        "$R/src/clib/@@arget.c" "$R/src/clib/@@arfre.c"
 *     ./t                                             # 22/22, rc 0
 *
 * ASAN here buys memory-error checking, not leak checking: LeakSanitizer is
 * not supported on macOS arm64 (detect_leaks=1 aborts).  Run it on Linux if
 * you want the leak side too.
 *
 * RED, against the pre-fix source:
 *
 *     git show <pre-fix-rev>:src/clib/@@txdsn.c > /tmp/old.c
 *     ... same link line with /tmp/old.c in place of $R/src/clib/@@txdsn.c
 *         and -Wno-error=implicit-function-declaration added ...
 *     ./t                                             # 14/22, 8 failures
 *
 * That extra flag is itself part of the story: the pre-fix file called
 * wtodumpf() with no prototype in scope - it includes neither clib.h nor
 * clibwto.h - so the compiler invented the signature, which on this target
 * decides linkage (#39).  cc370 -Wall -Werror -Wno-comment rejects the
 * pre-fix file for that implicit declaration and accepts the fixed one:
 * one of the 129 translation units in #39, off the list for free.
 *
 * The target-side half of the verification is not in this file: build the
 * library and confirm the generated src/clib/@@txdsn.s references
 * @@NWTX99, @@ARADD, STRLEN, STRCHR and FREE - and no longer WTODUMPF.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include "svc99.h"
#include "clibary.h"

/* --------------------------------------------------------------------------
 * Minimal mbtcheck.h-compatible harness, same inline copy as tstcmtt.c.
 * ------------------------------------------------------------------------ */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                       \
    do {                                       \
        mbt_run++;                             \
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
 * wtodumpf() is the whole point of case (2): the pre-fix @@txdsn.c resolves
 * to THIS definition and bumps the counter, the fixed one never references
 * it.  errno is a function in libc370 (include/errno.h) and @@aradd.c sets
 * it, so the host build needs somewhere for it to land.
 * ------------------------------------------------------------------------ */
static int dumps = 0;

void wtodumpf(void *buf, int len, const char *fmt, ...)
{
    (void)buf; (void)len; (void)fmt;
    dumps++;
}

int *__errno(void)
{
    static int e;
    return &e;
}

/* calloc as seen by @@nwtx99.c (-Dcalloc=tst_calloc).  fail_at counts DOWN:
 * 0 = pass everything through, n = fail the n-th call from now on. */
static int fail_at = 0;

void *tst_calloc(size_t nmemb, size_t size)
{
    if (fail_at > 0 && --fail_at == 0) return NULL;
    return calloc(nmemb, size);
}

/* --------------------------------------------------------------------------
 * Helpers.  Every case starts from a fresh, empty array - counts compound
 * otherwise, and an array left over from a failed case would hide the next
 * one's real count.
 * ------------------------------------------------------------------------ */
static TXT99 **new_array(void)
{
    return NULL;
}

static void drop_array(TXT99 **txt99)
{
    unsigned i, n;

    if (!txt99) return;
    n = arraycount(&txt99);
    for (i = 1; i <= n; i++) {
        void *tu = arrayget(&txt99, i);
        if (tu) free(tu);
    }
    arrayfree(&txt99);
}

/* Text unit at 1-based index: key, text length and text all as expected? */
static int tu_is(TXT99 **txt99, unsigned index, int dal,
                 const char *text, int len)
{
    TXT99 *tu = arrayget(&txt99, index);

    if (!tu)                                    return 0;
    if (tu->dal   != (unsigned short)dal)       return 0;
    if (tu->count != 1)                         return 0;
    if (tu->size  != (unsigned short)len)       return 0;
    return memcmp(tu->text, text, len) == 0;
}

/* No element of the array is NULL.  __dsalc() would pass a NULL element to
 * SVC 99 as a text unit at address 0. */
static int no_null_element(TXT99 **txt99)
{
    unsigned i, n;

    if (!txt99) return 1;
    n = arraycount(&txt99);
    for (i = 1; i <= n; i++) {
        if (!arrayget(&txt99, i)) return 0;
    }
    return 1;
}

int main(void)
{
    TXT99 **txt99;
    int rc;

    printf("TSTTXDSN - libc370 #60: __txdsn() text units, no console output\n");

    /* ---------------------------------------------------------------------
     * (1) Plain DSN, no member.  One DALDSNAM text unit, nothing else.
     * ------------------------------------------------------------------- */
    txt99 = new_array();
    rc = __txdsn(&txt99, "SYS1.PARMLIB");
    CHECK_EQ(rc, 0, "(1) plain DSN: rc 0");
    CHECK_EQ(arraycount(&txt99), 1, "(1) plain DSN: one text unit");
    CHECK(tu_is(txt99, 1, DALDSNAM, "SYS1.PARMLIB", 12),
          "(1) plain DSN: DALDSNAM = SYS1.PARMLIB");
    drop_array(txt99);

    /* ---------------------------------------------------------------------
     * (2) THE #60 CASE.  A DSN with a member, the everyday path: two text
     * units, DALMEMBR first, and NOT ONE LINE on the console.  Pre-fix this
     * case passes every assertion except the last one - the dump was the
     * only thing wrong with it.
     * ------------------------------------------------------------------- */
    txt99 = new_array();
    rc = __txdsn(&txt99, "SYS1.MACLIB(IEFZB4D0)");
    CHECK_EQ(rc, 0, "(2) member DSN: rc 0");
    CHECK_EQ(arraycount(&txt99), 2, "(2) member DSN: two text units");
    CHECK(tu_is(txt99, 1, DALMEMBR, "IEFZB4D0", 8),
          "(2) member DSN: text unit 1 is DALMEMBR = IEFZB4D0");
    CHECK(tu_is(txt99, 2, DALDSNAM, "SYS1.MACLIB", 11),
          "(2) member DSN: text unit 2 is DALDSNAM = SYS1.MACLIB");
    CHECK_EQ(dumps, 0, "(2) member DSN: nothing written to the console");
    drop_array(txt99);

    /* ---------------------------------------------------------------------
     * (3) Member with no closing parenthesis.  Documented as-is: the code
     * takes everything after '(' as the member name.  Here to keep the fix
     * from changing it by accident.
     * ------------------------------------------------------------------- */
    txt99 = new_array();
    rc = __txdsn(&txt99, "SYS1.MACLIB(IEFZB4D0");
    CHECK_EQ(rc, 0, "(3) unclosed member: rc 0");
    CHECK_EQ(arraycount(&txt99), 2, "(3) unclosed member: two text units");
    CHECK(tu_is(txt99, 1, DALMEMBR, "IEFZB4D0", 8),
          "(3) unclosed member: member name taken to end of string");
    drop_array(txt99);

    /* ---------------------------------------------------------------------
     * (4) No data set name at all.  Nonzero rc, and the caller's array is
     * left exactly as it was - this is how every caller reads it
     * (@@dsalc.c:126, @@fpnew.c:78, @@cpopen.c:155: "if (err) goto quit").
     * ------------------------------------------------------------------- */
    txt99 = new_array();
    rc = __txdsn(&txt99, NULL);
    CHECK(rc != 0, "(4) NULL dataset: nonzero rc");
    CHECK_EQ(arraycount(&txt99), 0, "(4) NULL dataset: no text unit added");
    drop_array(txt99);

    /* ---------------------------------------------------------------------
     * (5) DSN too long for the 80-byte work buffer (@@txdsn.c:12).  Same
     * contract as (4).
     * ------------------------------------------------------------------- */
    txt99 = new_array();
    rc = __txdsn(&txt99,
                 "A234567890.B234567890.C234567890.D234567890."
                 "E234567890.F234567890.G234567890.H234567890");
    CHECK(rc != 0, "(5) overlong DSN: nonzero rc");
    CHECK_EQ(arraycount(&txt99), 0, "(5) overlong DSN: no text unit added");
    drop_array(txt99);

    /* ---------------------------------------------------------------------
     * (6) calloc fails building the DALMEMBR text unit.  Pre-fix: wtodumpf()
     * dumps from address 0, arrayadd() stores the NULL, the DALDSNAM unit
     * follows, and the function returns 0 - a caller told "allocated" with a
     * text unit list SVC 99 cannot walk.  Fixed: nonzero rc, empty array.
     * ------------------------------------------------------------------- */
    txt99 = new_array();
    fail_at = 1;                    /* first NewTXT99() of this case fails */
    rc = __txdsn(&txt99, "SYS1.MACLIB(IEFZB4D0)");
    fail_at = 0;
    CHECK(rc != 0, "(6) DALMEMBR calloc fails: nonzero rc");
    CHECK_EQ(arraycount(&txt99), 0, "(6) DALMEMBR calloc fails: nothing added");
    CHECK(no_null_element(txt99),
          "(6) DALMEMBR calloc fails: no NULL text unit stored");
    drop_array(txt99);

    /* ---------------------------------------------------------------------
     * (7) The member text unit is built, then calloc fails on DALDSNAM - the
     * old line 35, which passed NewTXT99() straight into arrayadd().  The
     * array keeps the one good unit (the caller frees it with the rest), but
     * it must not gain a NULL, and rc must say so.
     * ------------------------------------------------------------------- */
    txt99 = new_array();
    fail_at = 2;                    /* second NewTXT99() of this case fails */
    rc = __txdsn(&txt99, "SYS1.MACLIB(IEFZB4D0)");
    fail_at = 0;
    CHECK(rc != 0, "(7) DALDSNAM calloc fails: nonzero rc");
    CHECK_EQ(arraycount(&txt99), 1, "(7) DALDSNAM calloc fails: member unit kept");
    CHECK(no_null_element(txt99),
          "(7) DALDSNAM calloc fails: no NULL text unit stored");
    drop_array(txt99);

    /* Nothing anywhere in the run may have reached the console. */
    CHECK_EQ(dumps, 0, "(*) no console output from any case");

    return mbt_test_summary("TSTTXDSN");
}
