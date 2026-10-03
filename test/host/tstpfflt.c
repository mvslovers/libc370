/*
 * tstpfflt.c - libc370 #355: printf's flags around a %f conversion.
 *
 * ISSUE #355: __examin() handed the field width to __dblcvt(), which pads
 * with blanks on the left, and then put a '+' or ' ' in front of the
 * padded text.  So the 0 flag was ignored ("%05.1f" of 2.5 gave "  2.5",
 * measured on mvsdev JOB01310), the - flag too, and "%+6.1f" came out
 * seven characters wide.  C99 7.19.6.1: 0 pads with zeros after the sign,
 * - pads on the right, and the width counts the sign.
 *
 * This run #includes vsnprint.c and @@examin.c with a __dblcvt() that
 * pads as the real one does (only the digits come from the host), and
 * checks the flags.  test/mvs/tstpfflt.c runs the same cases through the
 * real HFP __dblcvt on MVS.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstpfflt.c && ./t
 *
 * GREEN 19/19; RED against @@examin.c before #355: 6/19.
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

/* ---- shims -------------------------------------------------------------
 * __dblcvt is the S/370 floating point renderer; the %f case here tests
 * the bounded COPY in __examin(), not the conversion, so a fixed text is
 * exactly what it needs.  The __64_* helpers back %lld, which no case
 * uses; they only have to resolve.
 */
/* __dblcvt as the real one behaves around the width (@@dblcvt.c:277):
   the digits, then - only when nwidth exceeds them - blanks in front.
   The digits come from the host, which is all this test needs: what it
   checks is __examin()'s handling of the flags around them. */
void __dblcvt(double num, char cnvtype, size_t nwidth, int nprecision,
              char *result, size_t rsize)
{
    size_t n, pad;

    char digits[40];
    long long v;
    int i, k = 0, neg = num < 0;
    double scale = 1.0;

    (void)cnvtype;
    (void)rsize;
    /* fixed point by hand: snprintf here would be libc370's own, and
       recurse into this stub */
    for (i = 0; i < nprecision; i++)
        scale *= 10.0;
    v = (long long)((neg ? -num : num) * scale + 0.5);
    do {
        digits[k++] = (char)('0' + v % 10);
        v /= 10;
        if (k == nprecision)
            digits[k++] = '.';
    } while (v > 0 || k <= nprecision + 1);
    n = 0;
    if (neg)
        result[n++] = '-';
    while (k > 0)
        result[n++] = digits[--k];
    if (nprecision == 0)
        n--;                            /* no point without decimals */
    result[n] = '\0';
    if (nwidth > n) {
        pad = nwidth - n;
        memmove(result + pad, result, n + 1);
        while (pad > 0)
            result[--pad] = ' ';   /* no memset: libc370's is S/370 asm */
    }
}

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


#include "../mvs/tstpffltcase.h"

int main(void)
{
    ctype_init();
    printf("=== tstpfflt: printf flags around %%f (#355), host ===\n");
    pfflt_cases();
    printf("=== tstpfflt: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
