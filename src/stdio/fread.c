/* FREAD.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
    size_t rc;
    int    owned;

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    rc = __fread(ptr,size,nmemb,fp);

    if (owned) unlock(fp,0);

    return rc;
}
