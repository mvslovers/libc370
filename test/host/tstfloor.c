/*
 * tstfloor.c - libc370 #273: floor(), ceil(), modf() and fmod() beyond
 * 2**31, on the host (IEEE).
 *
 * Links the REAL src/math/floor.c, ceil.c, modf.c and fmod.c, renamed so
 * they do not meet the host's libm, and runs test/mvs/tstfloorcase.h.  Red
 * before the fix: the old (int)/(long) conversions overflow beyond 2**31
 * (undefined in C; arm64 saturates, x86 gives INT_MIN - red either way).
 *
 * What it does NOT pin: that the integral part is exact in HFP.  Every step
 * of src/internal/ipart.h scales by a power of 16, which is exact in both
 * formats, but that is the claim test/mvs/tstfloor.c measures.
 *
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -Wextra -D__32BIT__ -I ../../include -I ../.. \
 *        -o tstfloor tstfloor.c
 *     ./tstfloor                                      # rc 0 when green
 */
#define floor lc_floor
#define ceil  lc_ceil
#define modf  lc_modf
#define fmod  lc_fmod

#include "../../src/math/floor.c"
#include "../../src/math/ceil.c"
#include "../../src/math/modf.c"
#include "../../src/math/fmod.c"

#include <stdio.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#include "../mvs/tstfloorcase.h"

int main(void)
{
    printf("=== tstfloor: floor/ceil/modf/fmod beyond 2**31 (#273) ===\n\n");
    floor_cases();
    printf("\n=== tstfloor: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
