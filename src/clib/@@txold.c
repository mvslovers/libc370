/* @@TXOLD.C */
#include "mvs/dynalloc.h"
#include "libc370/array.h"

int
__txold(TXT99 ***txt99, const char *unused)
{
    int     err     = 1;
    TXT99   *tu;

    tu = NewTXT99(DALSTATS,1,1,"\x01");     /* DISP=OLD */
    if (!tu) goto quit;

    if (arrayadd(txt99, tu)) goto quit;

    err = 0;

quit:
    return err;
}
