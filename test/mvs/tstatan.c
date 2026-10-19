/*
 * tstatan.c - libc370 #675 on MVS: atan() of a negative argument never
 * returned (S322), and atan(0.5) with it.
 *
 * atan() and asin() handled a negative argument as -atan(-x) and
 * -asin(-x).  cc370 folds that, the functions being odd, into atan(x) and
 * asin(x) and then into a branch back to the top: the call looped until
 * the step ran out of time.  atan() also reached itself with a negative
 * argument from its pi/6 reduction, so 0.268 < x < 0.577 and 1.732 < x <
 * 3.732 looped as well.  atan2() calls atan() and inherited all of it.
 *
 * Each case is an argument and Python's value for it (test/mvs/tstatan.py
 * writes tstatancase.h); the probe accepts a relative error of 1e-14 and
 * reports the worst it saw, in units of 1e-16.  A WTO follows each call,
 * so the red's last line names the case before the one that looped.
 *
 * PARM selects a group: P atan() of positive arguments, N negative ones,
 * S asin()/acos(), Q atan2(); no PARM runs them all.  The red runs each
 * group in a step of its own, as each one ends at its first loop.
 *
 * BUILD (from the repository root, after make; S a scratch directory,
 * R a directory with libc.a built from the tree before #675):
 *
 *     python3 test/mvs/tstatan.py > test/mvs/tstatancase.h
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstatan.c \
 *         -o $S/TSTATNG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -Iinclude -L $R test/mvs/tstatan.c \
 *         -o $S/TSTATNR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTATNG=TSTATNG.iebcopy \
 *         TSTATNR=TSTATNR.iebcopy -o atn -xmit --dsn IBMUSER.LIBC370.ATNSCR
 *
 * then jcl/recvatan.jcl and jcl/tstatan.jcl.
 *
 * RC 0 = passed, 8 = failed; the red steps end S322.
 */
#include <math.h>
#include <mvs/wto.h>

struct acase {
    char        grp;
    int         fn;
    const char *lbl;
    double      a, b, want;
};

static const struct acase cases[] = {
#include "tstatancase.h"
};

static const char *const fname[] = { "atan", "asin", "acos", "atan2" };

/* through a volatile, so cc370 cannot fold the call */
static double call(const struct acase *c)
{
    volatile double a = c->a, b = c->b;

    switch (c->fn) {
    case 0:  return atan(a);
    case 1:  return asin(a);
    case 2:  return acos(a);
    default: return atan2(a, b);
    }
}

/* relative error in units of 1e-16, absolute for a reference of 0 */
static long err16(double got, double want)
{
    double d = got - want;

    if (d < 0) d = -d;
    if (want < 0) want = -want;
    if (want != 0.0) d = d / want;
    if (d > 1e-7) return 999999999L;
    return (long)(d * 1e16);
}

int main(int argc, char **argv)
{
    char   grp = argc > 1 ? argv[1][0] : 0;
    int    i, n = 0, bad = 0;
    long   e, worst = 0;

    for (i = 0; i < (int)(sizeof cases / sizeof cases[0]); i++) {
        const struct acase *c = &cases[i];

        if (grp && c->grp != grp) continue;
        e = err16(call(c), c->want);
        n++;
        if (e > worst) worst = e;
        if (e > 100) bad++;
        wtof("TSTATAN %s %s(%s) err %ld e-16", e > 100 ? "FAIL" : "PASS",
             fname[c->fn], c->lbl, e);
    }
    wtof("TSTATAN %d cases, %d failed, worst relative error %ld e-16",
         n, bad, worst);
    return bad ? 8 : 0;
}
