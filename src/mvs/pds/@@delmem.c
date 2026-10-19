/* @@DELMEM.C - delete a PDS member via STOW (delete) */
#include <stddef.h>
#include "mvs/pds.h"
#include "src/internal/pdsupd.h"

/*
 * __delmem() - delete member 'mem' from PDS 'dsn';
 * __delmemv() - the same on the PDS 'dsn' on volume 'vol', cataloged or
 * not (#143).
 *
 * STOW delete under a DISP=SHR allocation, the way ISPF removes a member;
 * never IDCAMS, never exclusive (#127, mvslovers/mvsmf#342).  See
 * src/mvs/pds/@@pdsupd.c.
 *
 * Only the directory entry is removed; the member's tracks are reclaimed
 * by the next compress, exactly as with IDCAMS.
 *
 * Returns:
 *    0  success
 *   >0  STOW return code (register 15), typically:
 *          8  the member was not found in the directory
 *         12  permanent I/O error
 *         20  insufficient virtual storage
 *   -1  invalid parameters or out of memory
 *   -2  the data set could not be allocated (not found, not on the
 *       volume, or in use)
 *   -3  the data set could not be opened
 */
int
__delmemv(const char *dsn, const char *vol, const char *mem)
{
    char    area[8];

    if (!dsn || !mem || *mem <= ' ') return -1;

    __pdsmem(area, mem);
    return __pdsupd(dsn, vol, area, 'D');
}

int
__delmem(const char *dsn, const char *mem)
{
    return __delmemv(dsn, NULL, mem);
}
