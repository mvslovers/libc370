/* @@TXINPU.C */
#include "mvs/dynalloc.h"
#include "ext/array.h"

int
__txinpu(TXT99 ***txt99, const char *unused)
{
    int     err     = 1;
    TXT99   *tu;

    tu = NewTXT99(DALINOUT,1,1,"\x80");
    if (!tu) goto quit;

    err = arrayadd(txt99, tu);

quit:
    return err;
}
