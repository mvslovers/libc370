/* @@DDDUM.C - is a DD a DUMMY? (#277)
**
** Out of @@fpopen.c, so that the SYSENV reader (@@envdd.c) asks the same
** question without linking fopen().
*/
#include "src/internal/fileio.h"
#include "ibm/mvs/iefjfcbn.h"
#include <mvs/dd.h>
#include "ibm/mvs/ieftiot1.h"

/* Is DD ddname a DUMMY?  Its JFCB names NULLFILE, whether the DD is a JCL
   DD DUMMY or 'NULLFILE' allocated by name.  Read through the TIOT entry's
   SWA pointer, as __listal() reads it, so it is known BEFORE the OPEN -
   __rdjfcb() needs an open DCB (#277). */
int
__dddum(const char *ddname)
{
    static const char nullfile[9] = "NULLFILE ";
    DSAB    *dsab   = get_dsab(0, ddname);
    TIOTDD  *tiotdd;
    JFCB    *jfcb;
    int     i;

    if (!dsab || !(tiotdd = dsab->dsabtiot)) return 0;
    jfcb = (JFCB *)(((unsigned char)tiotdd->TIOEJFCB[0] << 16
                     | (unsigned char)tiotdd->TIOEJFCB[1] << 8
                     | (unsigned char)tiotdd->TIOEJFCB[2]) + 16);
    for (i = 0; i < 9; i++) {
        if (jfcb->jfcbdsnm[i] != nullfile[i]) return 0;
    }
    return 1;
}
