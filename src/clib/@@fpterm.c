/* @@FPTERM.C - the teardown tail shared by fclose() and __fabandon().
**
** Everything after the DCB is closed (or abandoned) is bookkeeping, and it
** is the same bookkeeping either way: drop the buffer, unallocate the DD
** fopen() created, take the FILE out of grt->grtfile, free it, release the
** FILE lock.  #145 and #147 both live in this sequence - the lock has to
** span it and the grtfile lock has to be taken exactly once - so it exists
** here once instead of twice.
**
** The caller holds the FILE lock.  `unlk` says whether to release it:
** fclose() passes what its own lock() answered (rc=8 means an outer caller
** owns it and fclose() must not DEQ), __fabandon() always passes 1, for the
** reason spelled out in @@faband.c.
*/
#include <stdio.h>
#include <stdlib.h>
#include "clibcrt.h"
#include "cliblock.h"
#include "clibary.h"

extern int  __fpfree(FILE *fp);

int
__fpterm(FILE *fp, int unlk)
{
    CLIBGRT *grt    = __grtget();
    unsigned count;
    int      rc     = 0;

    if (fp->buf) {
        free(fp->buf);
        fp->buf     = 0;
        fp->upto    = 0;
        fp->endbuf  = 0;
    }

    if (fp->flags & _FILE_FLAG_DYNAMIC) {
        /* deallocate the dataset.  rc is handed back rather than dropped:
           an SVC 99 that answers 4 because a DCB is still open on the DD
           is exactly what #168 needs to report */
        rc = __fpfree(fp);
    }

    /* remove file handle from array of open file handles */
    lock(&grt->grtfile,0);
    count = arraycount(&grt->grtfile);
    while(count > 0) {
        count--;
        if (grt->grtfile[count]==fp) {
            arraydel(&grt->grtfile, count+1);
            break;
        }
    }
    unlock(&grt->grtfile,0);

    free(fp);

    /* after free(fp) on purpose: the lock rname is built from the
       pointer VALUE, unlock() never dereferences it.  A waiter that
       acquires now and uses the freed FILE was always a caller error */
    if (unlk) unlock(fp,0);

    return rc;
}
