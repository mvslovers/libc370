/*
 * tstmathl.c - libc370 #340 on MVS: the C99 long double functions of
 * <math.h> (sqrtl() and the 21 others).
 *
 * long double is double under cc370, and <math.h> declares each l
 * function with the asm label of its double function.  The probe calls
 * all 22 and checks that each returns exactly what the double function
 * returns for the same arguments, sqrtl(2.0L) == sqrt(2.0) among them.
 * Arguments go through volatile variables: cc370 folds a math call on a
 * constant at compile time, and the library would not be reached.
 *
 * RED is the build: without the declarations cc370 -Wall -Werror stops
 * at "implicit declaration of function `sqrtl'", and a link without
 * -Werror leaves SQRTL and the others unresolved.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstmathl.c \
 *         -o $S/TSTMTLG -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTMTLG=TSTMTLG.iebcopy -o mtl -xmit \
 *         --dsn IBMUSER.LIBC370.MLSCR
 *
 * then jcl/recvmathl.jcl and jcl/tstmathl.jcl.
 *
 * Own MVS/CE container, 2026-10-20: CC 0000, "26 of 26 checks passed",
 * sqrtl(2.0L) * 1e12 = 1414213562373.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <math.h>
#include <mvs/wto.h>

static volatile double      dx = 0.5, dy = 2.0;
static volatile long double lx = 0.5L, ly = 2.0L;
/* 0.25 takes atan()'s series directly.  Up to 2.23.0, atan() of 0.5
   did not return: it reduced 0.5 to atan(-0.06), and atan() of a
   negative argument looped (#675, fixed). */
static volatile double      ax = 0.25;
static volatile long double lax = 0.25L;
static int bad, run;

static void same(const char *name, long double l, double d)
{
    run++;
    if (l == d) return;
    bad++;
    wtof("TSTMATHL FAIL: %s differs from its double function", name);
}

int main(void)
{
    int         ei = 0, el = 0;
    double      di;
    long double li;

    run++;
    if (sizeof(long double) != sizeof(double)) {
        bad++;
        wtof("TSTMATHL FAIL: sizeof(long double) %d, sizeof(double) %d",
             (int)sizeof(long double), (int)sizeof(double));
    }

    same("ceill",  ceill(lx),       ceil(dx));
    same("fabsl",  fabsl(-lx),      fabs(-dx));
    same("floorl", floorl(lx),      floor(dx));
    same("fmodl",  fmodl(ly, lx),   fmod(dy, dx));
    same("atan2l", atan2l(lx, ly),  atan2(dx, dy));
    same("frexpl", frexpl(ly, &el), frexp(dy, &ei));
    run++;
    if (el != ei) { bad++; wtof("TSTMATHL FAIL: frexpl exponent"); }
    same("ldexpl", ldexpl(lx, 3),   ldexp(dx, 3));
    same("modfl",  modfl(ly + lx, &li), modf(dy + dx, &di));
    same("modfl integral part", li, di);
    same("powl",   powl(ly, lx),    pow(dy, dx));
    same("sqrtl",  sqrtl(ly),       sqrt(dy));
    same("acosl",  acosl(lx),       acos(dx));
    same("asinl",  asinl(lx),       asin(dx));
    same("atanl",  atanl(lax),      atan(ax));
    same("cosl",   cosl(lx),        cos(dx));
    same("sinl",   sinl(lx),        sin(dx));
    same("tanl",   tanl(lx),        tan(dx));
    same("coshl",  coshl(lx),       cosh(dx));
    same("sinhl",  sinhl(lx),       sinh(dx));
    same("tanhl",  tanhl(lx),       tanh(dx));
    same("expl",   expl(lx),        exp(dx));
    same("logl",   logl(ly),        log(dy));
    same("log10l", log10l(ly),      log10(dy));

    /* the issue's own example: sqrtl(2.0L) == sqrt(2.0), both from the
       library (through volatile; "sqrtl" above), and near the value.
       Not against the constant sqrt(2.0): cc370 folds that at compile
       time, and the folded value differs from the library's in the last
       digit (measured) */
    run++;
    if (sqrtl(ly) < 1.41421356237 || sqrtl(ly) > 1.41421356238) {
        bad++;
        wtof("TSTMATHL FAIL: sqrtl(2.0L) out of range");
    }
    wtof("TSTMATHL sqrtl(2.0L) * 1e12 = %.0f", (double)(sqrtl(ly) * 1e12L));

    wtof("TSTMATHL %s: %d of %d checks passed", bad ? "FAILED" : "PASSED",
         run - bad, run);
    return bad ? 8 : 0;
}
