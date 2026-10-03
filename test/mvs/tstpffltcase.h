/*
 * tstpffltcase.h - the cases of test/host/tstpfflt.c and
 * test/mvs/tstpfflt.c (libc370 #355): printf's flags around %f.
 *
 * C99 7.19.6.1: the width counts the whole conversion, sign included;
 * - pads on the right; 0 pads with zeros between the sign and the digits;
 * + and space put a sign in front of a non-negative value.
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

static void pfflt_cases(void)
{
    pf("002.5", "%05.1f", 2.5);
    pf("-02.5", "%05.1f", -2.5);
    pf("+02.5", "%+05.1f", 2.5);
    pf(" 02.5", "% 05.1f", 2.5);
    pf("2.5  |", "%-5.1f|", 2.5);
    pf("-2.5 |", "%-5.1f|", -2.5);
    pf("2.5  |", "%-05.1f|", 2.5);          /* - wins over 0 */
    pf("  +2.5", "%+6.1f", 2.5);            /* the sign counts in the width */
    pf("   2.5", "% 6.1f", 2.5);
    pf("+2.5", "%+.1f", 2.5);
    pf(" 2.5", "% .1f", 2.5);
    pf("   2.5", "%6.1f", 2.5);             /* unchanged: blanks by default */
    pf("  -2.5", "%6.1f", -2.5);
    pf("2.5", "%2.1f", 2.5);                /* a width below the length */
    pf("00012.50", "%08.2f", 12.5);
    pf("-0012.50", "%08.2f", -12.5);
    pf("1.000000", "%f", 1.0);
    pf("[  1.0][-1.0  ]", "[%5.1f][%-6.1f]", 1.0, -1.0);
    {
        char b[8];
        int r = snprintf(b, 5, "%08.2f", 12.5);
        CHECK(r == 8 && strcmp(b, "0001") == 0,
              "snprintf bounds a zero-padded %f: \"0001\", returns 8");
    }
}
