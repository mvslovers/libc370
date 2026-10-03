/*
 * tstsnp.c - libc370 #339 on MVS: snprintf() and vsnprintf() take a size_t,
 * setbuf() returns void, as C99 declares them.
 *
 * Before #339 they took an int and setbuf() returned one: on S/370 the bits
 * passed are the same, but a program declaring the C99 prototype did not
 * compile.  This test is built with the C99 prototypes in force, so it
 * compiling is part of the check; then it pins the behaviour across the
 * size_t range: truncation, n == 0, and n == SIZE_MAX, which vsnprintf()
 * caps to an int budget instead of letting n - 1 wrap.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstsnp.c -o TSTSNP -flinker-output=iebcopy
 *          ld370 --pack TSTSNP=TSTSNP.iebcopy -o tstsnp -xmit \
 *                --dsn IBMUSER.LIBC370.SNPSCR
 * Install: jcl/recvsnp.jcl.   Run: jcl/tstsnp.jcl.
 *
 * GREEN: mvsdev JOB01313, CC 0000, 5/5, 2026-10-03 (RECEIVE JOB01312).
 * RED: the same source against the header before #339 does not compile
 * ("conflicting types for 'snprintf'", 'vsnprintf').
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

/* the C99 prototypes: these lines do not compile against a header that
   still declares int n or an int setbuf */
int snprintf(char *s, size_t n, const char *format, ...);
int vsnprintf(char *s, size_t n, const char *format, va_list arg);
void setbuf(FILE *stream, char *buf);

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static int vs(char *b, size_t n, const char *f, ...)
{
    va_list ap;
    int r;

    va_start(ap, f);
    r = vsnprintf(b, n, f, ap);
    va_end(ap);
    return (r);
}

int main(void)
{
    char b[16];
    int r;

    setbuf(stdout, NULL);       /* void: no value to use */
    printf("=== tstsnp: snprintf/vsnprintf size_t, setbuf void (#339) ===\n\n");

    memset(b, 'x', sizeof(b));
    r = snprintf(b, sizeof(b), "%d-%s", 42, "abc");
    CHECK(r == 6 && strcmp(b, "42-abc") == 0, "snprintf fits");

    memset(b, 'x', sizeof(b));
    r = snprintf(b, 4, "%d-%s", 42, "abc");
    CHECK(r == 6 && strcmp(b, "42-") == 0, "snprintf truncates to n - 1, returns the full length");

    memset(b, 'x', sizeof(b));
    r = snprintf(b, 0, "%d-%s", 42, "abc");
    CHECK(r == 6 && b[0] == 'x', "snprintf with n == 0 writes nothing");

    memset(b, 'x', sizeof(b));
    r = snprintf(b, SIZE_MAX, "%d-%s", 42, "abc");
    CHECK(r == 6 && strcmp(b, "42-abc") == 0, "snprintf with n == SIZE_MAX: the budget is capped, not wrapped");

    memset(b, 'x', sizeof(b));
    r = vs(b, 5, "%s", "abcdefgh");
    CHECK(r == 8 && strcmp(b, "abcd") == 0, "vsnprintf truncates");

    printf("\n=== tstsnp: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
