/* MKTIME.C */
#include <stddef.h>
#include <time.h>
#include <ext/time64.h>

/* mktime() reads struct tm as local time: the fields minus the zone offset
   __tzget() (#392).  Out of range (fields before 1970 or after 2105, or a
   result outside 0..2**32-2): -1, and the caller's struct tm stays as it
   was.  The fields are normalised by writing back what localtime_r() makes
   of the result, which is why mktime(localtime(&t)) == t. */
__PDPCLIB_API__ time_t mktime(struct tm *timeptr)
{
    struct tm     tmp = *timeptr;
    unsigned long t;
    unsigned long n;
    int           tz;
    time_t        tt;

    t = (unsigned long)timegm(&tmp);
    if (t == (unsigned long)(time_t)-1) return (time_t)-1;

    tz = __tzget();
    if (tz >= 0) {
        n = (unsigned long)tz;
        if (t < n) return (time_t)-1;
        t -= n;
    } else {
        n = (unsigned long)-(long)tz;
        if (n > 0xFFFFFFFEUL - t) return (time_t)-1;
        t += n;
    }

    tt = (time_t)t;
    localtime_r(&tt, timeptr);
    return tt;
}
