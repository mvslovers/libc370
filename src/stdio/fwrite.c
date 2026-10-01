/* FWRITE.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

size_t
fwrite(const void *vptr, size_t size, size_t nmemb, FILE *fp)
{
    size_t  rc;
    int     owned;

    owned = (lock(fp,0) == 0);  /* rc=8 = caller already holds it (#145) */

    rc = __fwrite(vptr, size, nmemb, fp);

    if (owned) unlock(fp,0);

    return rc;
}
