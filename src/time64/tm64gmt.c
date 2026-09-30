#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libc370/time64.h>
#include "__time64.h"
#include "mvssupa.h"
#include "mvs/crt.h"

__asm__("\n&FUNC    SETC 'gmtime64'");
struct tm *gmtime64(const time64_t *timer)
{
    CLIBCRT     *crt    = __crtget();
    struct tm   *tms    = (struct tm*)crt->crttms;

	return gmtime64_r(timer, tms);
}

