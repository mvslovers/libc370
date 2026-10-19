/* FFLUSH.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"
#include "mvs/crt.h"
#include "ext/array.h"
#include <stdlib.h>
#include <string.h>

/* is fp still one of the program's open streams? */
__asm__("\n&FUNC    SETC 'isopen'");
static int
isopen(CLIBGRT *grt, FILE *fp)
{
    unsigned    i, n;
    int         found = 0;

    lock(&grt->grtfile, 0);
    n = arraycount(&grt->grtfile);
    for (i = 0; i < n && !found; i++) found = (grt->grtfile[i] == fp);
    unlock(&grt->grtfile, 0);
    return found;
}

/* fflush(NULL): every stream open for writing, C99 7.19.5.2 (#395).  The
   list is copied first and each stream locked before it is looked up
   again - the FILE lock, then the list's, the order fclose() takes them.
   A stream closed in the meantime is no longer on the list; its lock is
   only an ENQ on the pointer value, which is safe to take. */
__asm__("\n&FUNC    SETC 'flushall'");
static int
flushall(void)
{
    CLIBGRT     *grt    = __grtget();
    FILE        **copy;
    unsigned    i, n;
    int         err     = 0;
    int         owned;

    if (!grt || !grt->grtfile) return 0;
    lock(&grt->grtfile, 0);
    n = arraycount(&grt->grtfile);
    copy = n ? malloc(n * sizeof(FILE *)) : NULL;
    if (copy) memcpy(copy, grt->grtfile, n * sizeof(FILE *));
    unlock(&grt->grtfile, 0);
    if (!copy) return n ? EOF : 0;

    for (i = 0; i < n; i++) {
        owned = __flock(copy[i]);
        if (isopen(grt, copy[i]) && (copy[i]->flags & _FILE_FLAG_WRITE)
                && __fflush(copy[i])) err = EOF;
        if (owned) unlock(copy[i], 0);
    }
    free(copy);
    return err;
}

int
fflush(FILE *fp)
{
    int             err     = 0;
    int             owned;

    /* NULL read low storage as a FILE (#395) */
    if (!fp) return flushall();

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    err = __fflush(fp);

    if (owned) unlock(fp,0);

    return err;
}
