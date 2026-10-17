/*
 * tstpf99.c - libc370 #578: the printf conversions against C99 7.19.6.1,
 * with the host's snprintf() as the reference.
 *
 * tstpf99l.c compiles libc370's real formatter (vsnprintf, __examin,
 * __dblcvt) as l370_snprintf(); this file uses only the host's headers
 * and prints every case where the two differ.
 *
 * What the host cannot answer:
 * - %ll, %j, %z, %t: long long goes through libc370's __64 helpers, which
 *   are big-endian only, and size_t is 64 bits here.  MVS cases.
 * - %p: its form is implementation-defined.
 * - Float digits: the host is IEEE, the target hexadecimal floating
 *   point; a value that is not exact in both (0.1, 0.0001) can round
 *   differently on MVS.  The host shows the algorithm, MVS the digits.
 *
 * BUILD / RUN (from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -Wno-unused-variable \
 *        -Wno-macro-redefined -Wno-asm-operand-widths \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -c -o pf99l.o tstpf99l.c
 *     cc -std=gnu99 -Wall -Wextra -Werror -Wno-format \
 *        -o t tstpf99.c pf99l.o && ./t
 *
 * RC: 0 = no case differs, 1 = at least one does.  -v prints every case.
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>

extern int  l370_snprintf(char *s, size_t n, const char *fmt, ...);
extern void l370_init(void);

static int run = 0, diff = 0, verbose = 0;

static void cmp(const char *fmt, const char *arg, const char *want,
                int wn, const char *got, int gn)
{
    run++;
    if (strcmp(want, got) != 0 || wn != gn) {
        diff++;
        printf("DIFF %-8s %-14s C99 [%s] %d   libc370 [%s] %d\n",
               fmt, arg, want, wn, got, gn);
    }
    else if (verbose) {
        printf("ok   %-8s %-14s [%s]\n", fmt, arg, got);
    }
}

/* afmt prints the argument itself, for the report */
#define CASE(fmt, afmt, v) do {                                         \
        char w[256], g[256], a[40];                                     \
        int wn = snprintf(w, sizeof(w), fmt, v);                        \
        int gn = l370_snprintf(g, sizeof(g), fmt, v);                   \
        snprintf(a, sizeof(a), afmt, v);                                \
        cmp(fmt, a, w, wn, g, gn);                                      \
    } while (0)

#include "pf99cases.h"

/* -l: libc370's output only, one "<table> <i> <j> [text] <n>" line per
   case, the format test/mvs/tstpf99.c writes on MVS */
#define LINE(t, fmt, v) do {                                            \
        char g[256];                                                    \
        int gn = l370_snprintf(g, sizeof(g), fmt, v);                   \
        printf("%c %u %u [%s] %d\n", t, (unsigned)i, (unsigned)j, g, gn); \
    } while (0)

static int listing(void)
{
    size_t i, j;

    for (i = 0; i < sizeof(ifmts) / sizeof(ifmts[0]); i++)
        for (j = 0; j < sizeof(ints) / sizeof(ints[0]); j++)
            LINE('i', ifmts[i], ints[j]);
    for (i = 0; i < sizeof(ufmts) / sizeof(ufmts[0]); i++)
        for (j = 0; j < sizeof(ints) / sizeof(ints[0]); j++)
            LINE('u', ufmts[i], (unsigned)ints[j]);
    for (i = 0; i < sizeof(dfmts) / sizeof(dfmts[0]); i++)
        for (j = 0; j < sizeof(dbls) / sizeof(dbls[0]); j++)
            LINE('d', dfmts[i], dbls[j]);
    for (i = 0; i < sizeof(sfmts) / sizeof(sfmts[0]); i++)
        for (j = 0; j < sizeof(strs) / sizeof(strs[0]); j++)
            LINE('s', sfmts[i], strs[j]);
    return 0;
}

int main(int argc, char **argv)
{
    size_t i, j;

    verbose = argc > 1 && strcmp(argv[1], "-v") == 0;
    l370_init();
    if (argc > 1 && strcmp(argv[1], "-l") == 0) return listing();

    for (i = 0; i < sizeof(ifmts) / sizeof(ifmts[0]); i++)
        for (j = 0; j < sizeof(ints) / sizeof(ints[0]); j++)
            CASE(ifmts[i], "%d", ints[j]);
    for (i = 0; i < sizeof(ufmts) / sizeof(ufmts[0]); i++)
        for (j = 0; j < sizeof(ints) / sizeof(ints[0]); j++)
            CASE(ufmts[i], "%d", (unsigned)ints[j]);
    for (i = 0; i < sizeof(dfmts) / sizeof(dfmts[0]); i++)
        for (j = 0; j < sizeof(dbls) / sizeof(dbls[0]); j++)
            CASE(dfmts[i], "%.9g", dbls[j]);
    for (i = 0; i < sizeof(sfmts) / sizeof(sfmts[0]); i++)
        for (j = 0; j < sizeof(strs) / sizeof(strs[0]); j++)
            CASE(sfmts[i], "\"%s\"", strs[j]);
    CASE("%c", "%d", 'x');
    CASE("%3c", "%d", 'x');
    CASE("%-3c", "%d", 'x');
    CASE("%%", "%d", 0);
    CASE("%5%", "%d", 0);

    printf("tstpf99: %d cases, %d differ from the host's snprintf\n", run, diff);
    return diff ? 1 : 0;
}
