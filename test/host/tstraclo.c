/*
 * tstraclo.c - libc370 #405: racf_logout() with no ACEE must not reach
 * RACINIT.
 *
 * ISSUE #405: racf_logout(NULL) put a zero ACEE address in the RACINIT
 * ENVIR=DELETE parameter list, which RAKF reads as "the address space's
 * own ACEE": the started task's resting identity was deleted (and RAKF
 * then allows what no ACEE covers), before the function stored through
 * NULL.  racf_logout(&p) with p NULL is the same request.
 *
 * This #includes the real raclgout.c with the inline assembler compiled
 * out.  The guard returns before anything else runs; without it the
 * function goes on to read the PSA at low addresses, which on the host
 * is a crash - that is the red.  There is no MVS red: run with the old
 * library it would delete the initiator's resting ACEE, which later jobs
 * in the same initiator inherit.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstraclo.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <errno.h>
#include "../../src/mvs/racf/raclgout.c"

int *__errno(void) { static int e; return &e; }
/* reached only past the guard, which no case here passes */
int __cas(unsigned *mem, unsigned *expect, unsigned new_value)
{ (void)mem; (void)expect; (void)new_value; return 0; }

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

int main(void)
{
    ACEE    *p = NULL;
    int     rc;

    errno = 0;
    rc = racf_logout(NULL);
    check(rc == -1 && errno == EINVAL, "racf_logout(NULL): -1, EINVAL");

    errno = 0;
    rc = racf_logout(&p);
    check(rc == -1 && errno == EINVAL && p == NULL,
          "racf_logout(&p), p NULL: -1, EINVAL, p unchanged");

    printf("tstraclo: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
