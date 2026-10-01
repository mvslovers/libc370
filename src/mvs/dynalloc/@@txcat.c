/* @@TXCAT.C */
#include "mvs/dynalloc.h"
#include "ext/array.h"

int
__txcat(TXT99 ***txt99, const char *unused)
{
    int     err     = 1;
    TXT99   *tu;

    tu = NewTXT99(DALNDISP,1,1,"\x02");
    if (!tu) goto quit;

    err = arrayadd(txt99, tu);

quit:
    return err;
}
