/*
 * tstmathm.c - libc370 #389 on MVS: exp(), pow(), atan2(), log(0) and
 * frexp(0) on System/370 floating point.
 *
 * exp() summed its series without argument reduction: wrong for x < 0
 * (exp(-30) negative), and an intermediate term ended S0CC from about
 * x = 173.  sinh, cosh, tanh and pow inherit it.  atan2(0, 0) and
 * pow(0, -n) divided by zero (S0CF), pow() with an integral y multiplied
 * |y| times, log(0) returned +HUGE_VAL with EDOM, and frexp(0) cleared
 * its pointer instead of *exp.
 *
 * The cases that ended in a program check run under try(), so the red can
 * fail instead of ending the step; pow(1.0, 2e9), which the red works on
 * for minutes, is the last one.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstmathm.c \
 *         -o $S/TSTMATG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstmathm.c \
 *         -o $S/TSTMATR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTMATG=TSTMATG.iebcopy \
 *         TSTMATR=TSTMATR.iebcopy -o ma -xmit --dsn IBMUSER.LIBC370.MASCR
 *
 * then jcl/recvmathm.jcl and jcl/tstmathm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <math.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/recovery.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTMATHM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* relative difference below 1e-13 */
static int near(double got, double ref)
{
    double d = got - ref;

    if (d < 0) d = -d;
    if (ref < 0) ref = -ref;
    return d <= ref * 1e-13;
}

static void show(const char *what, double v)
{
    /* the leading 15 digits and the exponent, without printf's %e */
    int     e = 0;
    double  m = v < 0 ? -v : v;

    if (m != 0) {
        while (m >= 10) { m /= 10; e++; }
        while (m < 1) { m *= 10; e--; }
    }
    wtof("TSTMATHM   %s = %s%d.%014.0f e%d", what, v < 0 ? "-" : "",
         (int)m, (m - (int)m) * 1e14, e);
}

static double  arg1, arg2, res;
static int      err;

static int f_exp(void)   { errno = 0; res = exp(arg1); err = errno; return 0; }
static int f_pow(void)   { errno = 0; res = pow(arg1, arg2); err = errno; return 0; }
static int f_atan2(void) { errno = 0; res = atan2(arg1, arg2); err = errno; return 0; }
static int f_sinh(void)  { res = sinh(arg1); return 0; }
static int f_tanh(void)  { res = tanh(arg1); return 0; }

static int call(int (*f)(void), double a, double b)
{
    arg1 = a;
    arg2 = b;
    res = -12345.0;
    err = -1;
    return try(f, 0);
}

int main(void)
{
    int     rc, e = 99;
    double  v;

    rc = call(f_exp, 1.0, 0);
    want(rc == 0 && near(res, 2.718281828459045), "exp(1)");
    rc = call(f_exp, -20.0, 0);
    show("exp(-20)", res);
    want(rc == 0 && near(res, 2.061153622438558e-09), "exp(-20)");
    rc = call(f_exp, -30.0, 0);
    show("exp(-30)", res);
    want(rc == 0 && near(res, 9.357622968840175e-14), "exp(-30)");
    rc = call(f_exp, 174.0, 0);
    wtof("TSTMATHM   exp(174): try rc %08X", (unsigned)rc);
    want(rc == 0 && near(res, 3.691814329580466e+75), "exp(174), no S0CC");
    rc = call(f_exp, -179.0, 0);
    want(rc == 0 && near(res, 1.8251045143570802e-78), "exp(-179)");
    rc = call(f_exp, 175.0, 0);
    want(rc == 0 && res == HUGE_VAL && err == ERANGE, "exp(175) is HUGE_VAL, ERANGE");
    rc = call(f_exp, -185.0, 0);
    want(rc == 0 && res == 0.0 && err == ERANGE, "exp(-185) is 0, ERANGE");

    rc = call(f_sinh, -30.0, 0);
    want(rc == 0 && near(res, -5343237290762.231), "sinh(-30)");
    rc = call(f_tanh, 20.0, 0);
    want(rc == 0 && near(res, 1.0), "tanh(20)");
    rc = call(f_tanh, -90.0, 0);
    wtof("TSTMATHM   tanh(-90): try rc %08X", (unsigned)rc);
    want(rc == 0 && near(res, -1.0), "tanh(-90), no S0CC");

    rc = call(f_atan2, 0.0, 0.0);
    wtof("TSTMATHM   atan2(0,0): try rc %08X", (unsigned)rc);
    want(rc == 0 && res == 0.0, "atan2(0, 0) is 0, no S0CF");
    rc = call(f_atan2, 1.0, 1.0);
    want(rc == 0 && near(res, 0.7853981633974483), "atan2(1, 1)");

    rc = call(f_pow, 0.0, -2.0);
    wtof("TSTMATHM   pow(0,-2): try rc %08X", (unsigned)rc);
    want(rc == 0 && res == HUGE_VAL && err == ERANGE, "pow(0, -2) is HUGE_VAL, ERANGE");
    rc = call(f_pow, 2.0, 10.0);
    want(rc == 0 && res == 1024.0, "pow(2, 10)");
    rc = call(f_pow, -2.0, 3.0);
    want(rc == 0 && res == -8.0, "pow(-2, 3)");
    rc = call(f_pow, 2.0, -3.0);
    want(rc == 0 && res == 0.125, "pow(2, -3)");
    rc = call(f_pow, 10.0, -15.5);
    show("pow(10,-15.5)", res);
    want(rc == 0 && near(res, 3.1622776601683793e-16), "pow(10, -15.5)");

    errno = 0;
    v = log(0.0);
    want(v == -HUGE_VAL && errno == ERANGE, "log(0) is -HUGE_VAL, ERANGE");
    v = frexp(0.0, &e);
    want(v == 0.0 && e == 0, "frexp(0) sets *exp to 0");

    rc = call(f_pow, 1.0, 2e9);
    want(rc == 0 && res == 1.0, "pow(1, 2e9) without 2e9 multiplications");
    return bad ? 8 : 0;
}
