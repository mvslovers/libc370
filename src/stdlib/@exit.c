/* @EXIT.C - _Exit() */
#include <stdlib.h>
#include <stddef.h>
#include "mvs/crt.h"
#include "ext/array.h"

void __exit(int status) __attribute__((noreturn));

/* C99 7.20.4.4: no atexit() or on_exit() function runs.  The
   registrations are dropped, as __ppahrv() drops a dead program's, and
   the rest of the teardown is left to __exit(): it closes the streams
   (whether they are flushed is implementation-defined) and frees the
   runtime's storage, which matters wherever the runtime does not end
   with its task. */
__PDPCLIB_API__ void _Exit(int status)
{
    CLIBGRT *grt = __grtget();

    if (grt) {
        if (grt->grtexit)  arrayfree(&grt->grtexit);
        if (grt->grtexita) arrayfree(&grt->grtexita);
    }
    __exit(status);
}
