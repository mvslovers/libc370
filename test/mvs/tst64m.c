/*
 * tst64m.c - libc370 #397 on MVS: the __64 helpers.  __64 is big-endian
 * by design (include/ext/int64.h), so this runs on the target only.
 *
 *   1 __64_div() by 0 returns: quotient 0, errno EDOM; __64_divmod() by 0
 *     leaves a as the remainder.  It looped for ever.
 *   2 __64_mul_i32(10, -3) is -30 modulo 2**64; __64_div_i32(100, -1) is
 *     0 (a divisor of 2**64 - 1).  The sign swapped the operation.
 *   3 __64_from_string("65539") is 65539 (gave 3), "12345678901" whole.
 *   4 __64_from_string("0b101") is 5 (gave 0); "0x1f" still 31.
 *
 * CASE=0 runs all four (this tree); the red build against the installed
 * library runs one case per step, CASE=1..4 - case 1 there needs the
 * step's TIME limit.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -DCASE=0 -Iinclude -L build/sdk \
 *         test/mvs/tst64m.c -o $S/TST64M -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DCASE=1 test/mvs/tst64m.c \
 *         -o $S/TST64R1 -flinker-output=iebcopy      (2 to 4 alike)
 *     cd $S && ld370 --pack TST64M=TST64M.iebcopy TST64R1=TST64R1.iebcopy \
 *         ... -o i64 -xmit --dsn IBMUSER.LIBC370.I6SCR
 *
 * then jcl/recv64m.jcl and jcl/tst64m.jcl.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <errno.h>
#include <mvs/wto.h>
#include <ext/int64.h>

#ifndef CASE
#define CASE 0
#endif

static int bad;

static void want(int cond, const char *what)
{
    wtof("TST64M %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    __64    a, b, c, d;

#if CASE == 0 || CASE == 1
    __64_from_u32(&a, 1000);
    __64_init(&b);
    __64_from_u32(&c, 77);
    errno = 0;
    __64_div(&a, &b, &c);
    want(__64_is_zero(&c) && errno == EDOM, "1 __64_div by 0: 0, EDOM");
    __64_divmod(&a, &b, &c, &d);
    want(__64_is_zero(&c) && d.u64 == 1000, "1 __64_divmod by 0: rem a");
#endif
#if CASE == 0 || CASE == 2
    __64_from_u32(&a, 10);
    __64_mul_i32(&a, -3, &c);
    want(c.u64 == (uint64_t)-30LL, "2 __64_mul_i32(10, -3) is -30");
    __64_from_u32(&a, 100);
    __64_div_i32(&a, -1, &c);
    want(__64_is_zero(&c), "2 __64_div_i32(100, -1) is 0");
#endif
#if CASE == 0 || CASE == 3
    __64_from_string(&c, "65539");
    want(c.u64 == 65539ULL, "3 \"65539\" is 65539");
    __64_from_string(&c, "12345678901");
    want(c.u64 == 12345678901ULL, "3 \"12345678901\" whole");
#endif
#if CASE == 0 || CASE == 4
    __64_from_string(&c, "0b101");
    want(c.u64 == 5, "4 \"0b101\" is 5");
    __64_from_string(&c, "0x1f");
    want(c.u64 == 31, "4 \"0x1f\" is 31");
#endif
    (void)a; (void)b; (void)d;
    wtof("TST64M VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
