/* @@TXAUCA.C */
#include "mvs/dynalloc.h"
#include "libc370/array.h"

int
__txauca(TXT99 ***txt99, const char *unused)
{
    int     err     = 1;
    TXT99   *tu;

    tu = NewTXT99(DALCDISP,1,1,"\x01");
    if (!tu) goto quit;

    err = arrayadd(txt99, tu);

quit:
    return err;
}
