/*
 * tstpf99.c - libc370 #578 on MVS: the printf cases of the host test
 * test/host/tstpf99.c, formatted by libc370 on the target.
 *
 * Part 1 writes one line per case of test/host/pf99cases.h to SYSPRINT,
 * "<table> <i> <j> [text] <n>", in the format "tstpf99 -l" prints for the
 * host build of the same formatter.  A diff of the two says which of the
 * host's findings hold on MVS; lines that differ are hexadecimal floating
 * point rounding a value that is not exact in both formats.
 *
 * Part 2 checks what the host cannot - %ll, %j, %z, %t, %p - against
 * C99, one TSTPF99 PASS/FAIL line per case on the console.
 *
 * RC 0 = part 2 passed, 8 = a part 2 case failed.  Part 1 is compared
 * offline.
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <mvs/wto.h>
#include "../host/pf99cases.h"

static int bad;

#define LINE(t, fmt, v) do {                                            \
        char g[256], line[300];                                         \
        int gn = snprintf(g, sizeof(g), fmt, v);                        \
        line[0] = t; line[1] = 0;                                       \
        put(line, (unsigned)i, (unsigned)j, g, gn);                     \
    } while (0)

/* the line is put together without the formatter under test */
static void num(char *s, long v)
{
    char    d[12];
    int     k = 0;
    unsigned long u = v < 0 ? -(unsigned long)v : (unsigned long)v;

    do { d[k++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) *s++ = '-';
    while (k) *s++ = d[--k];
    *s = 0;
}

static void put(char *line, unsigned i, unsigned j, const char *g, int gn)
{
    strcat(line, " ");
    num(line + strlen(line), (long)i);
    strcat(line, " ");
    num(line + strlen(line), (long)j);
    strcat(line, " [");
    strcat(line, g);
    strcat(line, "] ");
    num(line + strlen(line), gn);
    puts(line);
}

static void want(const char *what, const char *got, const char *exp)
{
    int ok = strcmp(got, exp) == 0;

    wtof("TSTPF99 %s: %s [%s] want [%s]", ok ? "PASS" : "FAIL", what, got, exp);
    if (!ok) bad++;
}

int main(void)
{
    size_t  i, j;
    char    b[64];

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

    snprintf(b, sizeof(b), "%lld", -1234567890123LL);
    want("%lld", b, "-1234567890123");
    snprintf(b, sizeof(b), "%llu", 18446744073709551615ULL);
    want("%llu", b, "18446744073709551615");
    snprintf(b, sizeof(b), "%llx", 0x123456789abcULL);
    want("%llx", b, "123456789abc");
    snprintf(b, sizeof(b), "%20lld|", 42LL);
    want("%20lld", b, "                  42|");
    snprintf(b, sizeof(b), "%jd", (intmax_t)-5);
    want("%jd", b, "-5");
    snprintf(b, sizeof(b), "%zu", (size_t)4000000000u);
    want("%zu", b, "4000000000");
    snprintf(b, sizeof(b), "%td", (ptrdiff_t)-7);
    want("%td", b, "-7");
    snprintf(b, sizeof(b), "%p", (void *)0x1234);
    wtof("TSTPF99 INFO: %%p of 0x1234 is [%s]", b);

    wtof("TSTPF99 VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
