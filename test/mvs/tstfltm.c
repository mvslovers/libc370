/*
 * tstfltm.c - libc370 #419: the <float.h> limits are constants of their
 * own type.
 *
 * Compile time: the static initializers below need constant expressions;
 * with the earlier <float.h> (members of extern objects) they did not
 * compile.  That is the red case.
 *
 * Run time on MVS:
 *   1 sizeof: FLT_* 4, DBL_* and LDBL_* 8 (LDBL_MAX was a long, the
 *     DBL/LDBL MIN and EPSILON floats)
 *   2 the bits of each constant equal the library's objects _FltMax,
 *     _DblMax, ... (the HFP maximum, minimum and epsilon), so the decimal
 *     literals cc370 predefines convert to exactly those values
 *   3 FLT_DIG is 6
 *   4 FLT_MAX widened to double is X'7FFFFFFF00000000' exactly: strtof()
 *     and scanf("%f") compare against that double literal now, so one a
 *     bit too large would let a value above FLT_MAX reach LRER (S0CC)
 *   5 strtof() and sscanf("%f") of a value just above FLT_MAX give
 *     FLT_MAX (strtof with ERANGE), and FLT_MAX itself round-trips
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstfltm.c \
 *         -o $S/TSTFLTG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstfltm.c -o $S/TSTFLTR \
 *         -flinker-output=iebcopy      (red: fails to compile)
 *     cd $S && ld370 --pack TSTFLTG=TSTFLTG.iebcopy -o fl -xmit \
 *         --dsn IBMUSER.LIBC370.FLSCR
 *
 * then jcl/recvfltm.jcl and jcl/tstfltm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <errno.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTFLTM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static float        fmax = FLT_MAX, fmin = FLT_MIN, feps = FLT_EPSILON;
static double       dmax = DBL_MAX, dmin = DBL_MIN, deps = DBL_EPSILON;
static long double  lmax = LDBL_MAX, lmin = LDBL_MIN, leps = LDBL_EPSILON;
static double       fmaxd = FLT_MAX, fmaxdn = -FLT_MAX;

/* the first n bytes of a constant against a library object */
static void same(const void *a, const void *obj, unsigned n, const char *what)
{
    const unsigned char *x = a, *y = obj;

    wtof("TSTFLTM   %s %02X%02X%02X%02X vs %02X%02X%02X%02X", what,
         x[0], x[1], x[2], x[3], y[0], y[1], y[2], y[3]);
    want(memcmp(a, obj, n) == 0, what);
}

int main(void)
{
    want(sizeof(FLT_MAX) == 4 && sizeof(FLT_MIN) == 4 && sizeof(FLT_EPSILON) == 4,
         "FLT_MAX/MIN/EPSILON are float");
    want(sizeof(DBL_MAX) == 8 && sizeof(DBL_MIN) == 8 && sizeof(DBL_EPSILON) == 8,
         "DBL_MAX/MIN/EPSILON are double");
    want(sizeof(LDBL_MAX) == 8 && sizeof(LDBL_MIN) == 8 && sizeof(LDBL_EPSILON) == 8,
         "LDBL_MAX/MIN/EPSILON are long double");
    want(FLT_DIG == 6, "FLT_DIG is 6");

    same(&fmax, &_FltMax, 4, "FLT_MAX");
    same(&fmin, &_FltMin, 4, "FLT_MIN");
    same(&feps, &_FltEps, 4, "FLT_EPSILON");
    same(&dmax, &_DblMax, 8, "DBL_MAX");
    same(&dmin, &_DblMin, 8, "DBL_MIN");
    same(&deps, &_DblEps, 8, "DBL_EPSILON");
    same(&lmax, &_DblMax, 8, "LDBL_MAX");
    same(&lmin, &_DblMin, 8, "LDBL_MIN");
    same(&leps, &_DblEps, 8, "LDBL_EPSILON");
    want(fmax > 1e75f && dmax > 7.2e75 && dmin < 5.4e-79 && dmin > 0.0,
         "the values are in range");

    {
        static const unsigned char  wide[8]  = { 0x7F, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0 };
        static const unsigned char  widen[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0 };
        float                       f;
        int                         n;

        same(&fmaxd, wide, 8, "(double)FLT_MAX is 7FFFFFFF00000000");
        same(&fmaxdn, widen, 8, "(double)-FLT_MAX is FFFFFFFF00000000");

        /* 7.2370052e75 lies between FLT_MAX (7.23700515e75) and DBL_MAX */
        errno = 0;
        f = strtof("7.2370052e75", NULL);
        want(memcmp(&f, &_FltMax, 4) == 0 && errno == ERANGE,
             "strtof() just above FLT_MAX is FLT_MAX, ERANGE");
        errno = 0;
        f = strtof("-7.2370052e75", NULL);
        want(f == -fmax && errno == ERANGE,
             "strtof() just below -FLT_MAX is -FLT_MAX, ERANGE");
        f = 0;
        n = sscanf("7.2370052e75", "%f", &f);
        want(n == 1 && memcmp(&f, &_FltMax, 4) == 0,
             "sscanf(\"%f\") just above FLT_MAX is FLT_MAX");
        errno = 0;
        f = strtof("1e75", NULL);
        want(f > 9.9e74f && f < 1.01e75f && errno == 0,
             "control: strtof(\"1e75\") converts, no ERANGE");
    }

    return bad ? 8 : 0;
}
