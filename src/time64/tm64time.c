#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time64.h>

__asm__("\n&FUNC    SETC 'time64'");
time64_t time64(time64_t *timer)
{
    time64_t tt;

    /* clock64() is seconds since #49, so there is nothing left to scale.
     * This used to divide by CLOCKS_PER_SEC, which cancelled clock64()'s
     * millisecond divisor and made time64() come out right by way of two
     * wrong constants.  Both moved together; time64()'s VALUE is unchanged.
     * CLOCKS_PER_SEC describes clock(), which this library does not
     * implement (src/clib/clock.c returns -1), and is no longer read here.
     */
    tt.u64 = clock64();

	if (timer) *timer = tt;

    return tt;
}

