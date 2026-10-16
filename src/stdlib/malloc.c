/* MALLOC.C */
#define STDLIB_C
#include "stdlib.h"
#include "signal.h"
#include "string.h"
#include "ctype.h"
#include "stddef.h"
#include "errno.h"
#include "mvs/storage.h"
#include "stdio.h"
#include "mvs/wto.h"
#include "src/internal/wtofmt.h"

extern void (*__userex[__NATEXIT])(void);

__PDPCLIB_API__ void *malloc(size_t size)
{
    void *ptr;

#define MAX_CHUNK	(6 * 1024 * 1024)	/* 6M */
    if (size > MAX_CHUNK) {
		ptr = NULL;
	}
	else {
		ptr = (__getm(size));
	}
	
    if (!ptr) {
        errno = ENOMEM;
        {
            /* without the printf family (#564): every program links
               malloc(), and wtof() made it link printf() too */
            char    msg[48];
            int     i = 0;

            wtofmt_str(msg, &i, sizeof(msg), "Out of memory, bytes needed=");
            wtofmt_dec(msg, &i, sizeof(msg), (unsigned)size);
            wto(msg);
        }
        wto_traceback(0);
    }
    return ptr;
}
