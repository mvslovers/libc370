/* @@TXSYSO.C */
#include "mvs/dynalloc.h"
#include "ext/array.h"
#include "ibm/mvs/iefzb4d2.h"

int
__txclos(TXT99 ***txt99, const char *unused)
{
    int     err     = 1;
    TXT99   *tu;

    tu = NewTXT99(DALCLOSE,0,0,0);
    if (!tu) goto quit;

    if (arrayadd(txt99, tu)) goto quit;

    err = 0;

quit:
    return err;
}
