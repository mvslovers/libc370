/*
 * tstfloorcase.h - the cases of test/host/tstfloor.c and test/mvs/tstfloor.c
 * (libc370 #273): floor(), ceil(), modf() and fmod() beyond 2**31.
 *
 * All four took the integral part through a 32-bit int (modf through a
 * long), so for |x| >= 2**31 they returned garbage: brexx370 measured
 * 10540800000 % 1 = 1950865408, which is 10540800000 mod 2**32 (mvsdev
 * JOB01005/JOB01006, mvslovers/brexx370#254).
 *
 * Every expected value below is exact in IEEE and in HFP: integers below
 * 2**52, and fractions of .5, .25 and .75.  2**52 - 0.5 is the largest
 * value with a fraction in both formats; from 2**52 = 16**13 up every
 * double is integral.  1e20 is only ever compared with itself.
 *
 * The includer supplies CHECK(cond, msg) and <stdio.h>, and floor, ceil,
 * modf and fmod (on the host renamed, see test/host/tstfloor.c).
 */

#define P52 4503599627370496.0                 /* 2**52 = 16**13 */

/* Every argument goes through V(): cc370 (GCC 3.4) folds floor() and ceil()
   of a constant at compile time, so floor(2147483648.5) never reached the
   library - the first MVS run passed the old code on every literal case
   (JOB01335).  A volatile read is not a constant. */
static volatile double vv;

static double V(double x)
{
    vv = x;
    return (vv);
}

static void show(const char *what, double got, double want)
{
    const unsigned char *g = (const unsigned char *)&got;
    const unsigned char *w = (const unsigned char *)&want;
    int i;

    printf("        %s got ", what);
    for (i = 0; i < 8; i++) printf("%02X", g[i]);
    printf(" want ");
    for (i = 0; i < 8; i++) printf("%02X", w[i]);
    printf("\n");
}

static void eq(const char *msg, double got, double want)
{
    CHECK(got == want, msg);
    if (got != want) show("", got, want);
}

static void tm(const char *msg, double x, double wanti, double wantf)
{
    double i = -1.0;
    double f = modf(V(x), &i);

    CHECK(i == wanti && f == wantf, msg);
    if (i != wanti) show("int ", i, wanti);
    if (f != wantf) show("frac", f, wantf);
}

static void floor_cases(void)
{
    double big = 1e20;

    printf("floor()\n");
    eq("floor(2.5) = 2", floor(V(2.5)), 2.0);
    eq("floor(-2.5) = -3", floor(V(-2.5)), -3.0);
    eq("floor(-0.5) = -1", floor(V(-0.5)), -1.0);
    eq("floor(0.0) = 0", floor(V(0.0)), 0.0);
    eq("floor(2147483647.5) = 2147483647", floor(V(2147483647.5)), 2147483647.0);
    eq("floor(2147483648.0) = 2147483648", floor(V(2147483648.0)), 2147483648.0);
    eq("floor(2147483648.5) = 2147483648", floor(V(2147483648.5)), 2147483648.0);
    eq("floor(-2147483648.5) = -2147483649", floor(V(-2147483648.5)),
       -2147483649.0);
    eq("floor(4294967296.5) = 4294967296", floor(V(4294967296.5)), 4294967296.0);
    eq("floor(10540800000.25) = 10540800000", floor(V(10540800000.25)),
       10540800000.0);
    eq("floor(-10540800000.25) = -10540800001", floor(V(-10540800000.25)),
       -10540800001.0);
    eq("floor(2**52 - 0.5) = 2**52 - 1", floor(V(P52 - 0.5)), P52 - 1.0);
    eq("floor(2**52) = 2**52", floor(V(P52)), P52);
    eq("floor(2**52 + 1) = 2**52 + 1", floor(V(P52 + 1.0)), P52 + 1.0);
    eq("floor(1e20) = 1e20", floor(V(big)), big);
    eq("floor(-1e20) = -1e20", floor(V(-big)), -big);

    printf("ceil()\n");
    eq("ceil(2.5) = 3", ceil(V(2.5)), 3.0);
    eq("ceil(-2.5) = -2", ceil(V(-2.5)), -2.0);
    eq("ceil(-0.5) = 0", ceil(V(-0.5)), 0.0);
    eq("ceil(2147483647.5) = 2147483648", ceil(V(2147483647.5)), 2147483648.0);
    eq("ceil(2147483648.5) = 2147483649", ceil(V(2147483648.5)), 2147483649.0);
    eq("ceil(-2147483648.5) = -2147483648", ceil(V(-2147483648.5)),
       -2147483648.0);
    eq("ceil(10540800000.25) = 10540800001", ceil(V(10540800000.25)),
       10540800001.0);
    eq("ceil(2**52 - 0.5) = 2**52", ceil(V(P52 - 0.5)), P52);
    eq("ceil(1e20) = 1e20", ceil(V(big)), big);
    eq("ceil(-1e20) = -1e20", ceil(V(-big)), -big);

    printf("modf()\n");
    tm("modf(-3.5) = -3, -0.5", -3.5, -3.0, -0.5);
    tm("modf(2147483648.25) = 2147483648, 0.25", 2147483648.25,
       2147483648.0, 0.25);
    tm("modf(10540800000.75) = 10540800000, 0.75", 10540800000.75,
       10540800000.0, 0.75);
    tm("modf(-10540800000.75) = -10540800000, -0.75", -10540800000.75,
       -10540800000.0, -0.75);
    tm("modf(1e20) = 1e20, 0", big, big, 0.0);

    printf("fmod()\n");
    eq("fmod(10540800000, 1) = 0", fmod(V(10540800000.0), V(1.0)), 0.0);
    eq("fmod(2147483648, 1) = 0", fmod(V(2147483648.0), V(1.0)), 0.0);
    eq("fmod(10540800000.5, 1) = 0.5", fmod(V(10540800000.5), V(1.0)), 0.5);
    eq("fmod(12884901893, 4294967296) = 5",
       fmod(V(12884901893.0), V(4294967296.0)), 5.0);
    eq("fmod(1000000000000003, 7) = 2", fmod(V(1000000000000003.0), V(7.0)), 2.0);
    eq("fmod(-5.5, 2) = -1.5", fmod(V(-5.5), V(2.0)), -1.5);
    eq("fmod(5.5, -2) = 1.5", fmod(V(5.5), V(-2.0)), 1.5);
    eq("fmod(7, 7) = 0", fmod(V(7.0), V(7.0)), 0.0);
    eq("fmod(3, 7) = 3", fmod(V(3.0), V(7.0)), 3.0);
}
