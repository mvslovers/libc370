#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libc370/time64.h>
#include "__time64.h"
#include "stddef.h"
#include "mvs/crt.h"

struct tm *mlocaltime64(const mtime64_t *mtimer)
{
    CLIBCRT     *crt    = __crtget();
    struct tm   *tms    = (struct tm*)crt->crttms;
	time64_t	timer;
	
	__64_div_u32((mtime64_t *)mtimer, 1000, &timer);

	return localtime64_r(&timer, tms);
}
