/*
 * tstvstd.c - libc370 #338 on MVS: vprintf(), vscanf() and vfscanf(),
 * the va_list forms of printf, scanf and fscanf that C99 requires and
 * libc370 did not have (vsscanf is covered by test/mvs/tstscnll.c).
 *
 * SYSIN (in jcl/tstvstd.jcl) holds the input: "42 -7" for vscanf() on
 * stdin, then "ff 3.5" for vfscanf() on the same stream.  vprintf()
 * writes to stdout (SYSPRINT); its return value, the number of characters,
 * is checked.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstvstd.c -o TSTVSTD -flinker-output=iebcopy
 *          ld370 --pack TSTVSTD=TSTVSTD.iebcopy -o tstvstd -xmit \
 *                --dsn IBMUSER.LIBC370.VSTDSCR
 * Install: jcl/recvvstd.jcl.   Run: jcl/tstvstd.jcl.
 *
 * GREEN: mvsdev JOB01310, CC 0000, 5/5, 2026-10-03 (RECEIVE JOB01309).
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <stdarg.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static int out(const char *f, ...)
{
    va_list ap;
    int r;

    va_start(ap, f);
    r = vprintf(f, ap);
    va_end(ap);
    return (r);
}

static int in(const char *f, ...)
{
    va_list ap;
    int r;

    va_start(ap, f);
    r = vscanf(f, ap);
    va_end(ap);
    return (r);
}

static int fin(FILE *fp, const char *f, ...)
{
    va_list ap;
    int r;

    va_start(ap, f);
    r = vfscanf(fp, f, ap);
    va_end(ap);
    return (r);
}

int main(void)
{
    int a = 0, b = 0, r;
    unsigned x = 0;
    double d = 0;
    char buf[64];

    printf("=== tstvstd: vprintf, vscanf, vfscanf on MVS (#338) ===\n\n");

    r = out("  vprintf: %d|%s|%05.1f\n", 12, "ab", 2.5);
    CHECK(r == sprintf(buf, "  vprintf: %d|%s|%05.1f\n", 12, "ab", 2.5),
          "vprintf returns what sprintf counts for the same output");
    /* 23 = the length of "  vprintf: 12|ab|002.5\n"; the content is printf's
       business, not this test's (on MVS %05.1f pads with blanks: #355) */
    CHECK(r == 23, "    ... 23 characters");

    r = in("%d %d", &a, &b);
    CHECK(r == 2 && a == 42 && b == -7, "vscanf reads 42 -7 from stdin");

    r = fin(stdin, "%x %lf", &x, &d);
    CHECK(r == 2 && x == 0xff && d == 3.5, "vfscanf reads ff 3.5 from stdin");

    r = in("%d", &a);
    CHECK(r == EOF || r == 0, "vscanf at the end of the input");

    printf("\n=== tstvstd: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
