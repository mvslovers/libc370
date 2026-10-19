/* @@RENMEM.C - rename a PDS member via STOW (change) */
#include <stddef.h>
#include "mvs/pds.h"
#include "src/internal/pdsupd.h"

/*
 * __renmem() - rename member 'oldmem' to 'newmem' in PDS 'dsn';
 * __renmemv() - the same in the PDS 'dsn' on volume 'vol', cataloged or
 * not (#143).
 *
 * Uses STOW change, so the member's TTR and directory user data (ISPF
 * statistics) are preserved and no data is copied - this works for every
 * record format including RECFM=U load modules.  See
 * src/mvs/pds/@@pdsupd.c.
 *
 * Returns:
 *    0  success
 *   >0  STOW return code (register 15), typically:
 *          4  the new name already exists in the directory
 *          8  the old name was not found in the directory
 *         12  permanent I/O error
 *         16  no space left in the directory
 *         20  insufficient virtual storage
 *   -1  invalid parameters or out of memory
 *   -2  the data set could not be allocated (not found, not on the
 *       volume, or in use)
 *   -3  the data set could not be opened
 */
int
__renmemv(const char *dsn, const char *vol, const char *oldmem,
          const char *newmem)
{
    char    area[16];

    if (!dsn || !oldmem || !newmem) return -1;
    if (*oldmem <= ' ' || *newmem <= ' ') return -1;

    /* old name (8) + new name (8) */
    __pdsmem(area, oldmem);
    __pdsmem(area + 8, newmem);
    return __pdsupd(dsn, vol, area, 'C');
}

int
__renmem(const char *dsn, const char *oldmem, const char *newmem)
{
    return __renmemv(dsn, NULL, oldmem, newmem);
}
