/*
 * tsttm64vec.c - libc370 #49 step A: the vector table of test/mvs/tsttm64.c
 * class (C), re-derived with native 64-bit arithmetic (host).
 *
 * READ THIS FIRST, BECAUSE THIS FILE IS NOT WHAT IT LOOKS LIKE
 * ====================================================================
 * This test does NOT exercise libc370.  It includes no libc370 header and
 * calls no libc370 function.  It exists because the arithmetic half of #49
 * cannot be tested on a host at all, and the next best thing is to check
 * the numbers the target-side test asserts against.
 *
 * WHY __64 CANNOT BE HOST-TESTED.  __64 (include/clib64.h) keeps three
 * views of the same eight bytes and mixes them freely:
 *
 *     __64_cmp            compares  .u64                (native order)
 *     __64_div, __64_sub  work on   array[] of uint16_t (array[0] = MSH)
 *     __64_lshift_one_bit,
 *     __64_from_u32       work on   u32[]               (u32[0] = high)
 *
 * On the big-endian target all three coincide and the code is correct.  On
 * a little-endian host they do not.  The failure mode is the bad kind: the
 * sources compile once -U__LP64__ gets past time64.h's LP64 #error, they
 * link, they run - and they answer wrong.  A host build of the src/time64
 * scaling returns 0 for both /1000 and /1000000.  Nothing announces that
 * the harness is measuring nothing, so the obvious next move is to "fix"
 * the expected values until it is green.
 *
 * Note that word size is not the problem, so -m32 does not rescue it: an
 * ILP32 x86 host is still little-endian.  Nor is there an arrangement that
 * works - feeding operands in big-endian array[] layout does not help,
 * because __64_div_u32() builds its divisor with __64_from_u32() and
 * __64_div() compares with __64_cmp() on the way round the loop.
 *
 * WHAT THIS FILE IS FOR, THEN.  The vectors in test/mvs/tsttm64.c are
 * written as hi/lo halves of hand-computed 64-bit values.  A transposed
 * digit in one of them produces a target-side test that is green against
 * the wrong expectation - the worst outcome for a file whose job is to
 * guard a fix.  This test recomputes every one of those quotients and
 * remainders with plain unsigned long long division and compares against
 * the SAME literals, so the table is checked without an MVS and without
 * pretending to have exercised __64.
 *
 * It also checks the TOD epoch constant that src/time64/tm64clck.c,
 * tm64mclk.c and tm64uclk.c each subtract, against the calendar rather
 * than against itself.
 *
 * BUILD AND RUN (host):
 *     cc -std=gnu99 -Wall -Wextra -o t test/host/tsttm64vec.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Minimal mbtcheck.h-compatible harness, same inline copy as tsttxdsn.c.
 * ------------------------------------------------------------------------ */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                       \
    do {                                       \
        mbt_run++;                             \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * The vector table, in exactly the hi/lo form test/mvs/tsttm64.c uses.
 * ------------------------------------------------------------------------ */
struct vec {
    const char         *what;
    unsigned int        ahi, alo;   /* dividend, as the target test spells it */
    unsigned long long  d;          /* divisor                                */
    unsigned int        qhi, qlo;   /* expected quotient                      */
    unsigned int        rhi, rlo;   /* expected remainder                     */
};

/* Cases (10)-(16b) of test/mvs/tsttm64.c.  Keep the two tables in step: if
   a vector changes there, it changes here, and this test is what says so.

   Case (12) has no row of its own on purpose: it is case (11) checked with
   __64_divmod_u32() instead of __64_div_u32(), over the same dividend and
   divisor, so the (11) row already carries both its quotient and its
   remainder.  The gap in the numbering is not a gap in coverage. */
static const struct vec vectors[] = {
    { "(10)  1777488851576000 / 1000",
      0x0006509DU, 0xDF9724C0U, 1000ULL,
      0x0000019DU, 0xDA977278U, 0x00000000U, 0x00000000U },
    { "(11)  1777488851576000 / 1000000",
      0x0006509DU, 0xDF9724C0U, 1000000ULL,
      0x00000000U, 0x69F253D3U, 0x00000000U, 0x0008CA00U },
    { "(13)  0 / 1000",
      0x00000000U, 0x00000000U, 1000ULL,
      0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U },
    { "(14)  999 / 1000",
      0x00000000U, 0x000003E7U, 1000ULL,
      0x00000000U, 0x00000000U, 0x00000000U, 0x000003E7U },
    { "(15a) 1000 / 1000",
      0x00000000U, 0x000003E8U, 1000ULL,
      0x00000000U, 0x00000001U, 0x00000000U, 0x00000000U },
    { "(15b) 1000000 / 1000000",
      0x00000000U, 0x000F4240U, 1000000ULL,
      0x00000000U, 0x00000001U, 0x00000000U, 0x00000000U },
    { "(16a) (2^62 + 12345) / 1000",
      0x40000000U, 0x00003039U, 1000ULL,
      0x0010624DU, 0xD2F1AA08U, 0x00000000U, 0x000000F9U },
    { "(16b) (2^62 + 12345) / 1000000",
      0x40000000U, 0x00003039U, 1000000ULL,
      0x00000431U, 0xBDE82D7BU, 0x00000000U, 0x00061B79U }
};

static unsigned long long join(unsigned int hi, unsigned int lo)
{
    return ((unsigned long long)hi << 32) | (unsigned long long)lo;
}

int main(void)
{
    unsigned      i;
    char          msg[128];

    /* The TOD clock epoch is 1900-01-01; bit 51 is one microsecond, so one
       microsecond is 2^12 clock units.  1900-01-01 to 1970-01-01 is 25 567
       days - 70 years with 17 leap days - which is 2 208 988 800 seconds. */
    const unsigned long long secs_1900_to_1970 = 25567ULL * 86400ULL;
    const unsigned long long epoch = secs_1900_to_1970 * 1000000ULL * 4096ULL;

    printf("TSTTM64VEC - libc370 #49 step A, vector table check (host)\n\n");

    printf("TOD epoch constant\n");

    CHECK(secs_1900_to_1970 == 2208988800ULL,
          "1900-01-01 to 1970-01-01 is 2208988800 seconds");

    sprintf(msg, "epoch constant is 0x7D91048BCA000000 (got 0x%016llX)", epoch);
    CHECK(epoch == 0x7D91048BCA000000ULL, msg);

    /* The "epoch + 1 second" raw value #49 names.  One second is 10^6
       microseconds, each 2^12 clock units. */
    sprintf(msg, "epoch + 1 s is 0x7D91048CBE240000 (got 0x%016llX)",
            epoch + (1000000ULL << 12));
    CHECK(epoch + (1000000ULL << 12) == 0x7D91048CBE240000ULL, msg);

    /* ... and that it scales to exactly 1 000 000 us / 1 000 ms / 1 s, which
       is what makes it the one vector that catches the x1000 error and pins
       the dispatch_work() timer semantics at the same time. */
    {
        unsigned long long raw = epoch + (1000000ULL << 12);
        unsigned long long us  = (raw - epoch) >> 12;

        CHECK(us == 1000000ULL,        "epoch + 1 s scales to 1000000 us");
        CHECK(us / 1000ULL == 1000ULL, "epoch + 1 s scales to 1000 ms");
        CHECK(us / 1000000ULL == 1ULL, "epoch + 1 s scales to 1 s");
    }

    printf("\nDivision vectors of test/mvs/tsttm64.c class (C)\n");

    for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        const struct vec  *v = &vectors[i];
        unsigned long long a = join(v->ahi, v->alo);
        unsigned long long q = a / v->d;
        unsigned long long r = a % v->d;

        sprintf(msg, "%s -> %llu rem %llu", v->what, q, r);
        CHECK(q == join(v->qhi, v->qlo) && r == join(v->rhi, v->rlo), msg);
    }

    /* Every dividend must stay below 2^63: above that __64_div() hands over
       to its own overflow guard, which class (C) deliberately does not
       exercise.  If a future vector crosses that line, say so here rather
       than let the target test wander into untested code. */
    printf("\nBounds\n");
    for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        const struct vec *v = &vectors[i];

        sprintf(msg, "%s dividend is below 2^63", v->what);
        CHECK(join(v->ahi, v->alo) < 0x8000000000000000ULL, msg);
    }

    return mbt_test_summary("TSTTM64VEC");
}
