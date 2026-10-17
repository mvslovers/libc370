/* LOADENV.C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "mvs/env.h"
#include "src/internal/envld.h"

int
loadenv(const char *fn)
{
    int     err     = 1;
    FILE    *fp     = fopen(fn,"r");
    char    *p;
    char    buf[1024];

    if (!fp) goto quit;

    while(fgets(buf, sizeof(buf), fp)) {
        p = strrchr(buf, '\n');     /* get last newline character in buf    */

        if (p) *p = 0;              /* remove the trailing newline          */
        __envln(buf);
    }

    err = 0;

quit:
    if (fp) fclose(fp);
    return err;
}
