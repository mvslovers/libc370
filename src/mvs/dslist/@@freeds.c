/* @@FREEDS.C - free array created by __listds() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "ext/array.h"        /* dynamic array prototypes     */
#include "mvs/dslist.h"       /* __listc()                    */

void
__freeds(DSLIST ***dslist)
{
    if (dslist && *dslist) {
        unsigned    count = arraycount(dslist);
        unsigned    n;

        if (count) {
            DSLIST  **list = *dslist;
            for(n=0; n < count; n++) {
                const char  *cat;
                unsigned    m;

                if (!list[n]) continue;

                /* the records of one catalog share its name (#50), and the
                   caller may have reordered them: free each name once, and
                   clear it in the records still to come */
                cat = list[n]->catnm;
                if (cat) {
                    for(m=n+1; m < count; m++) {
                        if (list[m] && list[m]->catnm == cat) list[m]->catnm = 0;
                    }
                    free((void *)cat);
                }
                free(list[n]);
                list[n] = 0;
            }
        }
        arrayfree(dslist);
        *dslist = 0;
    }
}
