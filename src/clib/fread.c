/* FREAD.C */
#include "clibio.h"
#include "cliblock.h"

size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
    size_t rc;
    int    owned;

    owned = (lock(fp,0) == 0);  /* rc=8 = caller already holds it (#145) */

    rc = __fread(ptr,size,nmemb,fp);

    if (owned) unlock(fp,0);

    return rc;
}
