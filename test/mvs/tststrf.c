/*
 * tststrf.c - libc370 #314 on MVS: strtof() and strtold().
 *
 * long double is double under cc370, so strtold() is strtod().  strtof()
 * is strtod() narrowed to float, and float and double share the HFP
 * exponent range: the only way the narrowing fails is LRER rounding a
 * value just above FLT_MAX up past the largest exponent.  That is an
 * exponent-overflow program check, S0CC, which no program mask turns off.
 * strtof() answers anything above FLT_MAX itself, with ERANGE.
 *
 * Built twice from this source: TSTSTRF links libc.a's strtof(); TSTSTRR
 * (-DRED) replaces it with the naive (float)strtod(), the red control for
 * the S0CC.  Only the probe value is converted in RED.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tststrf.c -o TSTSTRF -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk -DRED \
 *                test/mvs/tststrf.c -o TSTSTRR -flinker-output=iebcopy
 *          ld370 --pack TSTSTRF=TSTSTRF.iebcopy TSTSTRR=TSTSTRR.iebcopy \
 *                -o tststrf -xmit --dsn IBMUSER.LIBC370.STRFSCR
 * Install: jcl/recvstrf.jcl.   Run: jcl/tststrf.jcl.
 *
 * mvsdev JOB01159, 2026-10-02 (RECEIVE JOB01158): GREEN CC 0000, 15/15;
 * RED ABEND S0CC.  RED's printf lines are lost with the abend (stdio is
 * not flushed); GREEN runs the same strtod(probe) without one, so the
 * S0CC is the LRER.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <float.h>

static int mbt_run __attribute__((unused)) = 0;
static int mbt_passed __attribute__((unused)) = 0;
static int mbt_failed __attribute__((unused)) = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static unsigned long fbits(float f)
{
    unsigned long u;

    memcpy(&u, &f, sizeof(u));
    return (u);
}

/* just above FLT_MAX (X'7FFFFFFF'), below DBL_MAX, and close enough to
   FLT_MAX + 1 ulp that LRER rounds up */
static const char probe[] = "7.2370055e75";

int main(void)
{
    float f;
    double d;
#ifndef RED
    char *end;
    long double ld;
#endif

#ifdef RED
    printf("=== tststrf RED: (float)strtod(\"%s\") ===\n", probe);
    d = strtod(probe, NULL);
    printf("  strtod gave a value %s FLT_MAX\n", d > FLT_MAX ? ">" : "<=");
    f = (float)d;
    printf("  survived: X'%08lX'\n", fbits(f));
    return 0;
#else
    printf("=== tststrf: strtof, strtold on MVS (#314) ===\n\n");

    CHECK(sizeof(long double) == sizeof(double), "long double is double");
    ld = strtold("1.5xyz", &end);
    CHECK(ld == 1.5, "strtold(\"1.5xyz\") is 1.5");
    CHECK(strcmp(end, "xyz") == 0, "    ... endptr at xyz");

    errno = 0;
    f = strtof("1.5xyz", &end);
    CHECK(f == 1.5f, "strtof(\"1.5xyz\") is 1.5");
    CHECK(strcmp(end, "xyz") == 0, "    ... endptr at xyz");
    CHECK(errno == 0, "    ... no errno");
    f = strtof("-0.25", NULL);
    CHECK(f == -0.25f, "strtof(\"-0.25\")");
    f = strtof("0.1", NULL);
    CHECK(f == (float)strtod("0.1", NULL), "strtof(\"0.1\") is (float)strtod");

    d = strtod(probe, NULL);
    CHECK(d > FLT_MAX, "the probe is above FLT_MAX as a double");
    errno = 0;
    f = strtof(probe, &end);
    CHECK(fbits(f) == 0x7FFFFFFFUL, "strtof(probe) is FLT_MAX, no S0CC");
    CHECK(errno == ERANGE, "    ... ERANGE");
    CHECK(*end == '\0', "    ... endptr past the number");
    errno = 0;
    f = strtof("-7.2370055e75", NULL);
    CHECK(fbits(f) == 0xFFFFFFFFUL, "strtof(-probe) is -FLT_MAX");
    CHECK(errno == ERANGE, "    ... ERANGE");
    errno = 0;
    f = strtof("7.2370050e75", NULL);
    CHECK(errno == 0 && f <= FLT_MAX && f > 7.0e75f,
          "just below FLT_MAX converts, no errno");

    printf("\n=== tststrf: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
#endif
}
