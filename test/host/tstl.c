/*
 * tstl.c - libc370 #316: strtol, strtoul, atoi, atol.
 *
 * ISSUE #316: strtoul accepted any digit in any base ("9" in base 8 was
 * 9, "g" in base 16 was 16), knew no sign ("-1" was 0), left endptr past
 * what it skipped when nothing converted, never set ERANGE, read letters
 * as c - 'A' + 10 (wrong in EBCDIC past 'I'), and strtol inherited all of
 * it.  Both are rewritten on the shape of strtoll/strtoull (#314), which
 * now share src/internal/digval.h with them.
 *
 * This run #includes the four TUs and checks test/mvs/tstlcase.h with
 * the host compiler.  test/mvs/tstl.c runs the same cases on MVS.
 *
 * <ctype.h> is suppressed: libc370's macros index the EBCDIC tables.  The
 * host's isspace() and tolower() stand in.  errno is shimmed.
 *
 * BUILD / RUN (host, from the repository root):
 *
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I include -I . -o t test/host/tstl.c && ./t
 *
 * GREEN 35/35.  RED, 2026-10-03, against strtol.c/strtoul.c before #316:
 * 21/35 - the EBCDIC cases pass on the host either way.
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

#include "../../src/stdlib/strtoul.c"
#include "../../src/stdlib/strtol.c"
#include "../../src/stdlib/atoi.c"
#include "../../src/stdlib/atol.c"

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; }                                       \
        else { mbt_failed++; printf("  FAIL: %s\n", (msg)); }             \
    } while (0)

#include "../mvs/tstlcase.h"

int main(void)
{
    printf("=== tstl: strtol, strtoul, atoi, atol (#316), host ===\n\n");
    l_cases();
    printf("\n=== tstl: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
