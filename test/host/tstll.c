/*
 * tstll.c - libc370 #314: the long long family of <stdlib.h> -- strtoll,
 * strtoull, atoll, llabs, lldiv -- and LLONG_MIN / LLONG_MAX / ULLONG_MAX
 * in <limits.h>.
 *
 * ISSUE #314: none of them existed.  strtol, strtoul, atol, labs and ldiv
 * did; their long long siblings were neither declared nor in libc.a, so a
 * program calling strtoll() failed to link ("unresolved ... STRTOLL").
 *
 * This run #includes the five TUs and checks the cases in
 * test/mvs/tstllcase.h with the host compiler: the C logic -- ERANGE and
 * saturation, endptr when nothing converts, the 0x prefix, invalid bases,
 * truncation toward zero.  test/mvs/tstll.c runs the same cases on MVS,
 * which is what checks the EBCDIC digit table and the 64-bit helpers.
 *
 * <ctype.h> is suppressed: libc370's macros index the EBCDIC tables
 * __isbuf and __tolow, which the host has not got.  The host's isspace()
 * and tolower() stand in.  errno is libc370's *(__errno()), shimmed below.
 *
 * BUILD / RUN (host, from the repository root):
 *
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I include -o t test/host/tstll.c && ./t
 *
 * RED by construction: on main 440cff8 none of the five TUs exists and
 * neither the functions, lldiv_t nor the three limits are declared, so
 * this test does not compile there.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#define __CTYPE_INCLUDED
int isspace(int c);
int tolower(int c);

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

static int errno_;
int *__errno(void) { return &errno_; }

/* both TUs define a file-static digval() */
#define digval digval_ull
#include "../../src/stdlib/strtoull.c"
#undef digval
#define digval digval_ll
#include "../../src/stdlib/strtoll.c"
#undef digval
#include "../../src/stdlib/atoll.c"
#include "../../src/stdlib/llabs.c"
#include "../../src/stdlib/lldiv.c"

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; }                                       \
        else { mbt_failed++; printf("  FAIL: %s\n", (msg)); }             \
    } while (0)

#include "../mvs/tstllcase.h"

int main(void)
{
    printf("=== tstll: the long long family (#314), host ===\n\n");
    ll_cases();
    printf("\n=== tstll: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
