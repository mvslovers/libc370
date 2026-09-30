/* @@TXRLSE.C */
#include "mvs/dynalloc.h"
#include "libc370/array.h"

int
__txrlse(TXT99 ***txt99, const char *unused)
{
    int     err     = 1;
    TXT99   *tu;

    tu = NewTXT99(DALRLSE,0,0,0);
    if (!tu) goto quit;

    err = arrayadd(txt99, tu);

quit:
    return err;
}
