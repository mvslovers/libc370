/* CLAIM: take a free slot in a table that several tasks share */
#include <s370/atomic.h>

#define SLOT_FREE   0u
#define NSLOTS      16

static unsigned slot[NSLOTS];          /* fullwords: CS needs alignment */

/* returns the slot claimed for owner (not 0), or -1 if all are taken */
int claim(unsigned owner)
{
    int         i;
    unsigned    want;

    for (i = 0; i < NSLOTS; i++) {
        want = SLOT_FREE;
        if (__cas(&slot[i], &want, owner) == 0)
            return i;                   /* it was free, and is ours now */
        /* want holds the owner that got there first; try the next */
    }
    return -1;
}

void release(int i)
{
    __swap(&slot[i], SLOT_FREE);
}
