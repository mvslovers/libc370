/* JESVOL.C - find a data set by its name on the online volumes (#142)
 *
 * jesopen() falls back to this when SYS1.HASPCKPT (or the name given to
 * jesopen_ex()) is not in the catalog, which is the case on some systems:
 * JES2 opens its checkpoint through a DD with a volume serial and never
 * needed a catalog entry.  OBTAIN takes an exact name, so this finds the
 * data set wherever it is, but not a data set under another prefix.
 */
#include <errno.h>
#include <string.h>
#include "ext/array.h"      /* arraycount()                         */
#include "mvs/dscb.h"       /* __dscbdv(), DSCB                     */
#include "mvs/dslist.h"     /* __listvl(), __freevl(), VOLLIST      */
#include "src/mvs/jes2/jesvol.h"

int __jesvol(const char *dsn, char volser[7], unsigned *scanned)
{
    VOLLIST     **vl    = NULL;
    DSCB        dscb;
    unsigned    count   = 0;
    unsigned    n       = 0;
    int         rc      = -1;

    if (scanned) *scanned = 0;
    if (!dsn || !dsn[0] || strlen(dsn) > 44 || !volser) goto quit;

    vl = __listvl(NULL, 0, NULL);
    if (!vl) {
        /* NULL is an empty list as well as a failure (#61); an empty one
           is a plain "not found" */
        rc = errno ? -1 : 1;
        goto quit;
    }

    rc = 1;
    count = arraycount(&vl);
    for (n = 0; n < count && rc; n++) {
        if (__dscbdv(dsn, vl[n]->volser, &dscb) == 0) {
            memcpy(volser, vl[n]->volser, 6);
            volser[6] = 0;
            rc = 0;
        }
    }
    if (scanned) *scanned = n;
    __freevl(&vl);

quit:
    return rc;
}
