/*
 * tstgmain.c - libc370 #401: getmain() must refuse a size whose rounded
 * length does not fit 24 bits, instead of asking for 0 or 64 bytes.
 *
 * ISSUE #401: getmain() rounded size + 8 to 64 bytes with the mask
 * X'00FFFFC0', which drops the carry: sizes X'FFFFB9'..X'FFFFF8' asked
 * GETMAIN for 0 bytes and X'FFFFF9'..X'FFFFFF' for 64, and the caller
 * was told it owned the full size.
 *
 * This #includes the real getmain.c with the inline assembler compiled
 * out; <ext/strutil.h> is kept out, see below.  A size that is refused returns NULL before anything else runs;
 * one that is not goes on to read the PSA and the TCB at low addresses,
 * which on the host is a crash - that is the red.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstgmain.c && ./t
 *
 * RED, 2026-10-18, against main's getmain.c: SIGSEGV on the first case,
 * X'FFFFB9' (the size passes, and the PSA read at 0x21c faults).
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stddef.h>
/* <ext/strutil.h>'s memclr() is __asm__ __volatile__(...), which the
   defines below cannot take apart: keep the header out and supply it */
#define EXT_STRUTIL_H
static void *memclr(void *s, size_t n) { (void)n; return s; }
#include "../../src/mvs/storage/getmain.c"

/* what the TU references and the refused sizes never reach */
int __issup(void) { return 0; }
void wto(const char *msg) { (void)msg; }
/* main's getmain.c before #283 reported through these */
void wtof(const char *fmt, ...) { (void)fmt; }
void wtodumpf(void *buf, int len, const char *fmt, ...) { (void)buf; (void)len; (void)fmt; }
void __wfstr(char *b, int *k, int n, const char *s) { (void)b; (void)k; (void)n; (void)s; }
void __wfhex(char *b, int *k, int n, unsigned v, int w) { (void)b; (void)k; (void)n; (void)v; (void)w; }
void __wfdec(char *b, int *k, int n, unsigned v) { (void)b; (void)k; (void)n; (void)v; }

static int run = 0, failed = 0;

static void refused(unsigned size)
{
    run++;
    if (getmain(size, 0) != NULL) {
        failed++;
        printf("FAIL: getmain(0x%06X) was not refused\n", size);
    }
}

int main(void)
{
    refused(0x00FFFFB9);    /* rounded to X'1000000': 0 bytes before */
    refused(0x00FFFFF8);
    refused(0x00FFFFF9);    /* 64 bytes before */
    refused(0x00FFFFFF);
    refused(0x01000000);    /* refused before too */
    refused(0);
    printf("tstgmain: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
