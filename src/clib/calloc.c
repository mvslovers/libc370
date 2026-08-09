/* CALLOC.C */
#define STDLIB_C
#include "stdlib.h"
#include "signal.h"
#include "string.h"
#include "ctype.h"
#include "stddef.h"
#include "errno.h"
#include "mvssupa.h"

__PDPCLIB_API__ void *calloc(size_t nmemb, size_t size)
{
    size_t total;
    void *ptr;

    /* refuse a product a 32-bit size_t cannot hold: the old 24-bit
     * mask turned such requests into tiny valid allocations that the
     * caller then overran (#84).  malloc() itself refuses anything
     * over its 6 MB cap. */
    if (size != 0 && nmemb > 0xFFFFFFF8u / size) {
        errno = ENOMEM;
        return NULL;
    }
    total = ((nmemb * size) + 7) & 0xFFFFFFF8;

    ptr = malloc(total);
    if (ptr) {
        /* clear allocated memory [memset(ptr, 0, total)] */
        __asm__("\n"
"* Clear allocated memory\n"
"         LR    14,%0   => ptr\n"
"         LR    15,%1   == total size\n"
"         SLR   0,0\n"
"         LR    1,0\n"
"         MVCL  14,0\n" : : "r"(ptr), "r"(total) : "0", "1", "14", "15");
    }

    return (ptr);
}
