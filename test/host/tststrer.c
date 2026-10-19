/*
 * tststrer.c - libc370 #420: strerror() returns a string for every value.
 *
 * ISSUE #420: strerror() returned NULL above ENOTRECOVERABLE (131) -
 * the library's own VSAM errnos 150-154 among them - and below 0, and
 * the slots of its table that no errno has (11, 87-116, 122) held texts
 * of other systems' numbers.  C99 7.21.6.2 allows no null pointer.
 *
 * This #includes the real strerror.c.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tststrer.c && ./t
 *
 * RED, 2026-10-19, against main's strerror.c: NULL for 150, 200 and -1,
 * "Software caused connection abort" for 103.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include "../../src/string/strerror.c"

static int bad;

static void want(int n, const char *expect)
{
    const char *got = strerror(n);
    int ok = got != NULL && __builtin_strcmp(got, expect) == 0;

    printf("  %s  strerror(%d) = \"%s\"\n", ok ? "ok  " : "FAIL", n,
           got ? got : "(null)");
    if (!ok) bad++;
}

int main(void)
{
    want(EIO, "I/O error");
    want(ENOTRECOVERABLE, "State not recoverable");
    want(EVSTYPE, "Invalid VSAM type");
    want(EVSERROR, "VSAM error occurred");
    want(103, "Unknown error");
    want(11, "Unknown error");
    want(132, "Unknown error");
    want(200, "Unknown error");
    want(-1, "Unknown error");
    printf("tststrer: %s\n", bad ? "FAILED" : "passed");
    return bad ? 1 : 0;
}
