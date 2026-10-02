/*
 * tstlcase.h - the cases of test/host/tstl.c and test/mvs/tstl.c
 * (libc370 #316): strtol, strtoul, atoi and atol after their rewrite on
 * the shape of strtoll/strtoull (#314).
 *
 * The limits are libc370's 32-bit LONG_MIN/LONG_MAX/ULONG_MAX.  The code
 * clamps by those macros, not by the width of the type, so the cases
 * hold on a host whose long is 64 bits too.  The #316 cases are marked.
 *
 * The includer supplies CHECK(cond, msg), <stdio.h>, <stdlib.h>,
 * <limits.h> and <errno.h>.
 */

/* strtoul(s, &end, base) == want, end at s + off, errno == err */
static void lu(const char *s, int base, unsigned long want,
               long off, int err, const char *msg)
{
    char *end = NULL;
    unsigned long r;
    int e;

    errno = 0;
    r = strtoul(s, &end, base);
    e = errno;
    CHECK(r == want && end == s + off && e == err, msg);
    if (r != want)
        printf("        got %lu, want %lu\n", r, want);
    if (end != s + off)
        printf("        endptr at %ld, want %ld\n", (long)(end - s), off);
    if (e != err)
        printf("        errno %d, want %d\n", e, err);
}

/* strtol(s, &end, base) == want, end at s + off, errno == err */
static void ls(const char *s, int base, long want,
               long off, int err, const char *msg)
{
    char *end = NULL;
    long r;
    int e;

    errno = 0;
    r = strtol(s, &end, base);
    e = errno;
    CHECK(r == want && end == s + off && e == err, msg);
    if (r != want)
        printf("        got %ld, want %ld\n", r, want);
    if (end != s + off)
        printf("        endptr at %ld, want %ld\n", (long)(end - s), off);
    if (e != err)
        printf("        errno %d, want %d\n", e, err);
}

static void l_cases(void)
{
    printf("(1) the #316 cases:\n");
    lu("9", 8, 0, 0, 0, "(1) #316 strtoul(\"9\", 8): 9 is no octal digit");
    lu("g", 16, 0, 0, 0, "(1) #316 strtoul(\"g\", 16): g is no hex digit");
    /* the negation in the type's own width: ULONG_MAX on MVS, 2**64 - 1
       on a host whose long is 64 bits */
    lu("-1", 10, (unsigned long)-1L, 2, 0,
       "(1) #316 strtoul(\"-1\") is the negation");
    ls("--5", 10, 0, 0, 0, "(1) #316 strtol(\"--5\"): no conversion");
    lu("  x", 10, 0, 0, 0, "(1) #316 strtoul(\"  x\"): endptr is nptr");
    lu("4294967296", 10, ULONG_MAX, 10, ERANGE,
       "(1) #316 strtoul overflow saturates, ERANGE");
    ls("2147483648", 10, LONG_MAX, 10, ERANGE,
       "(1) #316 strtol overflow saturates, ERANGE");
    ls("-2147483649", 10, LONG_MIN, 11, ERANGE,
       "(1) #316 strtol underflow saturates, ERANGE");
    lu("JR", 36, 711, 2, 0, "(1) #316 JR, base 36 (EBCDIC gap J-R)");
    lu("sz", 36, 1043, 2, 0, "(1) #316 sz, base 36 (EBCDIC gap S-Z)");
    lu("12", 1, 0, 0, EINVAL, "(1) #316 base 1 is invalid");
    lu("12", 37, 0, 0, EINVAL, "(1) #316 base 37 is invalid");

    printf("\n(2) strtoul:\n");
    lu("0", 10, 0, 1, 0, "(2) \"0\"");
    lu("  +123abc", 10, 123, 6, 0, "(2) blanks, plus, trailing text");
    lu("4294967295", 10, ULONG_MAX, 10, 0, "(2) ULONG_MAX");
    lu("ffffffff", 16, ULONG_MAX, 8, 0, "(2) ffffffff");
    lu("0x1F", 0, 31, 4, 0, "(2) 0x1F, base 0");
    lu("0x", 16, 0, 1, 0, "(2) \"0x\": the subject is the 0");
    lu("0777", 0, 511, 4, 0, "(2) 0777, base 0 is octal");
    lu("0778", 0, 63, 3, 0, "(2) 0778 stops at the 8");
    lu("", 10, 0, 0, 0, "(2) empty: endptr is nptr");
    lu("+", 10, 0, 0, 0, "(2) sign only: endptr is nptr");
    CHECK(strtoul("77", NULL, 8) == 63, "(2) endptr may be NULL");

    printf("\n(3) strtol:\n");
    ls("2147483647", 10, LONG_MAX, 10, 0, "(3) LONG_MAX");
    ls("-2147483648", 10, LONG_MIN, 11, 0, "(3) LONG_MIN");
    ls(" -42 ", 10, -42, 4, 0, "(3) \" -42 \"");
    ls("-0x10", 0, -16, 5, 0, "(3) -0x10, base 0");
    ls("7fffffff", 16, LONG_MAX, 8, 0, "(3) LONG_MAX in hex");
    ls("-80000000", 16, LONG_MIN, 9, 0, "(3) LONG_MIN in hex");
    ls("+-5", 10, 0, 0, 0, "(3) plus minus: no conversion");
    ls("-JR", 36, -711, 3, 0, "(3) -JR, base 36");

    printf("\n(4) atoi, atol:\n");
    CHECK(atoi("  -123x") == -123, "(4) atoi(\"  -123x\")");
    CHECK(atol("2147483647") == LONG_MAX, "(4) atol LONG_MAX");
    CHECK(atoi("0x10") == 0, "(4) atoi is base 10: 0x10 is 0");
    CHECK(atoi("") == 0, "(4) atoi(\"\")");
}
