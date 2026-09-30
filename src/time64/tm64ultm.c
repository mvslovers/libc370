#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/time64.h>
#include "src/time64/calendar.h"
#include "stddef.h"
#include "mvs/crt.h"

struct tm *ulocaltime64(const utime64_t *utimer)
{
    CLIBCRT     *crt    = __crtget();
    struct tm   *tms    = (struct tm*)crt->crttms;
	time64_t	timer;
	
	__64_div_u32((utime64_t *)utimer, 1000000, &timer);

	return localtime64_r(&timer, tms);
}

