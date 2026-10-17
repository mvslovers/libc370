/*
 * tstpfvalcase.h - the cases of test/host/tstpfval.c and
 * test/mvs/tstpfval.c: printf values that were wrong (#220, #384), and
 * conversions whose form differed from C99 (#221, #384, #578).
 *
 * #220: %f of a value below the last place printed, and %g's precision
 * as significant digits.  #384: the 0 flag, + and space before the
 * digits rather than in front of the zeros, the 0x of %#x in the width,
 * a precision of 128 or more (it ran past __examin()'s work[128]), and
 * %s of NULL with a width or a precision.
 *
 * Values are chosen so that hexadecimal floating point and IEEE
 * give the same digits.
 *
 * The includer supplies CHECK(cond, msg), <stdio.h>, <string.h> and
 * <stdarg.h>.
 */

static void pf(const char *want, const char *f, ...)
{
    char b[64];
    va_list ap;
    int r;

    va_start(ap, f);
    r = vsnprintf(b, sizeof(b), f, ap);
    va_end(ap);
    CHECK(strcmp(b, want) == 0 && r == (int)strlen(want), f);
    if (strcmp(b, want) != 0 || r != (int)strlen(want))
        printf("        got \"%s\" (%d), want \"%s\"\n", b, r, want);
}

/* a long precision: n zeros and digits, into a buffer that holds them */
static void pflong(int len, char last, const char *f, int v)
{
    char b[300];
    int r = snprintf(b, sizeof(b), f, v);
    int i, zeros = 1;

    for (i = (b[0] == '-'); i < len - 1; i++) {
        if (b[i] != '0') zeros = 0;
    }
    CHECK(r == len && (int)strlen(b) == len && b[len - 1] == last && zeros, f);
    if (!(r == len && (int)strlen(b) == len && b[len - 1] == last && zeros))
        printf("        got %d characters (strlen %d), want %d\n",
               r, (int)strlen(b), len);
}

static volatile double v0006 = 0.006, v6 = 0.6, v3 = 0.3, v001 = 0.001,
                       vsmall = 0.000123456, v15e7 = 1.5e-7, v123 = 123.456,
                       v12345 = 12345.678, v1234567 = 1234567.0,
                       v123456 = 1234.56789, v05 = 0.5, v1e5 = 1e-5;
static char * volatile nullp;
static volatile double v15 = 1.5, v1 = 1.0, v12 = 12.0;

static void pfval_cases(void)
{
    /* #220: %f below the last place printed */
    pf("0.000", "%.3f", vsmall);
    pf("0.0", "%.1f", v001);
    pf("0", "%.0f", v001);
    pf("0.000000", "%f", v15e7);
    pf("0.01", "%.2f", v0006);              /* rounds up to the last place */
    pf("1", "%.0f", v6);
    pf("0", "%.0f", v3);
    pf("     0.000", "%10.3f", vsmall);
    pf("-0.000", "%.3f", -vsmall);

    /* #220: %g counts significant digits */
    pf("123", "%.3g", v123);
    pf("12345.7", "%g", v12345);
    pf("1234567", "%.10g", v1234567);
    pf("1234.57", "%.6g", v123456);
    pf("0.000123456", "%.6g", vsmall);
    pf("0.5", "%.0g", v05);
    pf("1e-05", "%g", v1e5);                /* e style, zeros dropped, e (#221) */

    /* #384: zeros after the sign, 0x in the width */
    pf("-0001", "%05d", -1);
    pf("+0003", "%+05d", 3);
    pf(" 0003", "% 05d", 3);
    pf("0x000001", "%#08x", 1);
    pf("     0x1", "%#8x", 1);
    pf("-1   |", "%-5d|", -1);
    pf("00042", "%.5d", 42);
    pf("  00042", "%7.5d", 42);
    pf("-00042", "%.5d", -42);
    pf("00ff", "%04x", 255);

    /* form: e for %e and %g, E for %E and %G (#221) */
    pf("1.500000e+00", "%e", v15);
    pf("1.500000E+00", "%E", v15);
    pf("1E-05", "%G", v1e5);
    /* form: no point with precision 0, unless # (#578) */
    pf("1e+00", "%.0e", v1);
    pf("1.e+00", "%#.0e", v1);
    pf("1.", "%#.0f", v1);
    pf("12.", "%#.0f", v12);
    pf("1.00000", "%#g", v1);               /* # keeps %g's zeros */
    pf("1.50", "%#.3g", v15);
    /* form: # on integers (#384, #578) */
    pf("010", "%#o", 8);
    pf("0", "%#o", 0);
    pf("0", "%#.0o", 0);
    pf("0", "%#x", 0);                      /* no 0x in front of 0 */
    pf("0XFF", "%#X", 255);
    pf("00000000", "%#08x", 0);
    /* form: the 0 flag is ignored with a precision (#578) */
    pf("     001", "%08.3d", 1);
    pf("     ", "%05.0d", 0);
    /* form: h and hh narrow the value (#384) */
    pf("65535", "%hu", -1);
    pf("-1", "%hhd", 255);
    pf("-16657", "%hd", 48879);
    pf("ff", "%hhx", 0x1ff);
    pf("1", "%hhu", 257);

#ifndef PFVAL_NO_MEM   /* the red run counts the rest without them */
    /* #384: a precision past work[128] */
    pflong(200, '7', "%.200d", 7);
    pflong(201, '1', "%.200d", -1);
    pflong(150, 'f', "%.150x", 15);

    /* #384: %s of NULL with a width or a precision */
    pf("(null)", "%5s", nullp);
    pf(" (null)", "%7s", nullp);
    pf("(nu", "%.3s", nullp);
#else
    (void)pflong;
    (void)nullp;
#endif
}
