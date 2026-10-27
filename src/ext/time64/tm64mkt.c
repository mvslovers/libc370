/*
Copyright (c) 2007-2008  Michael G Schwern
This software originally derived from Paul Sheer's pivotal_gmtime_r.c.
The MIT License:
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/* See http://code.google.com/p/y2038 for this code's origin */
#if defined(__LP64__)
#error This cruft should be LP32 only!
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ext/time64.h>
#include "src/ext/time64/calendar.h"

__asm__("\n&FUNC    SETC 'mktime64'");
time64_t mktime64(struct tm *input_date) 
{
    struct tm safe_date;
    struct tm date;
    struct tm local;
    time64_t  time;
    int       year = input_date->tm_year + 1900;
    int       tz   = __tzget();
    uint64_t  seconds;

    __64_init(&time);

    if ((year < 1970) || (year > 9999)) {
        /* in keeping with our 32 bit mktime() function:
            disallow anything before 1970
            disallow anything after 9999
        */
        goto fail;
    }

    /* the fields read as UTC, on copies: *input_date stays as it was
       unless the whole conversion succeeds (#392) */
    date = *input_date;

    /* safe "years" can use the 32 bit timegm() function */
    if( MIN_SAFE_YEAR <= year && year <= MAX_SAFE_YEAR ) { 
        __64_from_u32(&time, timegm(&date));
    }
    else {
        /* Have to make the year safe in date else it won't fit in safe_date */
        date.tm_year = safe_year(year) - 1900;
        safe_date = date;

        /* initialize time using safe date */
        __64_from_u32(&time, (uint32_t) timegm(&safe_date));
        /* add the seconds between the input_date year and safe_date year */
        seconds = seconds_between_years(year, (int)(safe_date.tm_year + 1900));
        __64_add_u64(&time, seconds, &time);
    }

    /* struct tm is local time (#392): subtract the zone offset */
    if (tz > 0 && __64_cmp_u32(&time, (uint32_t)tz) == __64_SMALLER)
        goto fail;
    __64_sub_i32(&time, tz, &time);

    /* update the struct tm input_date with the normalised local time */
    if (localtime64_r(&time, &local) == NULL)
        goto fail;
    *input_date = local;
    return time;

fail:
    __64_init(&time);
    __64_sub_i32(&time, 1, &time);  /* make time -1 */
    return time;
}
