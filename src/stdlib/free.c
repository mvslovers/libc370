/* FREE.C */
#define STDLIB_C
#include "src/internal/fileio.h"
#include "stdlib.h"
#include "signal.h"
#include "string.h"
#include "ctype.h"
#include "stddef.h"
#include "mvs/storage.h"

__PDPCLIB_API__ void free(void *ptr)
{
#if 0 /* debug free problem */
    char caller[256] = "";
    __caller(caller);
    wtof("free(%08X) called by %s", ptr, caller);
#endif /* debug free problem */
    if (ptr) {
        __freem(ptr);
    }

    return;
}
