/*
 * tstvsnp.c - libc370 #128: vsnprintf() must honour its buffer bound on
 * EVERY conversion, and must terminate what it wrote.
 *
 * ISSUE #128: the simple conversions in src/clib/vsnprint.c were bounded
 * (`if (chcount < n) outch(...)`), but every width/precision conversion is
 * handed to __examin() - and @@examin.c:61 read `unused(chcount);`: the
 * budget vsnprintf so carefully computed was DISCARDED, and outch()/memcpy
 * wrote the full conversion through `*s++` wherever it landed.  A format
 * as small as "S%03X" overran any deliberately short buffer.  On top of
 * that, vsnprintf wrote up to n content bytes and skipped the NUL whenever
 * the text filled the buffer, so even the bounded paths handed back an
 * unterminated buffer and the caller's strlen() ran off it.
 *
 * That is exactly how it was caught: mvsMF's TSTABND formats an abend code
 * with snprintf(buf, 10, ...) into a canary-guarded buffer, and on MVS the
 * canary died while the host run (host snprintf, C semantics) stayed
 * green.  See mvslovers/mvsmf test/host/tstabnd.c case 5.
 *
 * What this pins:
 *
 *   1. C semantics for the bound: at most n-1 content bytes, always a NUL
 *      when n > 0, return value = the length the full text would have had.
 *   2. The __examin() conversions (width, zero-fill, %-8s padding) respect
 *      the budget - these are the paths that used to write unbounded.
 *   3. The %f path's memcpy is bounded too (its __dblcvt is stubbed here,
 *      so the case exercises the copy, not the conversion).
 *   4. n = 1 and n = 0 degenerate correctly.
 *
 * Buffers are heap allocations of exactly the size handed to vsnprintf, so
 * one byte past the bound is a diagnosable overflow: RUN THIS UNDER ASAN,
 * the same reasoning as test/host/tstjestx.c.
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tstjestx.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tstvsnp.c && ./t
 *
 * RED against the pre-fix source (plus the __examin() declaration, which
 * is part of the fix): case (2) dies under ASAN with a heap-buffer-overflow
 * on the ten-byte allocation.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/clib/vsnprint.c"
#include "../../src/clib/snprintf.c"
#include "../../src/clib/@@examin.c"

/* ---- shims -------------------------------------------------------------
 * __dblcvt is the S/370 floating point renderer; the %f case here tests
 * the bounded COPY in __examin(), not the conversion, so a fixed text is
 * exactly what it needs.  The __64_* helpers back %lld, which no case
 * uses; they only have to resolve.
 */
void __dblcvt(double num, char cnvtype, size_t nwidth, int nprecision,
              char *result)
{
    (void)num; (void)cnvtype; (void)nwidth; (void)nprecision;
    strcpy(result, "1.500000");
}

void    __64_from_i32(__64 *n, int32_t i32) { (void)i32; n->u32[0] = n->u32[1] = 0; }
void    __64_from_u32(__64 *n, uint32_t u32) { (void)u32; n->u32[0] = n->u32[1] = 0; }
int32_t __64_to_i32(__64 *n) { (void)n; return 0; }
int     __64_is_zero(__64 *n) { (void)n; return 1; }
void    __64_copy(__64 *dst, __64 *src) { (void)src; dst->u32[0] = dst->u32[1] = 0; }
void    __64_divmod(__64 *a, __64 *b, __64 *c, __64 *d)
{ (void)a; (void)b; c->u32[0] = c->u32[1] = 0; d->u32[0] = d->u32[1] = 0; }

int *__errno(void) { static int e; return &e; }

/* libc370's ctype is table-driven and __examin() uses it LIVE: isdigit()
** parses the width, toupper() classifies the specifier.  So unlike
** tstjestx.c's zeroed table, these are filled in at startup. */
static unsigned short isbuf_tbl[256];
unsigned short *__isbuf = isbuf_tbl;
static short toup_tbl[256];
short *__toup = toup_tbl;

static void ctype_init(void)
{
    int c;

    for (c = 0; c < 256; c++) toup_tbl[c] = (short)c;
    for (c = 'a'; c <= 'z'; c++) toup_tbl[c] = (short)(c - 'a' + 'A');
    for (c = '0'; c <= '9'; c++) isbuf_tbl[c] |= 0x0008U;   /* isdigit */
}

/* ---- harness ---------------------------------------------------------- */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define CHECK_STR(got, want, msg)                                          \
    do {                                                                   \
        mbt_run++;                                                         \
        if (strcmp((const char *)(got), (want)) == 0) {                    \
            mbt_passed++; printf("  PASS: %s\n", (msg)); }                 \
        else { mbt_failed++;                                               \
               printf("  FAIL: %s (got \"%s\", want \"%s\")\n",            \
                      (msg), (const char *)(got), (want)); }               \
    } while (0)

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
    char *b;
    int   rc;

    ctype_init();

    printf("=== tstvsnp: vsnprintf honours its bound on every conversion "
           "(#128) ===\n\n");

    printf("(1) a width format that fits:\n");
    b = calloc(1, 32);
    rc = snprintf(b, 32, "S%03X", 0x0C4);
    CHECK_STR(b, "S0C4", "(1) text rendered");
    CHECK_EQ(rc, 4, "(1) return = length");
    free(b);

    printf("\n(2) the TSTABND shape, one byte short (#128):\n");
    b = malloc(10);                     /* heap-exact: [10] does not exist */
    rc = snprintf(b, 10, "S%03X U%04d", 0xD37, 0);
    CHECK_EQ((int)strlen(b), 9, "(2) truncated to n-1 and terminated");
    CHECK_STR(b, "SD37 U000", "(2) the truncation is a prefix");
    CHECK_EQ(rc, 10, "(2) return = untruncated length");
    free(b);

    printf("\n(3) left-justified string padding, truncated (#128):\n");
    b = malloc(6);
    rc = snprintf(b, 6, "[%-8s]", "AB");
    CHECK_STR(b, "[AB  ", "(3) five content bytes, then the NUL");
    CHECK_EQ(rc, 10, "(3) return = untruncated length");
    free(b);

    printf("\n(4) zero-filled number, truncated mid-fill (#128):\n");
    b = malloc(4);
    rc = snprintf(b, 4, "%08d", 7);
    CHECK_STR(b, "000", "(4) three fill bytes, then the NUL");
    CHECK_EQ(rc, 8, "(4) return = untruncated length");
    free(b);

    printf("\n(5) the %%f copy path, truncated (#128):\n");
    b = malloc(5);
    rc = snprintf(b, 5, "%f", 1.5);     /* stubbed __dblcvt: "1.500000" */
    CHECK_STR(b, "1.50", "(5) bounded copy, then the NUL");
    CHECK_EQ(rc, 8, "(5) return = untruncated length");
    free(b);

    printf("\n(6) n = 1 writes just the terminator:\n");
    b = malloc(1);
    rc = snprintf(b, 1, "S%03X", 0x0C4);
    CHECK_EQ((int)b[0], 0, "(6) only the NUL");
    CHECK_EQ(rc, 4, "(6) return = untruncated length");
    free(b);

    printf("\n(7) n = 0 writes nothing (probing):\n");
    rc = snprintf(NULL, 0, "S%03X U%04d", 0xD37, 0);
    CHECK_EQ(rc, 10, "(7) return = length to allocate");

    printf("\n(8) the simple paths still bound and terminate:\n");
    b = malloc(4);
    rc = snprintf(b, 4, "%s", "IBMUSER");
    CHECK_STR(b, "IBM", "(8) %s truncated to n-1 and terminated");
    CHECK_EQ(rc, 7, "(8) return = untruncated length");
    free(b);

    return mbt_test_summary("tstvsnp");
}
