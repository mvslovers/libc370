/*
 * tstpfarg.c - libc370 #383: every conversion printf hands to __examin()
 * takes its argument off the list.
 *
 * ISSUE #383: __examin() printed nothing for %c with a width or a flag,
 * %n with a length modifier, %a and %A, and took no argument for them, so
 * every later conversion read the argument meant for the one before it
 * ("%5c|%d" of 'x', 42 printed "|120").
 *
 * This run #includes vsnprint.c and @@examin.c (with the same stand-in
 * __dblcvt() as tstpfflt.c) and runs test/mvs/tstpfargcase.h through
 * vsnprintf().  test/mvs/tstpfarg.c runs the same cases on MVS through
 * vsnprintf() and vsprintf().
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstpfarg.c && ./t
 *
 * GREEN 21/21.  RED against the sources before #383: the first 11 cases
 * fail, then ASan stops the run with a global-buffer-overflow at "abc%":
 * vsnprintf() stepped past the format's NUL.
 *
 * #423: hexflt_cases() runs the %a formatter on System/370 bit patterns,
 * which the host's IEEE doubles cannot carry through printf; the shared
 * cases check the exact %a text on MVS only.  GREEN 38/38.
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

/* __examin() calls __dblcvx(), __dblcvt() with printf's # flag (#578);
   the stand-in ignores the flag */
void __dblcvx(double num, char cnvtype, size_t nwidth, int nprecision,
              int alt, char *result, size_t rsize)
{
    (void)alt;
    __dblcvt(num, cnvtype, nwidth, nprecision, result, rsize);
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


#include "../mvs/tstpfargcase.h"

/* #423: hexflt() on System/370 bit patterns, which the host's IEEE
   doubles cannot carry through printf */
static void hx(const char *want, unsigned w0, unsigned w1, int upper,
               int prec, int alt)
{
    unsigned w[2];
    char b[128];

    w[0] = w0;
    w[1] = w1;
    hexflt(w, upper, prec, alt, b, sizeof(b));
    CHECK(strcmp(b, want) == 0, want);
    if (strcmp(b, want) != 0) printf("        got \"%s\"\n", b);
}

static void hexflt_cases(void)
{
    hx("0x1p+0", 0x41100000, 0, 0, -1, 0);              /* 1.0 */
    hx("0x8p-4", 0x40800000, 0, 0, -1, 0);              /* 0.5 */
    hx("-0x2.8p+0", 0xC1280000, 0, 0, -1, 0);           /* -2.5 */
    hx("0x0p+0", 0, 0, 0, -1, 0);
    hx("-0x0p+0", 0x80000000, 0, 0, -1, 0);             /* -0.0 */
    hx("0x0.000p+0", 0, 0, 0, 3, 0);
    hx("0XF.F8P+4", 0x42FF8000, 0, 1, -1, 0);           /* 255.5 */
    hx("0x2p+0", 0x41180000, 0, 0, 0, 0);               /* %.0a 1.5 */
    hx("0x1.0p+4", 0x41FF8000, 0, 0, 1, 0);             /* %.1a 15.96875 */
    hx("0x1.p+0", 0x41100000, 0, 0, 0, 1);              /* %#.0a */
    hx("0x1.000p+0", 0x41100000, 0, 0, 3, 0);
    hx("0x1p-4", 0x41010000, 0, 0, -1, 0);              /* unnormalized */
    hx("0xf.fffffffffffffp+248", 0x7FFFFFFF, 0xFFFFFFFF, 0, -1, 0);
    hx("0x1p-260", 0x00100000, 0, 0, -1, 0);            /* smallest */
    hx("0x1.23456789abcdep+0", 0x41123456, 0x789ABCDE, 0, -1, 0);
    hx("0x1.2345679p+0", 0x41123456, 0x789ABCDE, 0, 7, 0);
    hx("0x1.23456789abcde00p+0", 0x41123456, 0x789ABCDE, 0, 15, 0);
}

int main(void)
{
    ctype_init();
    printf("=== tstpfarg: printf takes every argument (#383), host ===\n");
    pfarg_cases();
    hexflt_cases();
    printf("=== tstpfarg: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
