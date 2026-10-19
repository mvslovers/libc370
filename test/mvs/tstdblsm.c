/*
 * tstdblsm.c - libc370 #225 on MVS: how many digits of %e are right.
 *
 * __dblcvt() scaled a value into [1,10) with one division or
 * multiplication by 10 per decimal exponent, up to 75 of them.  Each HFP
 * operation truncates, so the error built up in one direction and the
 * 15th digit of a large or small value was wrong.
 *
 * The probe prints, for a list of values, the bits of the double and its
 * %.16e text.  The exact decimal value of those bits is what %e should
 * show; test/mvs/tstdblsm.py works it out on the host and reports, per
 * value, how many significant digits are right and the error in units of
 * the 17th digit.  Values: 1e-78 .. 1e75, a few named ones, and 400
 * pseudo-random doubles across the exponent range.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstdblsm.c \
 *         -o $S/TSTDBLSG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstdblsm.c \
 *         -o $S/TSTDBLSR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTDBLSG=TSTDBLSG.iebcopy \
 *         TSTDBLSR=TSTDBLSR.iebcopy -o ds -xmit --dsn IBMUSER.LIBC370.DSSCR
 *
 * then jcl/recvdblsm.jcl and jcl/tstdblsm.jcl, and
 *     python3 test/mvs/tstdblsm.py <SYSPRINT of GREEN> <SYSPRINT of RED>
 *
 * RC 0 always: the verdict is the script's.
 */
#include <stdio.h>
#include <string.h>

static unsigned seed = 12345;

static unsigned rnd(void)
{
    seed = seed * 1103515245U + 12345U;
    return seed;
}

static void show(double v)
{
    unsigned w[2];

    memcpy(w, &v, sizeof(w));
    printf("%08X%08X %.16e\n", w[0], w[1], v);
}

int main(void)
{
    double  p;
    int     k;
    unsigned w[2];
    double  v;

    /* the cases of the issue, for the eye (the script skips them) */
    printf("ISSUE %%f 1e60: %f\n", 1e60);
    printf("ISSUE %%.30f 1e50: %.30f\n", 1e50);
    printf("ISSUE %%.20e -1e-30: %.20e\n", -1e-30);
    printf("ISSUE %%f 3.3e60: %f\n", 3.3e60);

    /* powers of ten, built by exact multiplication up to 1e22 and as
       literals through the compiler's conversion beyond */
    show(1e60);
    show(1e50);
    show(-1e-30);
    show(3.3e60);
    show(1.5);
    show(123.456);
    show(2.0 / 3.0);
    show(3.14159265358979);
    for (p = 1.0, k = 0; k <= 22; k++, p *= 10.0) show(p);
    show(1e23); show(1e30); show(1e40); show(1e45); show(1e55);
    show(1e65); show(1e70); show(1e75);
    show(1e-1); show(1e-5); show(1e-10); show(1e-20); show(1e-40);
    show(1e-50); show(1e-60); show(1e-70); show(1e-75);
    /* pseudo-random normalized doubles, exponents X'31'..X'4F' and
       X'01'..X'7F' */
    for (k = 0; k < 400; k++) {
        unsigned e = (k < 200) ? 0x31 + rnd() % 31 : 0x01 + rnd() % 127;

        w[0] = (e << 24) | (0x100000 + rnd() % 0xF00000);
        w[1] = rnd();
        memcpy(&v, w, sizeof(v));
        show(v);
    }
    return 0;
}
