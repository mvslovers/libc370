/* @@FREEPD.C - free array created by __listpd() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "ext/array.h"        /* dynamic array prototypes     */
#include "mvs/dslist.h"       /* __listpd()                   */

void
__freepd(PDSLIST ***pdslist)
{
    if (pdslist && *pdslist) {
        unsigned    count = arraycount(pdslist);
        unsigned    n;

        if (count) {
            PDSLIST **list = *pdslist;
            for(n=0; n < count; n++) {
                if (!list[n]) continue;
                free(list[n]);
                list[n] = 0;
            }
        }
        arrayfree(pdslist);
        *pdslist = 0;
    }
}
