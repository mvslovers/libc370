#include <stddef.h>
#include <mvs/mutex.h>

#define MAXITEM 100

struct table {
    CLIBMUTX *mtx;               /* from mtxnew(), not static     */
    int      n;
    int      item[MAXITEM];
};

int tbl_add(struct table *t, int v)
{
    int rc = -1;

    mtxlock(t->mtx);
    if (t->n < MAXITEM) {
        t->item[t->n++] = v;
        rc = 0;
    }
    mtxunlk(t->mtx);
    return rc;
}

int tbl_pair(struct table *t, int a, int b)
{
    int rc;

    mtxlock(t->mtx);             /* a mutex may be locked again   */
    rc = tbl_add(t, a);        /* by the task that holds it     */
    if (rc == 0)
        rc = tbl_add(t, b);
    mtxunlk(t->mtx);
    return rc;
}

int tbl_init(struct table *t)
{
    t->n   = 0;
    t->mtx = mtxnew();
    return t->mtx ? 0 : -1;
}

void tbl_term(struct table *t)
{
    if (t->mtx) {
        mtxfree(t->mtx);         /* only when no task holds it    */
        t->mtx = NULL;
    }
}
