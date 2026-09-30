#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/time64.h>
#include "src/time64/calendar.h"
#include "stddef.h"
#include "mvs/crt.h"

__asm__("\n&FUNC    SETC 'gmtime64'");
struct tm *gmtime64(const time64_t *timer)
{
    CLIBCRT     *crt    = __crtget();
    struct tm   *tms    = (struct tm*)crt->crttms;

	return gmtime64_r(timer, tms);
}

