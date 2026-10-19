/*
 * tsttodm.c - libc370 #390 on MVS: the TOD clock in seconds past 2038.
 *
 * __getclk(), behind time(), divided the microseconds since 1970 with the
 * signed 64/32 D, which raises a fixed-point divide exception (S0C9) once
 * the quotient leaves 31 bits: every time() from 2038-01-19 03:14:08 UTC
 * on.  It also dropped 0.32128 s of the 1900-1970 offset.
 *
 * The clock of the system cannot be set to 2038, so the conversion runs
 * on synthetic TOD values:
 *
 *   GREEN (this tree): __todsec(), the conversion __getclk() uses now.
 *   RED (-DRED): the instruction sequence of the 2.21.0 @@GETCLK, copied
 *   here verbatim, under try().  2.21.0 has no separate conversion.
 *
 *   1 2038-01-19 03:14:07 is 2147483647
 *   2 2038-01-19 03:14:08 is 2147483648        (red: S0C9)
 *   3 2042-09-17 23:53:47, the TOD clock's last second, is 2294610827
 *   4 0.7 s after 1970-01-01 00:00:00 is 0      (red: 1)
 *   5 control: time() agrees with the conversion of a STCK taken next to
 *     it (green only)
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tsttodm.c \
 *         -o $S/TSTTODG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DRED test/mvs/tsttodm.c \
 *         -o $S/TSTTODR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTTODG=TSTTODG.iebcopy \
 *         TSTTODR=TSTTODR.iebcopy -o td -xmit --dsn IBMUSER.LIBC370.TDSCR
 *
 * then jcl/recvtodm.jcl and jcl/tsttodm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <time.h>
#include <mvs/wto.h>
#include <mvs/clock.h>
#include <mvs/recovery.h>

static int      bad;
static unsigned result;

static void want(int cond, const char *what)
{
    wtof("TSTTODM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

#ifdef RED
/* the 2.21.0 @@GETCLK after its STCK */
static unsigned conv(const unsigned *t)
{
    unsigned r;

    __asm__("L     4,0(%1)\n"
"         L     5,4(%1)\n"
"         SRDL  4,12\n"
"         SL    4,=X'0007D910'\n"
"         D     4,=F'1000000'\n"
"         SL    5,=F'1220'\n"
"         LR    %0,5"
        : "=r"(r) : "r"(t) : "4", "5");
    return r;
}
#else
#define conv(t) __todsec(t)
#endif

static int run(const unsigned *t)
{
    result = conv(t);
    return 0;
}

static void at(unsigned hi, unsigned lo, unsigned want_s, const char *what)
{
    unsigned    t[2];
    int         rc;

    t[0] = hi;
    t[1] = lo;
    result = 0xFFFFFFFFU;
    rc = try(run, t);
    wtof("TSTTODM   %s: rc %08X, %u", what, (unsigned)rc, result);
    want(rc == 0 && result == want_s, what);
}

int main(void)
{
    at(0xF7A3048AU, 0xD5DC0000U, 2147483647U, "2038-01-19 03:14:07");
    at(0xF7A3048BU, 0xCA000000U, 2147483648U, "2038-01-19 03:14:08");
    at(0xFFFFFFFFU, 0xA58C0000U, 2294610827U, "2042-09-17 23:53:47");
    at(0x7D91048CU, 0x74E60000U, 0U, "1970-01-01 00:00:00.7");
#ifndef RED
    {
        unsigned    t[2];
        unsigned    a, b, c;

        a = (unsigned)time(NULL);
        b = __getclk(t);
        c = (unsigned)time(NULL);
        wtof("TSTTODM   time() %u, __getclk() %u, time() %u", a, b, c);
        want(a <= b && b <= c && b == __todsec(t),
             "time() agrees with the TOD clock");
    }
#endif
    return bad ? 8 : 0;
}
