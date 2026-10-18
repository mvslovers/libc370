/*
 * tstsubsy.c - libc370 #407: the subsystem helpers must not store
 * outside their target, in key 0.
 *
 * ISSUE #407: ssvt_set() checked its index against 256 instead of the
 * slots ssvt_new() allocated (ssvtfnum) and wrote past the end of the
 * SSVT, in CSA; ssvt_funcmap() accepted 256 and stored it into a byte
 * as 0; ssct_remove(NULL) matched the last SSCT (its next is NULL) and
 * stored 0 at absolute address 4.
 *
 * This #includes the real @@svset.c, @@svfmap.c and @@ssrem.c with the
 * inline assembler (the PSW key switches) compiled out, and runs under
 * AddressSanitizer: the SSVT is a heap block of exactly the size
 * ssvt_new() would allocate for 4 functions.  ssct_find() is a stand-in
 * that returns a two-SSCT chain.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstsubsy.c && ./t
 *
 * RED, 2026-10-18, against main: ASan heap-buffer-overflow, WRITE of
 * size 8 in ssvt_set() (index 5 of 4 slots).
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/mvs/subsys/@@svset.c"
#include "../../src/mvs/subsys/@@svfmap.c"
#include "../../src/mvs/subsys/@@ssrem.c"

/* a chain of two: first -> second -> NULL */
static SSCT first, second;
SSCT *ssct_find(const char *name) { (void)name; return &first; }

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

static void f(void) { }

int main(void)
{
    SSVT    *ssvt = calloc(1, sizeof(SSVT) + 4 * sizeof(void *));

    ssvt->ssvtfnum = 4;
    check(ssvt_set(ssvt, 4, (void *)f) == 0 && ssvt->ssvtfrtn[3] == (void *)f,
          "ssvt_set() of the last slot");
    check(ssvt_set(ssvt, 5, (void *)f) == EPERM, "ssvt_set() past ssvtfnum: EPERM");
    check(ssvt_set(ssvt, 0, (void *)f) == EPERM, "ssvt_set() of index 0: EPERM");

    check(ssvt_funcmap(ssvt, 4, 1) == 0 && (unsigned char)ssvt->ssvtfcod[0] == 4,
          "ssvt_funcmap() to the last slot");
    check(ssvt_funcmap(ssvt, 5, 1) == EPERM, "ssvt_funcmap() past ssvtfnum: EPERM");
    ssvt->ssvtfnum = 256;
    check(ssvt_funcmap(ssvt, 256, 2) == EPERM && ssvt->ssvtfcod[1] == 0,
          "ssvt_funcmap() of 256: EPERM, nothing stored");
    ssvt->ssvtfnum = 4;

    first.ssctscta = &second;
    second.ssctscta = NULL;
    check(ssct_remove(NULL) == 4, "ssct_remove(NULL): 4, nothing stored");
    check(ssct_remove(&second) == 0 && first.ssctscta == NULL,
          "ssct_remove() of an installed SSCT");
    check(ssct_remove(&first) == 4, "ssct_remove() of the first subsystem: 4");

    free(ssvt);
    printf("tstsubsy: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
