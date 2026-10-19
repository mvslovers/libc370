/* TIMEGM.C - timegm(): a broken-down UTC time to seconds since 1970.
**
** The inverse of gmtime(), in 32 bits (#392).  This is the arithmetic
** mktime() has always done - mktime() applied no time zone - under the
** name that says so.  Code that means UTC calls timegm(), so that
** mktime() can later read struct tm as local time without changing it.
**
** Range 1970 to 2105, as mktime(): -1, and *timeptr unchanged, outside.
** Inside it the fields are normalised and tm_wday/tm_yday filled in, by
** writing back what gmtime_r() makes of the result.
*/
#include <time.h>
#include <ext/time64.h>

/* scalar date routines -- public domain by Ray Gardner (src/time/) */
extern long __ymdts(unsigned yr, unsigned mo, unsigned day);

#define EPOCH   719163  /* Unix EPOCH 1970-01-01 */

time_t timegm(struct tm *timeptr)
{
    time_t tt;

    /* 70 = 1970 Unix EPOCH || 205 = 2105 (roll over in 2106) */
    if ((timeptr->tm_year < 70) || (timeptr->tm_year > 205))
        return (time_t)-1;

    tt = __ymdts(timeptr->tm_year + 1900,
                 timeptr->tm_mon + 1,
                 timeptr->tm_mday)
         - EPOCH;
    tt = tt * 24 + timeptr->tm_hour;
    tt = tt * 60 + timeptr->tm_min;
    tt = tt * 60 + timeptr->tm_sec;

    /* gmtime_r(), not gmtime(): no CRT needed for the static buffer */
    gmtime_r(&tt, timeptr);
    return tt;
}
