/* @@ERRNO.C */
#include "errno.h"
#include "mvs/crt.h"

static int  static_errno   = 0;

int *__errno(void)
{
    CLIBCRT *crt    = __crtget();
    int     *perr;

    if (crt) {
        perr = &crt->crterrno;
    }
    else {
        perr = &static_errno;
    }

    return perr;
}
