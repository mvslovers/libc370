/*
 * tstpfval.c - libc370 #220/#384 (host side): printf values that were
 * wrong - %f below the last place printed, %g's significant digits, the
 * 0 flag and the sign, the 0x of %#x in the width, a precision past
 * __examin()'s work[128], and %s of NULL with a width or a precision.
 *
 * This run #includes vsnprint.c, snprintf.c, @@examin.c and the real
 * @@dblcvt.c (with the target's DBL_MANT_DIG, as tstdblcv.c does), under
 * AddressSanitizer: the long precisions overran work[128] on the stack.
 * test/mvs/tstpfval.c runs the same cases (test/mvs/tstpfvalcase.h) on
 * MVS through the HFP __dblcvt.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstpfval.c && ./t
 *
 * RED, 2026-10-18, against main's @@examin.c and @@dblcvt.c: ASan
 * stack-buffer-overflow in __examin() at the first long precision
 * (%.200d), so the %s-of-NULL cases after it are not reached; with
 * -DPFVAL_NO_MEM, which leaves both groups out, 19 of 26 cases fail.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/stdio/vsnprint.c"
#include "../../src/stdio/snprintf.c"
#include "../../src/stdio/@@examin.c"

/* the target's value; @@dblcvt.c's own #include <float.h> is then a no-op */
#undef  DBL_MANT_DIG
#define DBL_MANT_DIG 14
#include "../../src/stdio/@@dblcvt.c"

void    __64_from_i32(__64 *n, int32_t i32) { (void)i32; n->u32[0] = n->u32[1] = 0; }
void    __64_from_u32(__64 *n, uint32_t u32) { (void)u32; n->u32[0] = n->u32[1] = 0; }
int32_t __64_to_i32(__64 *n) { (void)n; return 0; }
int     __64_is_zero(__64 *n) { (void)n; return 1; }
void    __64_copy(__64 *dst, __64 *src) { (void)src; dst->u32[0] = dst->u32[1] = 0; }
void    __64_divmod(__64 *a, __64 *b, __64 *c, __64 *d)
{ (void)a; (void)b; c->u32[0] = c->u32[1] = 0; d->u32[0] = d->u32[1] = 0; }

int *__errno(void) { static int e; return &e; }

/* The FILE sink of __examin() (#145); snprintf only uses the string sink,
** so these only have to resolve. */
int __fputc(int c, FILE *fp) { (void)fp; return c; }
int __fputs(const char *str, FILE *fp) { (void)str; (void)fp; return 0; }

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


#include "../mvs/tstpfvalcase.h"

int main(void)
{
    ctype_init();
    printf("=== tstpfval: printf values (#220, #384), host ===\n");
    pfval_cases();
    printf("=== tstpfval: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
