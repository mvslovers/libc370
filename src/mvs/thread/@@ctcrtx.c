/* @@CTCRTX.C - cthread_create_ex()
** create a thread (subtask) instance with stack size
** returns CTHDTASK handle or NULL on error.
*/
#include <mvs/wto.h>
#include <stdlib.h>
#include <stddef.h>
#include "mvs/thread.h"
#include "mvs/lock.h"
#include "mvs/crt.h"
#include "ext/array.h"
#include "mvs/mutex.h"
#include "src/internal/wtofmt.h"

static CTHDTASK *newthread(unsigned tcb, unsigned owntcb, unsigned stacksize);
static int attach(CTHDTASK *task);
static int dummy(void *arg1, void *arg2);

__asm__("\n&FUNC    SETC 'cthread_create_ex'");
CTHDTASK *
cthread_create_ex(void *func, void *arg1, void *arg2, unsigned stacksize)
{
    unsigned    *psa    = (unsigned*)0;         /* MVS prefixed save area */
    unsigned    tcb     = psa[0x21C/4];         /* get our TCB address */
    CTHDTASK    *owner  = cthread_find(tcb);    /* get owner thread */
    CTHDTASK    *task   = NULL;
    int         rc;

    /* wtof("%s(%08X, %08X, %08X, %u)", __func__, func, arg1, arg2, stacksize); */

    if (!stacksize) stacksize = (64 * 1024);    /* default 64K stack */
    if (!func) func = dummy;

    /* wtodumpf(func, 32, "FUNC"); */

    if (!owner) {
        /* we do this so that the main() thread will have
        ** CTHDTASK handle as a parent/owner for the new thread.
        */

        /* create the owner thread handle */
        owner = newthread(tcb, 0, 0);
        if (!owner) goto quit;      /* should never happen */
    }

    /* we create a new task handle */
    task = newthread(0, owner->tcb, stacksize);
    if (!task) goto quit;           /* must be out of memory! */

    task->func      = func;         /* thread function */
    task->arg1      = arg1;         /* thread arg1 */
    task->arg2      = arg2;         /* thread arg2 */

    /* attach our subtask driver module.
    ** we use try() to catch any abend caused by an ATTACH failure.
    */
#if 1
    rc = try(attach, task);
#else
    rc = attach(task);
#endif
#if 0   /* debug code */
    if (rc==0) {
        wtof("%s NEWTCB(%06X)", __func__, task->tcb);
    }
#endif
    if (rc) {
        /* well that's unexpected */
        task->tcb = 0;  /* make sure we clear the TCB address */
        cthread_delete(&task);
    }

quit:
    return task;
}

/* ATTACH names CTHREAD only as a string, which links nothing.  This hard
** reference pulls the subtask driver out of libc.a, and with it @@CRT0's
** weak reference resolves and the startup IDENTIFYs CTHREAD (#159). */
__asm__("\n\tEXTRN CTHREAD");

__asm__("\n&FUNC    SETC 'attach'");
static int
attach(CTHDTASK *task)
{
    unsigned    work[16]    = {0};
    int         arc;

    /* attach our subtask driver module.
    ** the task->tcb is created here if the attach was a success.
    **
    ** The subtask starts at once and, with a second CPU, can run to its end
    ** before this code has stored ATTACH's results.  Its function's return
    ** code lands in task->rc (CTHDRC), so ATTACH's own return code must not
    ** go there: stored over it, it erased the thread's return code, and read
    ** back, it made a thread that returned nonzero look like a failed ATTACH
    ** -- whose cleanup frees the CTHDTASK, ECB and stack under a subtask
    ** that is still ending (S43E, SA03, S0C4; #465).
    */
    __asm__(
        "LA\t1,0(,%2)\n\t"
        "ATTACH EP=CTHREAD,ECB=(%3),DPMOD=-1,SF=(E,(%4))\n\t"
        "ST\t1,%0\n\t"
        "ST\t15,%1"
        : "=m"(task->tcb), "=m"(arc)
        : "r"(task), "r"(&task->termecb), "r"(work)
        : "0", "1", "14", "15" , "memory");

    return arc;
}

__asm__("\n&FUNC    SETC 'newthread'");
static CTHDTASK *
newthread(unsigned tcb, unsigned owntcb, unsigned stacksize)
{
    CLIBGRT     *grt    = __grtget();
    unsigned    newstack;
    CTHDTASK    *task;

    /* 0 is the owner's handle, which needs no stack.  Otherwise at least
    ** 1 KB: the driver's save area alone is X'58' bytes.  The size was
    ** masked to 20 bits, so 1 MB gave 0 and 1 MB + 64 KB gave 64 KB, and
    ** sizes up to 79 were raised to 80 (#403). */
    if (stacksize == 0)                 newstack = 0;
    else if (stacksize < 1024)          newstack = 1024;
    else if (stacksize > 0x00FFFFF0)    return NULL;
    else                                newstack = (stacksize + 7) & ~7u;

    task = calloc(1, sizeof(CTHDTASK) + newstack);

    if (!grt) {
        /* no GRT: an unregistered thread would be invisible to
         * cthread_find() and cleanup (#85) */
        free(task);
        return NULL;
    }

    if (task) {
        strcpy(task->eye, CTHDTASK_EYE);
        task->tcb       = tcb;
        task->owntcb    = owntcb;
        task->stacksize = newstack;
        lock(&grt->grtcthrd,0);
        if (arrayadd(&grt->grtcthrd, task)) {
            /* not registered: cthread_find()/cleanup would never
             * see it, fail the create instead (#85) */
            unlock(&grt->grtcthrd,0);
            free(task);
            return NULL;
        }
        unlock(&grt->grtcthrd,0);
    }

    return task;
}

/* "<a>XXXXXXXX<b>XXXXXXXX<c>"; the second value only when c is not NULL */
__asm__("\n&FUNC    SETC 'hexline'");
static void
hexline(const char *a, unsigned v1, const char *b, unsigned v2, const char *c)
{
    char    msg[80];
    int     k = 0;

    wtofmt_str(msg, &k, sizeof(msg), a);
    wtofmt_hex(msg, &k, sizeof(msg), v1, 8);
    if (c) {
        wtofmt_str(msg, &k, sizeof(msg), b);
        wtofmt_hex(msg, &k, sizeof(msg), v2, 8);
        wtofmt_str(msg, &k, sizeof(msg), c);
    }
    wto(msg);
}

__asm__("\n&FUNC    SETC 'dummy'");
static int
dummy(void *arg1, void *arg2)
{
    CTHDTASK    *task   = cthread_self();

    /* without printf (size analysis F4): every threaded program links
       cthread_create_ex(), and with it this report */
    hexline("dummy(arg1=", (unsigned)arg1, ", arg2=", (unsigned)arg2, ")");
    hexline("CTHDTASK handle=", (unsigned)task, "", 0, NULL);
    if (task) {
        char    eye[40];
        int     k = 0;
        int     n;

        wtofmt_str(eye, &k, sizeof(eye), "task->eye         ");
        for (n = 0; n < 8; n++) eye[k++] = task->eye[n];
        eye[k] = 0;
        wto(eye);
        hexline("task->tcb         ", (unsigned)task->tcb, "", 0, NULL);
        hexline("task->owntcb      ", (unsigned)task->owntcb, "", 0, NULL);
        hexline("task->termecb     ", (unsigned)task->termecb, "", 0, NULL);
        hexline("task->rc          ", (unsigned)task->rc, "", 0, NULL);
        hexline("task->stacksize   ", (unsigned)task->stacksize, "", 0, NULL);
        hexline("task->func        ", (unsigned)task->func, "", 0, NULL);
        hexline("task->arg1        ", (unsigned)task->arg1, "", 0, NULL);
        hexline("task->arg2        ", (unsigned)task->arg2, "", 0, NULL);
    }

    return -1;
}

__asm__("\n&FUNC    SETC 'cleanup_thread'");
static void
cleanup_thread(void)
{
    CLIBCRT     *crt            = __crtget();
    unsigned    count;

    if (!crt) goto quit;    /* no thread level data, we're done */

    if (crt->crtpush) {
        /* cleanup the cthread_push() array's */
        count = arraycount(&crt->crtpush);
        while(count) {
            /* pop and execute in a try() wrapper */
            cthread_pop(CTHDPOP_TRY);
            count--;
        }
        /* cthread_push() array's should be empty now */
        arrayfree(&crt->crtpush);
        arrayfree(&crt->crtargs);
    }

    if (crt->crtmutx) {
        /* cleanup the mutex lock array */
        count = arraycount(&crt->crtmutx);
        while(count) {
            CLIBMUTX *x = arraydel(&crt->crtmutx, count);
            if (x) mtxclup(x);
            count--;
        }
        arrayfree(&crt->crtmutx);
    }

quit:
    return;
}

/* When a thread terminates and returns to the runtime code in @CRT0
** a call is made to @@CTCLUP so that thread level cleanup can be done
**/
void
__ctclup(void)
{
    try(cleanup_thread,0);
}
