/* MKTIME.C */
#include <stddef.h>
#include <time.h>
#include <ext/time64.h>

/* mktime() reads struct tm as UTC, as it always has: it is timegm(), and
   applies no time zone (#392).  Code that means UTC calls timegm(); a
   later release makes mktime() read struct tm as local time.  Out of
   range (before 1970, after 2105): -1, and the caller's struct tm stays
   as it was. */
__PDPCLIB_API__ time_t mktime(struct tm *timeptr)
{
    return timegm(timeptr);
}
