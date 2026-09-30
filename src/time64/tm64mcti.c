#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/time64.h>
#include "src/time64/calendar.h"

char *mctime64( const mtime64_t* mtime ) {
	time64_t	time;

	/* convert 64 bit mseconds (mtime) to 64 bit seconds (time) value */
	__64_div_u32((mtime64_t *)mtime, 1000, &time);

    return asctime64(localtime64(&time));
}
