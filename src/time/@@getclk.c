/* @@GETCLK.C */
#include "mvs/clock.h"

/* __todsec() - the seconds since 1970-01-01 00:00:00 of a TOD clock value
** (#390).  The TOD clock counts microseconds since 1900 in its bits 0-51.
** The conversion uses 32-bit arithmetic only: the 64/32 divide D is signed
** and faults once the quotient leaves 31 bits, which a single division of
** the microseconds did from 2038-01-19 03:14:08 UTC on.  Here every
** quotient stays below 2**16, and the result is right up to the end of
** the TOD clock itself, 2042-09-17 23:53:47 UTC, where it wraps to 1900. */
unsigned int
__todsec(const void *tod)
{
    const unsigned  *t  = (const unsigned *)tod;
    unsigned        uh, ul, n, q, r;

    /* microseconds since 1900: the clock shifted right by 12 */
    uh = t[0] >> 12;
    ul = (t[0] << 20) | (t[1] >> 12);

    /* minus 1900-01-01 to 1970-01-01: 2208988800 s, X'7D91048BCA000' us */
    if (ul < 0x48BCA000U) uh--;
    ul -= 0x48BCA000U;
    uh -= 0x0007D910U;

    /* us / 1000000 = (us / 64) / 15625.  us / 64 is below 2**46 and the
       seconds below 2**32, so it divides by 15625 in two steps whose
       quotients are below 2**16. */
    ul = (uh << 26) | (ul >> 6);
    uh >>= 6;
    n = (uh << 16) | (ul >> 16);
    q = n / 15625;
    r = n % 15625;
    n = (r << 16) | (ul & 0xFFFF);
    return (q << 16) | (n / 15625);
}

/* __getclk() - store the TOD clock in the 8 bytes at buf, which need no
** alignment, and return the seconds since 1970. */
unsigned int
__getclk(void *buf)
{
    __asm__("STCK 0(%0)" : : "r"(buf) : "memory");
    return __todsec(buf);
}
