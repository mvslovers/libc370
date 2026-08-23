/* @@CTDEL.C - cthread_delete()
** delete a CTHDTASK handle
*/
#include "clibthrd.h"
#include "clibwto.h"     /* wtof(): a prototype decides linkage here (#39) */

__asm__("\n&FUNC    SETC 'cthread_delete'");
void
cthread_delete(CTHDTASK **task)
{
    CLIBGRT     *grt    = __grtget();
    CTHDTASK    **array;
    unsigned    count;
    unsigned    n;

    if (!task) goto quit;
    if (!*task) goto quit;
    if (strcmp((*task)->eye, CTHDTASK_EYE)!=0) goto quit;

    /* Refuse the whole delete -- not just the DETACH -- while the subtask is
    ** still running.  free(*task) releases the stack, because newthread()
    ** allocates it INSIDE this handle (@@ctcrtx.c: calloc(1, sizeof(CTHDTASK)
    ** + newstack)).  Gating only cthread_detach() would leave the free()
    ** below reachable, which is the half that actually corrupts (#11).
    ** *task is left non-NULL so the caller keeps the handle it must retain.
    */
    if ((*task)->tcb && !((*task)->termecb & 0x40000000)) {
        wtof("cthread_delete(%08X): TCB(%06X) has not ended, "
            "task and stack retained", *task, (*task)->tcb);
        goto quit;
    }

    lock(&grt->grtcthrd,0);
    array = grt->grtcthrd;
    count = arraycount(&array);
    for(n=0; n < count; n++) {
        if (!array[n]) continue;
        if (array[n] == *task) {
            arraydel(&grt->grtcthrd, n+1);
            break;
        }
    }
    unlock(&grt->grtcthrd,0);

    /* detach the thread */
    cthread_detach(*task);

    free(*task);
    *task = 0;

quit:
    return;
}
