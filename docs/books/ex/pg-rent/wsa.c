#include <stdio.h>
#include <mvs/wsa.h>
#include <mvs/lock.h>

struct stats {                   /* the "static" data             */
    unsigned calls;
    unsigned hits;
    int      limit;
};

/* Initial values only: never written. Its address is the key.   */
static const struct stats stats0 = { 0, 0, 100 };

static struct stats *stats(void)
{
    return __wsaget((void *)&stats0, sizeof stats0);
}

int lookup(int key)
{
    struct stats *st = stats();
    int          rc;
    int          hit = (key % 3) == 0;

    if (!st)
        return -1;               /* no process anchor, no storage */

    rc = lock(st, LOCK_EXC);     /* the area is shared by threads */
    st->calls++;
    if (hit)
        st->hits++;
    if (rc == 0)
        unlock(st, LOCK_EXC);
    return hit;
}

int main(void)
{
    struct stats *st;
    int          i;

    for (i = 0; i < 10; i++)
        lookup(i);

    st = stats();
    if (!st)
        return 8;
    printf("%u calls, %u hits, limit %d\n", st->calls, st->hits, st->limit);
    return 0;
}
