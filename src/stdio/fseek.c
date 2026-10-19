/* FSEEK.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

int
fseek(FILE *fp, long int offset, int whence)
{
    int     rc;
    int     owned;
    unsigned short held;

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    /* C99 7.19.9.2: a successful fseek() clears the end-of-file indicator
       and leaves the error indicator alone; only rewind() clears both
       (#178).  __fseek() clears every indicator for its own reads and
       reopens, so the error indicator - with _FILE_FLAG_ENOSPC, which
       rides with it (#149) - is put back here.  The return value is the
       seek's own. */
    held = fp->flags & (_FILE_FLAG_ERROR | _FILE_FLAG_ENOSPC);
    rc = __fseek(fp,offset,whence);
    fp->flags |= held;

    if (owned) unlock(fp,0);

    return rc;
}
