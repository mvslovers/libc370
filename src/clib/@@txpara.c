/* @@TXPARA.C */
#include "mvs/dynalloc.h"
#include "ext/array.h"

int
__txpara(TXT99 ***txt99, const char *unused)
{
    int     err     = 1;
    TXT99   *tu;

    tu = NewTXT99(DALPARAL,0,0,0);
    if (!tu) goto quit;

    err = arrayadd(txt99, tu);

quit:
    return err;
}
