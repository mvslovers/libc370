/* @@FRTX9A.C */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "mvs/dynalloc.h"
#include "ext/array.h"

void
__frtx9a(TXT99 ***txt99)
{
    if (txt99 && *txt99) {
        TXT99       **p     = *txt99;
        unsigned    count   = arraycount(&p);
        unsigned    n;

        for( n=0; n < count; n++ ) {
            /* the list's last pointer may still carry the end-of-list
               bit __dsalc() and __dsfree() set for SVC 99 */
            p[n] = (TXT99 *)((unsigned)p[n] & 0x7FFFFFFF);
            FreeTXT99(&p[n]);
        }
        arrayfree(txt99);
    }
}
