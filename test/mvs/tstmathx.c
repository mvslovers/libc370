/*
 * tstmathx.c - libc370 #389 on MVS, the range and the edges: exp() over
 * the whole HFP range, pow(), atan2(), and sinh/cosh/tanh, which inherit
 * exp().  tstmathm.c is the red/green probe of the fix; this one covers
 * what it does not reach.
 *
 * Every call runs under try(), so a program check is a result, not the end
 * of the step.  A case is PASS when the call returned (no abend), the value
 * is within tol of the reference (tol 0: equal; tol < 0: the value is not
 * compared) and errno is the wanted one (-1: not checked).  The references
 * are computed to 40 digits on the host; inputs are exact in HFP (a base
 * like 1.0001 is not, and a power of 1e6 would magnify its rounding).
 *
 *   - exp(): a grid from 1e-20 to 174.6730 and from -1e-20 to -180.2, the
 *     two limits (174.6731, -180.2182: no abend, HUGE_VAL / 0 with
 *     ERANGE beyond), and a sweep of 1419 points that checks
 *     exp(x) * exp(-x) == 1 and a rising exp(x)
 *   - sinh, cosh, tanh from -174 to 174, atan2 in all quadrants and on the
 *     axes, pow(0, y) both ways, pow() by squaring against references, the
 *     sign of an odd power, EDOM for a negative base, and integral y of
 *     2**31 and more (these run last: the old pow() multiplied |y| times)
 *
 * With -DTSTMATHX_EDGES the cases beyond the range exp() covers run as
 * well, and a miss is FIND, not FAIL: sinh/cosh below -174.7 (S0CC, S0CF)
 * and above 174.7 (half of HUGE_VAL), sinh/tanh near 0 (cancellation),
 * pow() with a tiny base and a negative y (S0CC).  They turn into plain
 * cases when those are fixed.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstmathx.c \
 *         -o $S/TSTMATXG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstmathx.c \
 *         -o $S/TSTMATXR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTMATXG=TSTMATXG.iebcopy \
 *         TSTMATXR=TSTMATXR.iebcopy -o mx -xmit --dsn IBMUSER.LIBC370.MXSCR
 *
 * then jcl/recvmathx.jcl and jcl/tstmathx.jcl.  GREEN is this tree, RED
 * the installed library.
 *
 * RC 0 = passed, 4 = edge findings only, 8 = a case failed.
 */
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <mvs/wto.h>
#include <mvs/recovery.h>

enum { F_EXP, F_SINH, F_COSH, F_TANH, F_POW, F_ATAN2 };

typedef struct {
    const char *name;
    int         fn;
    double      a, b;
    double      ref;
    int         errwant;
    double      tol;
} CASE;

static int bad, finding;

static double  arg1, arg2, res;
static int     fnsel, err;

static int run(void)
{
    errno = 0;
    switch (fnsel) {
    case F_EXP:   res = exp(arg1); break;
    case F_SINH:  res = sinh(arg1); break;
    case F_COSH:  res = cosh(arg1); break;
    case F_TANH:  res = tanh(arg1); break;
    case F_POW:   res = pow(arg1, arg2); break;
    case F_ATAN2: res = atan2(arg1, arg2); break;
    }
    err = errno;
    return 0;
}

static int call(int fn, double a, double b)
{
    fnsel = fn;
    arg1 = a;
    arg2 = b;
    res = -12345.0;
    err = -1;
    return try(run, 0);
}

static void show(double v, char *out)
{
    int     e = 0, k;
    double  m = v < 0 ? -v : v;

    if (m != 0) {
        while (m >= 10) { m /= 10; e++; }
        while (m < 1) { m *= 10; e--; }
    }
    k = (int)m;
    sprintf(out, "%s%d.%014.0f e%d", v < 0 ? "-" : "", k,
            (m - k) * 1e14, e);
}

static int near(double got, double ref, double tol)
{
    double d = got - ref;

    if (tol == 0) return got == ref;
    if (d < 0) d = -d;
    if (ref < 0) ref = -ref;
    return d <= ref * tol;
}

/* core: 1 = a miss is FAIL, 0 = a miss is FIND (an edge not fixed yet) */
static void check(const CASE *c, int core)
{
    int     rc = call(c->fn, c->a, c->b);
    int     ok;
    char    v[40];

    ok = rc == 0
      && (c->tol < 0 || near(res, c->ref, c->tol))
      && (c->errwant < 0 || err == c->errwant);
    show(res, v);
    wtof("TSTMATHX %s %s rc=%08X err=%d res=%s",
         ok ? "PASS " : (core ? "FAIL " : "FIND "), c->name, (unsigned)rc,
         err, rc ? "-" : v);
    if (!ok) {
        if (core) bad++; else finding++;
    }
}

static const CASE core[] = {
    { "exp(0)", F_EXP, 0.0, 0, 1.0, -1, 1e-13 },
    { "exp(1e-20)", F_EXP, 1e-20, 0, 1.0, -1, 1e-13 },
    { "exp(-1e-20)", F_EXP, -1e-20, 0, 1.0, -1, 1e-13 },
    { "exp(0.3)", F_EXP, 0.3, 0, 1.34985880757600318e+00, -1, 1e-13 },
    { "exp(-0.3)", F_EXP, -0.3, 0, 7.40818220681717876e-01, -1, 1e-13 },
    { "exp(0.34)", F_EXP, 0.34, 0, 1.40494759056359375e+00, -1, 1e-13 },
    { "exp(0.35)", F_EXP, 0.35, 0, 1.41906754859325734e+00, -1, 1e-13 },
    { "exp(0.7)", F_EXP, 0.7, 0, 2.01375270747047663e+00, -1, 1e-13 },
    { "exp(-0.7)", F_EXP, -0.7, 0, 4.96585303791409527e-01, -1, 1e-13 },
    { "exp(1)", F_EXP, 1.0, 0, 2.71828182845904509e+00, -1, 1e-13 },
    { "exp(-1)", F_EXP, -1.0, 0, 3.67879441171442334e-01, -1, 1e-13 },
    { "exp(5)", F_EXP, 5.0, 0, 1.48413159102576600e+02, -1, 1e-13 },
    { "exp(10)", F_EXP, 10.0, 0, 2.20264657948067179e+04, -1, 1e-13 },
    { "exp(-10)", F_EXP, -10.0, 0, 4.53999297624848542e-05, -1, 1e-13 },
    { "exp(50)", F_EXP, 50.0, 0, 5.18470552858707205e+21, -1, 1e-13 },
    { "exp(-50)", F_EXP, -50.0, 0, 1.92874984796391782e-22, -1, 1e-13 },
    { "exp(100)", F_EXP, 100.0, 0, 2.68811714181613561e+43, -1, 1e-13 },
    { "exp(-100)", F_EXP, -100.0, 0, 3.72007597602083612e-44, -1, 1e-13 },
    { "exp(150)", F_EXP, 150.0, 0, 1.39370958066637969e+65, -1, 1e-13 },
    { "exp(170)", F_EXP, 170.0, 0, 6.76179381048500912e+73, -1, 1e-13 },
    { "exp(-170)", F_EXP, -170.0, 0, 1.47889750564321332e-74, -1, 1e-13 },
    { "exp(173)", F_EXP, 173.0, 0, 1.358e+75, -1, -1 },
    { "exp(174)", F_EXP, 174.0, 0, 3.69181432958046629e+75, -1, 1e-13 },
    { "exp(174.5)", F_EXP, 174.5, 0, 6.08677281265484849e+75, -1, 1e-13 },
    { "exp(174.6730)", F_EXP, 174.6730, 0, 7.23635788631234874e+75, -1, 1e-13 },
    { "exp(174.6731) no abend", F_EXP, 174.6731, 0, 0, -1, -1 },
    { "exp(-176)", F_EXP, -176.0, 0, 3.66582041117956304e-77, -1, 1e-13 },
    { "exp(-179.5)", F_EXP, -179.5, 0, 1.10698184513750531e-78, -1, 1e-13 },
    { "exp(-180.2)", F_EXP, -180.2, 0, 5.49710915859183085e-79, -1, 1e-13 },
    { "exp(-180.2182) no abend", F_EXP, -180.2182, 0, 0, -1, -1 },
    { "exp(-180.22) is 0", F_EXP, -180.22, 0, 0.0, ERANGE, 0 },
    { "exp(176) is HUGE", F_EXP, 176.0, 0, HUGE_VAL, ERANGE, 0 },
    { "exp(1e30) is HUGE", F_EXP, 1e30, 0, HUGE_VAL, ERANGE, 0 },
    { "exp(-1e30) is 0", F_EXP, -1e30, 0, 0.0, ERANGE, 0 },

    { "sinh(1)", F_SINH, 1.0, 0, 1.17520119364380138e+00, -1, 1e-13 },
    { "cosh(1)", F_COSH, 1.0, 0, 1.54308063481524371e+00, -1, 1e-13 },
    { "tanh(1)", F_TANH, 1.0, 0, 7.61594155955764851e-01, -1, 1e-13 },
    { "sinh(-1)", F_SINH, -1.0, 0, -1.17520119364380138e+00, -1, 1e-13 },
    { "tanh(-1)", F_TANH, -1.0, 0, -7.61594155955764851e-01, -1, 1e-13 },
    { "sinh(30)", F_SINH, 30.0, 0, 5.34323729076223145e+12, -1, 1e-13 },
    { "cosh(30)", F_COSH, 30.0, 0, 5.34323729076223145e+12, -1, 1e-13 },
    { "sinh(-30)", F_SINH, -30.0, 0, -5.34323729076223145e+12, -1, 1e-13 },
    { "cosh(-30)", F_COSH, -30.0, 0, 5.34323729076223145e+12, -1, 1e-13 },
    { "tanh(-30)", F_TANH, -30.0, 0, -1.0, -1, 1e-13 },
    { "sinh(100)", F_SINH, 100.0, 0, 1.34405857090806780e+43, -1, 1e-13 },
    { "sinh(-100)", F_SINH, -100.0, 0, -1.34405857090806780e+43, -1, 1e-13 },
    { "cosh(-100)", F_COSH, -100.0, 0, 1.34405857090806780e+43, -1, 1e-13 },
    { "tanh(-100) no abend", F_TANH, -100.0, 0, -1.0, -1, 1e-13 },
    { "sinh(170)", F_SINH, 170.0, 0, 3.38089690524250456e+73, -1, 1e-13 },
    { "sinh(-170)", F_SINH, -170.0, 0, -3.38089690524250456e+73, -1, 1e-13 },
    { "cosh(-170)", F_COSH, -170.0, 0, 3.38089690524250456e+73, -1, 1e-13 },
    { "sinh(-174)", F_SINH, -174.0, 0, -1.84590716479023314e+75, -1, 1e-13 },
    { "cosh(-174)", F_COSH, -174.0, 0, 1.84590716479023314e+75, -1, 1e-13 },

    { "atan2(0,0)", F_ATAN2, 0.0, 0.0, 0.0, -1, 0 },
    { "atan2(0,1)", F_ATAN2, 0.0, 1.0, 0.0, -1, 0 },
    { "atan2(0,-1)", F_ATAN2, 0.0, -1.0, 3.14159265358979324e+00, -1, 1e-13 },
    { "atan2(1,0)", F_ATAN2, 1.0, 0.0, 1.57079632679489662e+00, -1, 1e-13 },
    { "atan2(-1,0)", F_ATAN2, -1.0, 0.0, -1.57079632679489662e+00, -1, 1e-13 },
    { "atan2(1,1)", F_ATAN2, 1.0, 1.0, 7.85398163397448310e-01, -1, 1e-13 },
    { "atan2(-1,-1)", F_ATAN2, -1.0, -1.0, -2.35619449019234492e+00, -1, 1e-13 },

    { "pow(0,-2)", F_POW, 0.0, -2.0, HUGE_VAL, ERANGE, 0 },
    { "pow(0,-1)", F_POW, 0.0, -1.0, HUGE_VAL, ERANGE, 0 },
    { "pow(0,-0.5)", F_POW, 0.0, -0.5, HUGE_VAL, ERANGE, 0 },
    { "pow(0,0)", F_POW, 0.0, 0.0, 1.0, -1, 0 },
    { "pow(0,3)", F_POW, 0.0, 3.0, 0.0, -1, 0 },
    { "pow(0,0.5)", F_POW, 0.0, 0.5, 0.0, -1, 0 },
    { "pow(2,-250)", F_POW, 2.0, -250.0, 5.52714787526044456e-76, -1, 1e-12 },
    { "pow(10,-70)", F_POW, 10.0, -70.0, 1e-70, -1, 1e-12 },
    { "pow(3,-4)", F_POW, 3.0, -4.0, 1.23456790123456783e-02, -1, 1e-12 },
    { "pow(-2,-3)", F_POW, -2.0, -3.0, -0.125, -1, 0 },
    { "pow(-3,7)", F_POW, -3.0, 7.0, -2187.0, -1, 0 },
    { "pow(10,20)", F_POW, 10.0, 20.0, 1e20, -1, 1e-12 },
    { "pow(2,0.5)", F_POW, 2.0, 0.5, 1.41421356237309515e+00, -1, 1e-12 },
    { "pow(1.5,200)", F_POW, 1.5, 200.0, 1.65291991078820810e+35, -1, 1e-12 },
    { "pow(2,300) is HUGE", F_POW, 2.0, 300.0, HUGE_VAL, ERANGE, 0 },
    { "pow(-8,1/3) is EDOM", F_POW, -8.0, 0.333333333333333, 0.0, EDOM, 0 },
};

/* C99 answers at the edges of the range, open as #731 and #732 */
#ifdef TSTMATHX_EDGES
static const CASE edge[] = {
    { "sinh(1e-5) accuracy", F_SINH, 1e-5, 0, 1.00000000001666664e-05, -1, 1e-13 },
    { "tanh(1e-5) accuracy", F_TANH, 1e-5, 0, 9.99999999966666625e-06, -1, 1e-13 },
    { "cosh(1e-5)", F_COSH, 1e-5, 0, 1.00000000005, -1, 1e-13 },
    { "sinh(175) is HUGE", F_SINH, 175.0, 0, HUGE_VAL, ERANGE, 0 },
    { "sinh(200) is HUGE", F_SINH, 200.0, 0, HUGE_VAL, ERANGE, 0 },
    { "sinh(1000) is HUGE", F_SINH, 1000.0, 0, HUGE_VAL, ERANGE, 0 },
    { "sinh(-175) is -HUGE", F_SINH, -175.0, 0, -HUGE_VAL, ERANGE, 0 },
    { "sinh(-176) is -HUGE", F_SINH, -176.0, 0, -HUGE_VAL, ERANGE, 0 },
    { "sinh(-200) is -HUGE", F_SINH, -200.0, 0, -HUGE_VAL, ERANGE, 0 },
    { "sinh(-1000) is -HUGE", F_SINH, -1000.0, 0, -HUGE_VAL, ERANGE, 0 },
    { "cosh(175) is HUGE", F_COSH, 175.0, 0, HUGE_VAL, ERANGE, 0 },
    { "cosh(200) is HUGE", F_COSH, 200.0, 0, HUGE_VAL, ERANGE, 0 },
    { "cosh(-175) is HUGE", F_COSH, -175.0, 0, HUGE_VAL, ERANGE, 0 },
    { "cosh(-176) is HUGE", F_COSH, -176.0, 0, HUGE_VAL, ERANGE, 0 },
    { "cosh(-200) is HUGE", F_COSH, -200.0, 0, HUGE_VAL, ERANGE, 0 },
    { "cosh(-1000) is HUGE", F_COSH, -1000.0, 0, HUGE_VAL, ERANGE, 0 },
    { "tanh(88) is 1", F_TANH, 88.0, 0, 1.0, -1, 1e-13 },
    { "tanh(-88) is -1", F_TANH, -88.0, 0, -1.0, -1, 1e-13 },
    { "tanh(-91) is -1", F_TANH, -91.0, 0, -1.0, -1, 1e-13 },
    { "tanh(200) is 1", F_TANH, 200.0, 0, 1.0, -1, 1e-13 },
    { "tanh(-200) is -1", F_TANH, -200.0, 0, -1.0, -1, 1e-13 },
    { "tanh(-1000) is -1", F_TANH, -1000.0, 0, -1.0, -1, 1e-13 },
    { "pow(1e-78,-1) is HUGE", F_POW, 1e-78, -1.0, HUGE_VAL, ERANGE, 0 },
    { "pow(-1e-78,-1) is -HUGE", F_POW, -1e-78, -1.0, -HUGE_VAL, ERANGE, 0 },
    { "pow(1e-78,-2) is HUGE", F_POW, 1e-78, -2.0, HUGE_VAL, ERANGE, 0 },
    { "pow(2,-300) underflow", F_POW, 2.0, -300.0, 0.0, -1, 0 },
    { "pow(10,-80) underflow", F_POW, 10.0, -80.0, 0.0, -1, 0 },
    { "pow(2,-1000) underflow", F_POW, 2.0, -1000.0, 0.0, -1, 0 },
    { "pow(0.5,1000) underflow", F_POW, 0.5, 1000.0, 0.0, -1, 0 },
    { "atan2(1e-70,1e70) underflow", F_ATAN2, 1e-70, 1e70, 0.0, -1, 0 },
    { "atan2(1e70,1e-70)", F_ATAN2, 1e70, 1e-70, 1.57079632679489662e+00, -1, 1e-13 },
    { "atan2(1e-70,-1e70)", F_ATAN2, 1e-70, -1e70, 3.14159265358979324e+00, -1, 1e-13 },
};
#endif

/* integral y of 2**31 and more: the old pow() multiplied |y| times, so these
   come last and a step time limit costs nothing else */
static const CASE slow[] = {
    { "pow(0,-3.0e9)", F_POW, 0.0, -3.0e9, HUGE_VAL, ERANGE, 0 },
    { "pow(-2,2147483647) is -HUGE", F_POW, -2.0, 2147483647.0, -HUGE_VAL, ERANGE, 0 },
    { "pow(-1,3e9) is 1", F_POW, -1.0, 3000000000.0, 1.0, -1, 0 },
    { "pow(-1,3000000001) is -1", F_POW, -1.0, 3000000001.0, -1.0, -1, 0 },
    { "pow(0.5,3e9) is 0", F_POW, 0.5, 3.0e9, 0.0, -1, 0 },
    { "pow(1.5,3e9) is HUGE", F_POW, 1.5, 3.0e9, HUGE_VAL, ERANGE, 0 },
};

/* exp(x) * exp(-x) == 1 and exp rising, over the range, no abend */
static void sweep(void)
{
    double  x, prev = 0, p, q;
    int     rc, abends = 0, prod = 0, mono = 0, n = 0, rcfirst = 0;
    char    v[40];

    for (x = -180.0; x <= 174.5; x += 0.25) {
        n++;
        rc = call(F_EXP, x, 0);
        if (rc) { if (!abends++) rcfirst = rc; prev = 0; continue; }
        p = res;
        if (prev != 0 && p <= prev) mono++;
        prev = p;
        if (x >= -170.0 && x <= 170.0) {
            rc = call(F_EXP, -x, 0);
            if (rc) { if (!abends++) rcfirst = rc; continue; }
            q = res;
            if (!near(p * q, 1.0, 1e-13)) {
                if (!prod++) {
                    show(p * q, v);
                    wtof("TSTMATHX   first exp(x)*exp(-x) != 1 at x*100=%d: %s",
                         (int)(x * 100), v);
                }
            }
        }
    }
    wtof("TSTMATHX sweep: %d points, abends %d (first %08X), "
         "product off %d, not rising %d", n, abends, (unsigned)rcfirst,
         prod, mono);
    if (abends || prod || mono) bad++;
}

int main(void)
{
    unsigned i;

    for (i = 0; i < sizeof core / sizeof core[0]; i++) check(&core[i], 1);
    sweep();
#ifdef TSTMATHX_EDGES
    for (i = 0; i < sizeof edge / sizeof edge[0]; i++) check(&edge[i], 0);
#endif
    for (i = 0; i < sizeof slow / sizeof slow[0]; i++) check(&slow[i], 1);
    wtof("TSTMATHX done: %d FAIL, %d FIND", bad, finding);
    return bad ? 8 : finding ? 4 : 0;
}
