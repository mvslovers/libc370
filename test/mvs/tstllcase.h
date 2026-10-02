/*
 * tstllcase.h - the cases of test/host/tstll.c and test/mvs/tstll.c
 * (libc370 #314): strtoll, strtoull, atoll, llabs, lldiv and the
 * LLONG_MIN / LLONG_MAX / ULLONG_MAX limits.
 *
 * One catalogue, two runs: the host run checks the C logic of the five
 * TUs, the MVS run the same cases on the S/370 code, the 64-bit helpers
 * and the EBCDIC digit table.  Base-36 letters past 'I' are the EBCDIC
 * case: 'J' is X'D1' and 'A' X'C1', so a c - 'A' + 10 conversion reads
 * 'J' as 26 on MVS where it means 19.  On the host those cases pass
 * either way.
 *
 * The includer supplies CHECK(cond, msg), <stdio.h>, <stdlib.h>,
 * <limits.h> and <errno.h>.  Values are compared as long long and
 * printed as two hex words, so no case depends on %lld.
 */

static void ll_show(const char *what, unsigned long long v)
{
    printf("        %s %08lX%08lX\n", what,
           (unsigned long)(v >> 32), (unsigned long)(v & 0xFFFFFFFFUL));
}

#define CHECK_LL(got, want, msg)                                          \
    do {                                                                  \
        unsigned long long g_ = (unsigned long long)(got);                \
        unsigned long long w_ = (unsigned long long)(want);               \
        CHECK(g_ == w_, (msg));                                           \
        if (g_ != w_) { ll_show("got ", g_); ll_show("want", w_); }       \
    } while (0)

/* strtoull(s, &end, base) == want, end at s + off, errno == err */
static void cu(const char *s, int base, unsigned long long want,
               long off, int err, const char *msg)
{
    char *end = NULL;
    unsigned long long r;
    int e;

    errno = 0;
    r = strtoull(s, &end, base);
    e = errno;
    CHECK((unsigned long long)r == (unsigned long long)want
          && end == s + off && e == err, msg);
    if ((unsigned long long)r != (unsigned long long)want) {
        ll_show("got ", (unsigned long long)r);
        ll_show("want", (unsigned long long)want);
    }
    if (end != s + off)
        printf("        endptr at %ld, want %ld\n", (long)(end - s), off);
    if (e != err)
        printf("        errno %d, want %d\n", e, err);
}

/* strtoll(s, &end, base) == want, end at s + off, errno == err */
static void cs(const char *s, int base, long long want,
               long off, int err, const char *msg)
{
    char *end = NULL;
    long long r;
    int e;

    errno = 0;
    r = strtoll(s, &end, base);
    e = errno;
    CHECK((unsigned long long)r == (unsigned long long)want
          && end == s + off && e == err, msg);
    if ((unsigned long long)r != (unsigned long long)want) {
        ll_show("got ", (unsigned long long)r);
        ll_show("want", (unsigned long long)want);
    }
    if (end != s + off)
        printf("        endptr at %ld, want %ld\n", (long)(end - s), off);
    if (e != err)
        printf("        errno %d, want %d\n", e, err);
}

static void ll_cases(void)
{
    lldiv_t q;

    printf("(1) limits:\n");
    CHECK(sizeof(long long) == 8, "(1) long long is 8 bytes");
    CHECK_LL(LLONG_MAX, 0x7FFFFFFFFFFFFFFFULL, "(1) LLONG_MAX");
    CHECK_LL(LLONG_MIN, 0x8000000000000000ULL, "(1) LLONG_MIN");
    CHECK_LL(ULLONG_MAX, 0xFFFFFFFFFFFFFFFFULL, "(1) ULLONG_MAX");
    CHECK(LLONG_MIN < 0 && ULLONG_MAX > 0, "(1) signs");
#if LLONG_MAX != 9223372036854775807LL || ULLONG_MAX != 18446744073709551615ULL
    CHECK(0, "(1) the limits are wrong in #if");
#else
    CHECK(1, "(1) the limits work in #if");
#endif

    printf("\n(2) strtoull:\n");
    cu("0", 10, 0, 1, 0, "(2) \"0\"");
    cu("  +123abc", 10, 123, 6, 0, "(2) blanks, plus sign, trailing text");
    cu("4294967296", 10, 4294967296ULL, 10, 0, "(2) 2**32");
    cu("18446744073709551615", 10, ULLONG_MAX, 20, 0, "(2) ULLONG_MAX");
    cu("18446744073709551616", 10, ULLONG_MAX, 20, ERANGE,
       "(2) ULLONG_MAX + 1 saturates, endptr past all digits");
    cu("99999999999999999999999", 10, ULLONG_MAX, 23, ERANGE,
       "(2) far past ULLONG_MAX");
    cu("-1", 10, ULLONG_MAX, 2, 0, "(2) \"-1\" is the negation, no error");
    cu("ffffffffffffffff", 16, ULLONG_MAX, 16, 0, "(2) 16 x f");
    cu("10000000000000000", 16, ULLONG_MAX, 17, ERANGE, "(2) 2**64 in hex");
    cu("0x1F", 0, 31, 4, 0, "(2) 0x1F, base 0");
    cu("0X1f", 16, 31, 4, 0, "(2) 0X1f, base 16");
    cu("0x", 16, 0, 1, 0, "(2) \"0x\": the subject is the 0");
    cu("0xg", 0, 0, 1, 0, "(2) \"0xg\", base 0: the subject is the 0");
    cu("0777", 0, 511, 4, 0, "(2) 0777, base 0 is octal");
    cu("0778", 0, 63, 3, 0, "(2) 0778, base 0 stops at the 8");
    cu("1010", 2, 10, 4, 0, "(2) base 2");
    cu("zz", 36, 1295, 2, 0, "(2) zz, base 36");
    cu("JR", 36, 711, 2, 0, "(2) JR, base 36 (EBCDIC gap J-R)");
    cu("sz", 36, 1043, 2, 0, "(2) sz, base 36 (EBCDIC gap S-Z)");
    cu("ij", 20, 379, 2, 0, "(2) ij, base 20 (i = 18, j = 19)");
    cu("ik", 20, 18, 1, 0, "(2) ik, base 20 stops at k");
    cu("", 10, 0, 0, 0, "(2) empty: endptr is nptr");
    cu("   ", 10, 0, 0, 0, "(2) blanks only: endptr is nptr");
    cu("+", 10, 0, 0, 0, "(2) sign only: endptr is nptr");
    cu("-x", 10, 0, 0, 0, "(2) sign, no digit: endptr is nptr");
    cu("12", 1, 0, 0, EINVAL, "(2) base 1 is invalid");
    cu("12", 37, 0, 0, EINVAL, "(2) base 37 is invalid");
    cu("12", -2, 0, 0, EINVAL, "(2) a negative base is invalid");
    CHECK_LL(strtoull("77", NULL, 8), 63, "(2) endptr may be NULL");

    printf("\n(3) strtoll:\n");
    cs("9223372036854775807", 10, LLONG_MAX, 19, 0, "(3) LLONG_MAX");
    cs("9223372036854775808", 10, LLONG_MAX, 19, ERANGE,
       "(3) LLONG_MAX + 1 saturates");
    cs("-9223372036854775808", 10, LLONG_MIN, 20, 0, "(3) LLONG_MIN");
    cs("-9223372036854775809", 10, LLONG_MIN, 20, ERANGE,
       "(3) LLONG_MIN - 1 saturates");
    cs("18446744073709551616", 10, LLONG_MAX, 20, ERANGE,
       "(3) past ULLONG_MAX");
    cs(" -42 ", 10, -42, 4, 0, "(3) \" -42 \"");
    cs("-0x10", 0, -16, 5, 0, "(3) -0x10, base 0");
    cs("4294967296", 10, 4294967296LL, 10, 0, "(3) 2**32");
    cs("-4294967297", 10, -4294967297LL, 11, 0, "(3) -(2**32 + 1)");
    cs("7fffffffffffffff", 16, LLONG_MAX, 16, 0, "(3) LLONG_MAX in hex");
    cs("-8000000000000000", 16, LLONG_MIN, 17, 0, "(3) LLONG_MIN in hex");
    cs("--5", 10, 0, 0, 0, "(3) two signs: no conversion");
    cs("+-5", 10, 0, 0, 0, "(3) plus minus: no conversion");
    cs("-JR", 36, -711, 3, 0, "(3) -JR, base 36");
    cs("12", 1, 0, 0, EINVAL, "(3) base 1 is invalid");

    printf("\n(4) atoll:\n");
    CHECK_LL(atoll("-123456789012"), -123456789012LL, "(4) -123456789012");
    CHECK_LL(atoll("  77x"), 77, "(4) blanks, trailing text");
    CHECK_LL(atoll("0x10"), 0, "(4) base 10: 0x10 is 0");

    printf("\n(5) llabs:\n");
    CHECK_LL(llabs(-5), 5, "(5) llabs(-5)");
    CHECK_LL(llabs(5), 5, "(5) llabs(5)");
    CHECK_LL(llabs(-4294967296LL), 4294967296LL, "(5) llabs(-2**32)");
    CHECK_LL(llabs(LLONG_MIN + 1), LLONG_MAX, "(5) llabs(LLONG_MIN + 1)");

    printf("\n(6) lldiv:\n");
    q = lldiv(7, 2);
    CHECK(q.quot == 3 && q.rem == 1, "(6) 7 / 2");
    q = lldiv(-7, 2);
    CHECK(q.quot == -3 && q.rem == -1, "(6) -7 / 2 truncates toward zero");
    q = lldiv(7, -2);
    CHECK(q.quot == -3 && q.rem == 1, "(6) 7 / -2");
    q = lldiv(10000000000LL, 3);
    CHECK_LL(q.quot, 3333333333LL, "(6) 10**10 / 3, quotient");
    CHECK_LL(q.rem, 1, "(6) 10**10 / 3, remainder");
    q = lldiv(LLONG_MIN, 10);
    CHECK_LL(q.quot, -922337203685477580LL, "(6) LLONG_MIN / 10, quotient");
    CHECK_LL(q.rem, -8, "(6) LLONG_MIN / 10, remainder");
}
