/* @@EXIT.C */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stddef.h"
#include "mvs/crt.h"
#include "ext/array.h"

extern void __exita(int status);

void
__exit(int status)
{
    CLIBGRT     *grt    = __grtget();
    FILE        *fp;
    unsigned    count;
    void        (*func)(int,void*);

    if (!grt) goto quit;

    if (grt->grtexit) {
        /* call atexit()/on_exit() functions */
        count = arraycount(&grt->grtexit);
        /* we process the functions on a last in first out bases */
        while(count > 0) {
            count--;
            func = grt->grtexit[count];
            if (func) {
                func(status,grt->grtexita[count]);
            }
            grt->grtexit[count] = 0;
            grt->grtexita[count] = 0;
        }
        arrayfree(&grt->grtexit);
        arrayfree(&grt->grtexita);
    }

    /* from here on a standard stream that is still closed stays closed:
       stdin, stdout and stderr open on first use (#564), and closing the
       files must not open one.  The slots are read directly for the same
       reason - the stdin/stdout/stderr macros would open them. */
    grt->grtflag1 |= GRTFLAG1_STDEND;

    if (grt->grtfile) {
        /* close all open files */
        count = arraycount(&grt->grtfile);
        while(count > 0) {
            count--;
            fp = grt->grtfile[count];
            if (!fp) continue;

            fclose(fp);
            if (fp == grt->grtin)  grt->grtin  = 0;
            if (fp == grt->grtout) grt->grtout = 0;
            if (fp == grt->grterr) grt->grterr = 0;
        }
        arrayfree(&grt->grtfile);
    }

    if (grt->grtenv) {
        /* free environment variables */
        count = arraycount(&grt->grtenv);
        while(count > 0) {
            count--;
            if (grt->grtenv[count]) {
                free(grt->grtenv[count]);
                grt->grtenv[count] = 0;
            }
        }
        arrayfree(&grt->grtenv);
    }

    if (grt->grtwsa) {
        /* free writable static areas */
        count = arraycount(&grt->grtwsa);
        while(count > 0) {
            count--;
            if (grt->grtwsa[count]) {
                free(grt->grtwsa[count]);
                grt->grtwsa[count] = 0;
            }
        }
        arrayfree(&grt->grtwsa);
    }

    if (grt->grtdevtb) {
        /* free device table */
        count = arraycount(&grt->grtdevtb);
        while(count > 0) {
            count--;
            if (grt->grtdevtb[count]) {
                free(grt->grtdevtb[count]);
                grt->grtdevtb[count] = 0;
            }
        }
        arrayfree(&grt->grtdevtb);
    }

    if (grt->grtptrs) {
        /* the pointers in this array belong to the caller, so no free() */
        arrayfree(&grt->grtptrs);
    }

quit:
    __exita(status);
    return;
}
