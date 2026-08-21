/* @@TXDCBD.C */
#include "svc99.h"
#include "clibary.h"

/* __txdcbd() - add text unit for a DCB model reference (DALDCBDS).
**
** This is the SVC 99 form of the JCL DCB=(dsname) model reference: MVS reads
** the named data set's DSCB and copies its DCB attributes -- DSORG, RECFM,
** LRECL, BLKSIZE, KEYLEN -- into the new allocation. Explicit DCB text units
** still override what the model supplies.
**
** It does NOT copy SPACE. Copying the space allocation as well arrived with
** DFSMS LIKE=, which does not exist on 3.8j: LIKE= on a DD statement is a JCL
** ERROR here, and key X'004B' is DALRSRVS ("secondary buffer reserve") in this
** era's table rather than DALLIKE, so sending it expecting LIKE semantics would
** quietly set a TCAM buffer size. Callers supply SPACE themselves.
*/
int
__txdcbd(TXT99 ***txt99, const char *dataset)
{
    int     err     = 1;
    int     len;
    TXT99   *tu;

    if (dataset) {
        len = strlen(dataset);
        if (len == 0 || len > 44) goto quit;

        tu = NewTXT99(DALDCBDS, 1, len, dataset);
        if (!tu) goto quit;

        err = arrayadd(txt99, tu);
        if (err) free(tu);
    }

quit:
    return err;
}
