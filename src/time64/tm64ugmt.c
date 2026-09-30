#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libc370/time64.h>
#include "src/time64/__time64.h"
#include "stddef.h"
#include "mvs/crt.h"

struct tm *ugmtime64(const utime64_t *utimer)
{
    CLIBCRT     *crt    = __crtget();
    struct tm   *tms    = (struct tm*)crt->crttms;
    time64_t	timer;
    
    __64_div_u32((utime64_t *)utimer, 1000000, &timer);

	return gmtime64_r(&timer, tms);
}

