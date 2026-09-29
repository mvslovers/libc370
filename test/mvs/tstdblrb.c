/*
 * tstdblrb.c - libc370 #222: printf of a large double, a large precision
 * or a large width must stay inside the conversion buffer, on MVS.
 *
 * __dblcvt() wrote with strcat into a buffer it had no length for:
 * numbuf[50] on the plain %f path of vsnprintf()/vvprintf() - plain
 * printf("%f", 1e41) was enough - and work[80] in __examin() for any
 * width or precision.  Every one of them is an automatic, so the
 * overflow lands in the caller's stack frame.
 *
 * The RED is on the host: test/host/tstdblrb.c, under ASAN.  A few bytes
 * past an automatic may show nothing at all here, depending on the frame
 * layout, so this probe does not hunt for one.  It is the green gate on
 * HFP, reached the way a program reaches it:
 *
 *   (1) the plain path (no width, no precision) through vsnprintf and
 *       through vsprintf -> vvprintf: every value HFP holds now fits,
 *       up to about 7.2e75
 *   (2) __examin() with a precision: fits below its 128-byte work area,
 *       and is cut off - exponent kept - above it
 *   (3) __examin() with a width
 *   (4) the 0.xxx leading-zero loop, which ran past numbuf[50] as well
 *
 * No value here is a power of ten, and only the first digit is compared:
 * the /10 scaling loop is inexact on HFP, so 1e60 prints as
 * 999999999999998046...000.000000 and 1e-30 under %e as 9.99...E-31
 * (JOB00720) - on main as well, not #222.  Length and the position of
 * the '.' are what the bound is about.
 *
 * Build:   make build
 *          cc370 -O1 -Iinclude -L build/sdk test/mvs/tstdblrb.c \
 *                -o TSTDBLRB -flinker-output=iebcopy
 *          ld370 --pack TSTDBLRB=TSTDBLRB.iebcopy -o tstdblrb -xmit \
 *                --dsn IBMUSER.LIBC370.DBRBSCR
 * Install: jcl/recvdbrb.jcl (its own staging data set, not the mbt one).
 * Run:     jcl/tstdblrb.jcl.
 *
 * GREEN: mvsdev JOB00722, CC 0000, 59/59, 2026-09-29.
 * RED the same day, the same source linked against the installed sysroot
 * libc (main, before the fix): JOB00724, ABEND S0C4, PSW 078D1000
 * 00F0F0F6 - a return address overwritten with the EBCDIC digits "006";
 * SYSPRINT lost every line.  Not a red this probe relies on (see above),
 * but it is what the overflow does on MVS.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

static void
check(int ok, const char *what, const char *got)
{
    mbt_run++;
    if (ok) {
        mbt_passed++;
    }
    else {
        mbt_failed++;
        printf("  FAIL: %s: got \"%s\" (%d bytes)\n",
               what, got, (int)strlen(got));
    }
}

/* len: exact length, or -1; dot: index of the '.', or -1 */
static void
expect(const char *what, const char *got, int len, const char *prefix,
       const char *suffix, int dot)
{
    size_t n = strlen(got);
    char   *p;

    if (len >= 0) {
        check(n == (size_t)len, what, got);
    }
    if (prefix != NULL) {
        check(strncmp(got, prefix, strlen(prefix)) == 0, what, got);
    }
    if (suffix != NULL) {
        check(n >= strlen(suffix) &&
              strcmp(got + n - strlen(suffix), suffix) == 0, what, got);
    }
    if (dot >= 0) {
        p = strchr(got, '.');
        check(p != NULL && p - got == dot, what, got);
    }
}

int
main(void)
{
    char   buf[256];
    double big = 3.3e60, hfpmax = 7.2e75, one = 1.0, tiny = 1e-60;

    /* (1) */
    snprintf(buf, sizeof(buf), "%f", big);
    expect("snprintf %f 3.3e60", buf, 68, "3", NULL, 61);
    sprintf(buf, "%f", big);
    expect("sprintf %f 3.3e60", buf, 68, "3", NULL, 61);
    snprintf(buf, sizeof(buf), "%f", -hfpmax);
    expect("snprintf %f -7.2e75", buf, 84, "-7", NULL, 77);
    sprintf(buf, "%f", -hfpmax);
    expect("sprintf %f -7.2e75", buf, 84, "-7", NULL, 77);
    snprintf(buf, sizeof(buf), "%e", -hfpmax);
    expect("snprintf %e -7.2e75", buf, 13, "-7.", "E+75", 2);

    /* (2) */
    snprintf(buf, sizeof(buf), "%.30f", 3.3e50);
    expect("%.30f 3.3e50", buf, 82, "3", NULL, 51);
    snprintf(buf, sizeof(buf), "%.100f", one);
    expect("%.100f 1.0 (fits)", buf, 102, "1.0000000000", NULL, 1);
    snprintf(buf, sizeof(buf), "%.150f", one);
    expect("%.150f 1.0 (cut at 126)", buf, 126, "1.0000000000", NULL, 1);
    snprintf(buf, sizeof(buf), "%.100e", one);
    expect("%.100e 1.0 (fits)", buf, 106, "1.0000000000", "E+00", 1);
    snprintf(buf, sizeof(buf), "%.150e", one);
    expect("%.150e 1.0 (cut at 126)", buf, 126, "1.0000000000", "E+00", 1);
    snprintf(buf, sizeof(buf), "%+.150e", one);
    expect("%+.150e 1.0 (sign byte)", buf, 127, "+1.0000000000", "E+00", 2);
    sprintf(buf, "%.150e", -3.3e-30);
    expect("sprintf %.150e -3.3e-30", buf, 126, "-3.", "E-30", 2);

    /* (3) */
    snprintf(buf, sizeof(buf), "%90f", one);
    expect("%90f 1.0", buf, 90, "          ", " 1.000000", -1);
    snprintf(buf, sizeof(buf), "%200f", one);
    expect("%200f 1.0 (padding cut at 126)", buf, 126, "          ",
           " 1.000000", -1);

    /* (4) - how far past the precision it prints is #220 */
    snprintf(buf, sizeof(buf), "%f", tiny);
    expect("%f 1e-60", buf, -1, "0.0000000000", NULL, 1);
    check(strlen(buf) < 96, "%f 1e-60 inside numbuf", buf);

    /* (5) what fitted before, unchanged */
    snprintf(buf, sizeof(buf), "%f", 1.5);
    expect("%f 1.5", buf, 8, "1.500000", NULL, 1);
    snprintf(buf, sizeof(buf), "%10.2f", -2.25);
    expect("%10.2f -2.25", buf, 10, "     -2.25", NULL, 7);
    snprintf(buf, sizeof(buf), "%.3e", 5000.0);
    expect("%.3e 5000", buf, 9, "5.000E+03", NULL, 1);

    /* and once through a real FILE, vvprintf with fq set */
    printf("  printf %%f of 3.3e60: %f\n", big);

    printf("TSTDBLRB: %d checks, %d passed, %d failed\n",
           mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
