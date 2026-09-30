/* @@DSALCF.C */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include "mvs/dynalloc.h"
#include "ext/array.h"
#include "mvs/crt.h"
#include "stdio.h"

int __dsalcf(char *ddname, const char *opts, ...)
{
    int     rc  = 4;
    va_list list;
    char    temp[512];

    if (!opts) goto quit;
    
    va_start(list, opts);
    vsnprintf(temp, sizeof(temp), opts, list);
    va_end(list);

    temp[sizeof(temp)-1] = 0;
    
    rc = __dsalc(ddname, temp);

quit:    
    return rc;
}
