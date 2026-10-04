/* @@LISTPD.C - create PDSLIST array
**
** __walkpd() walks the directory; this keeps a calloc()ed copy of every
** entry it hands over.  When storage runs out the records collected so far
** are freed and the call answers NULL with errno ENOMEM: 1.x returned them,
** a short list indistinguishable from a complete one (#80, defect 3).
*/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "ext/array.h"        /* dynamic array prototypes     */
#include "mvs/dslist.h"       /* __listpd(), __walkpd()       */

typedef struct {
    PDSLIST     **array;
    int         failed;     /* storage ran out                  */
} COLLECT;

static int collect(void *arg, const PDSLIST *entry)
{
    COLLECT     *c      = arg;
    unsigned    size    = 12 + (entry->idc & PDSLIST_IDC_UDATA) * 2;
    PDSLIST     *copy   = calloc(1, size);

    if (!copy || arrayadd(&c->array, copy)) {
        free(copy);
        c->failed = 1;
        return 1;           /* stop the walk */
    }
    memcpy(copy, entry, size);
    return 0;
}

PDSLIST **
__listpd(const char *dataset, const char *filter)
{
    COLLECT     c       = { 0, 0 };
    int         rc;

    errno = 0;      /* an empty NULL is not a failure (#61) */
    rc = __walkpd(dataset, filter, collect, &c);

    if (rc < 0 || c.failed) {
        int err = c.failed ? ENOMEM : errno;
        if (c.array) __freepd(&c.array);
        errno = err;
        return 0;
    }
    return c.array;
}
