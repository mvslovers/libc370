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
        wtof("Out of memory, bytes needed=%u", size);
        wto_traceback(0);
    }
    return ptr;
}
