#ifndef CLIBTHRD_H
#define CLIBTHRD_H
#include <libc370/time64.h>
#include <time.h>

/*  CLIBTHRD - A thread implementation for the CLIB environment.

    cthread_create          Create a thread instance for func(arg1,arg2)
                            with default stack size (64K).
                            Returns a CTHDTASK handle or NULL.

    cthread_create_ex       Create a thread instance for func(arg1,arg2)
                            with specified stack size.
                            Returns a CTHDTASK handle or NULL.

    cthread_delete          Destroy CTHDTASK created by cthread_create or
                            cthread_create_ex functions.
                            Returns nothing (void).

    cthread_detach          Detaches the subtask (TCB) for this thread.
                            The CTHDTASK handle remains allocated.
                            Returns the subtask return code.

    cthread_find            Returns the CTHDTASK handle for a given TCB
                            address or NULL if TCB is not a thread.

    cthread_self            Returns the CTHDTASK handle for the current thread
                            or NULL if this TCB is not a thread.

    cthread_get_tcb         Returns the TCB for the CTHDTASK handle or TCB
                            for the current task if CTHDTASK is NULL.

    cthread_post            Performs MVS POST of Event Control Block (ECB).
                            Returns 0, may ABEND if POST fails.

    cthread_wait            Waits for ECB to be posted.
                            Returns ECB value as a positive integer value.

    cthread_timed_wait      Waits for ECB to be posted or timer to expire.
                            The 'bintvl' value indicates the number of .01
                            seconds to wait for the ECB to be posted.
                            Returns ECB value as a positive integer value.

    cthread_push            Push a function pointer and argument to the
                            current thread/task.
                            Returns 0 if successful.

    cthread_pop             Pop a function pointer and argument from the
                            current thread/task. The function is then:
                            CTHDPOP_NOEXEC      0=do not execute
                            CTHDPOP_EXEC        1=execute
                            CTHDPOP_TRY         2=execute with try()
                            CTHDPOP_ESTAE       3=execute with ESTAE
                            Returns function return code.

    cthread_exit            Terminates the current thread and saves the
                            return code value in the CTHDTASK for this
                            thread.
                            The thread cleanup routine will pop any
                            pushed functions (cthread_push) and execute them
                            inside a try() wrapper.

*/
#include "mvs/lock.h"
#include "mvs/recovery.h"
#include "mvs/crt.h"
#include "libc370/array.h"
#include "mvs/ecb.h"

typedef struct cthdtask     CTHDTASK;   /* a subtask instance               */
typedef enum   cthdpop      CTHDPOP;    /* cthread_pop() pop type           */

enum cthdpop {
    CTHDPOP_NOEXEC=0,                   /* pop func, do not execute         */
    CTHDPOP_EXEC,                       /* pop func, execute                */
    CTHDPOP_TRY,                        /* pop func, execute with try()     */
    CTHDPOP_ESTAE                       /* pop func, execute with ESTAE     */
};

/* Each subtask (thread) will have a CTHDTASK */
struct cthdtask {
    char        eye[8];                 /* 00 eye catcher for dumps         */
#define CTHDTASK_EYE    "CTHDTASK"      /* ...                              */
    unsigned    tcb;                    /* 08 subtask TCB address           */
    unsigned    owntcb;                 /* 0C owner TCB address (parent)    */
    ECB         termecb;                /* 10 posted by MVS when task ends  */
    int         rc;                     /* 14 return code from function     */
    unsigned    stacksize;              /* 18 stack size in bytes           */
    void        *func;                  /* 1C subtask function address      */
    void        *arg1;                  /* 20 arg1 for subtask function     */
    void        *arg2;                  /* 24 arg2 for subtask function     */
    unsigned    stack[1];               /* 28 stack for subtask             */
};                                      /* 2C (44 bytes)                    */

CTHDTASK *cthread_create(void *func, void *arg1, void *arg2)                        asm("@@CTCRTE");

CTHDTASK *cthread_create_ex(void *func, void *arg1, void *arg2, unsigned stacksize) asm("@@CTCRTX");

void cthread_delete(CTHDTASK **task)                                                asm("@@CTDEL");

/* cthread_detach() refuses a subtask that has not ended and returns this.
** The DETACH SVC answers 0/4/8 in task->rc and try() answers an abend code,
** so a negative value cannot be confused with either. */
#define CTHREAD_DETACH_LIVE (-1)        /* subtask still running, not detached */

int cthread_detach(CTHDTASK *task)                                                  asm("@@CTDET");

CTHDTASK *cthread_find(unsigned tcb)                                                asm("@@CTFIND");

CTHDTASK *cthread_self(void)                                                        asm("@@CTSELF");

unsigned cthread_get_tcb(CTHDTASK *task)                                            asm("@@CTGTCB");

int cthread_post(ECB *ecb, unsigned code)                                           asm("@@CTPOST");

int cthread_wait(ECB *ecb)                                                          asm("@@CTWAIT");

int cthread_timed_wait(ECB *ecb, unsigned bintvl, unsigned postcode)                asm("@@CTTWAT");

int cthread_push(int (*func)(void*), void *arg)                                     asm("@@CTPUSH");

int cthread_pop(CTHDPOP type)                                                       asm("@@CTPOP");

int cthread_exit(int rc)                                                            asm("@@CTEXIT");

int cthread_lock(int shared, const char *rname)                                     asm("@@CTLOCK");

int cthread_unlock(const char *rname)                                               asm("@@CTUNLK");

int cthread_yield(void)                                                             asm("@@CTYIEL");

/* ---- from 1.x clibos.h -------------------------------------------------- */
/* clib_identify_cthread() - find CDE for CTHREAD program and make APF authorized AC(1) */
int clib_identify_cthread(void)                             asm("@@IDECTH");

/* ---- from 1.x clibthdi.h ------------------------------------------------ */
typedef struct cthdmgr      CTHDMGR;    /* thread manager instance          */
typedef struct cthdque      CTHDQUE;    /* thread manager queue             */
typedef struct cthdwork     CTHDWORK;   /* worker thread instance           */

struct cthdmgr {
    char        eye[8];                 /* 00 eye catcher for dumps         */
#define CTHDMGR_EYE     "CTHDMGR"       /* ...                              */
    CTHDTASK    *task;                  /* 08 thread manager task           */
    unsigned    wait;                   /* 0C wait for work (ECB)           */
#define CTHDMGR_POST_DATA       0       /* ... data was queued              */
#define CTHDMGR_POST_WAIT       1       /* ... worker waiting for data      */
#define CTHDMGR_POST_QUIESCE    2       /* ... quiesce thread manager       */
#define CTHDMGR_POST_SHUTDOWN   3       /* ... terminate thread manager     */
#define CTHDMGR_POST_TIMER      4       /* ... timer                        */

    void        *func;                  /* 10 thread function               */
    void        *udata;                 /* 14 user data for threads         */
    unsigned    stacksize;              /* 18 thread stack size             */
    CTHDWORK    **worker;               /* 1C work threads                  */

    CTHDQUE     **queue;                /* 20 work queue                    */
    volatile int state;                 /* 24 thread manager state          */
#define CTHDMGR_STATE_INIT      0       /* ... initialized                  */
#define CTHDMGR_STATE_RUNNING   1       /* ... thread manager is active     */
#define CTHDMGR_STATE_QUIESCE   2       /* ... thread manager is quiesced   */
#define CTHDMGR_STATE_STOPPED   3       /* ... thread manager is stopped    */
#define CTHDMGR_STATE_WAITING   4       /* ... thread manager is waiting    */
    unsigned    mintask;                /* 28 min task                      */
    unsigned    maxtask;                /* 2C max task                      */

    __64        dispatched;             /* 30 dispatched counter            */
    unsigned    start;                  /* 38 round robbin start index      */
    char        rname[24];              /* 3C resource name                 */
};                                      /* 54 (84 bytes)                    */

struct cthdwork {
    char        eye[8];                 /* 00 eye catcher for dumps         */
#define CTHDWORK_EYE    "CTHDWORK"      /* ...                              */
    unsigned    wait;                   /* 08 thread wait ecb               */
#define CTHDWORK_POST_REQUEST   0       /* ... process request              */
#define CTHDWORK_POST_TIMER     1       /* ... timer pop                    */
#define CTHDWORK_POST_SHUTDOWN  2       /* ... shutdown thread              */
    CTHDMGR     *mgr;                   /* 0C thread manager instance       */

    CTHDTASK    *task;                  /* 10 thread instance               */
    CTHDQUE     *queue;                 /* 14 queued work                   */
    volatile int state;                 /* 18 thread worker state           */
#define CTHDWORK_STATE_INIT     0       /* ... initialized                  */
#define CTHDWORK_STATE_RUNNING  1       /* ... worker thread is active      */
#define CTHDWORK_STATE_WAITING  2       /* ... worker thread is waiting     */
#define CTHDWORK_STATE_DISPATCH 3       /* ... worker thread is dispatched  */
#define CTHDWORK_STATE_SHUTDOWN 4       /* ... worker thread is stopping    */
#define CTHDWORK_STATE_STOPPED  5       /* ... worker thread is stopped     */
#define CTHDWORK_STATE_STUCK    6       /* ... did not stop, retained (#11) */
    unsigned    opt;                    /* 1C worker optionsnt              */
#define CTHDWORK_OPT_TIMER	0x00000001	/* on=post timer desired (1 sec)	*/
#define CTHDWORK_OPT_NOWORK	0x00000002  /* on=don't give queued work		*/

    time64_t    start_time;             /* 20 time worker created           */
    time64_t    wait_time;              /* 28 time worker waited for work   */
    time64_t    disp_time;              /* 30 time worker was dispatched    */
    __64		dispatched;				/* 38 dispatched count				*/
};                                      /* 40 (64 bytes)                    */

struct cthdque {
    char        eye[8];                 /* 00 eye catcher for dumps         */
#define CTHDQUE_EYE     "CTHDQUE"       /* ...                              */
    CTHDMGR     *mgr;                   /* 0C thread manager instance       */
    void        *data;                  /* 10 queue data item               */
};                                      /* 14 (20 bytes)                    */


CTHDMGR *cthread_manager_init(unsigned count, void *func, void *udata, unsigned stacksize)  asm("@@CMINIT");
int cthread_manager_term(CTHDMGR **cthdmgr)                                                 asm("@@CMTERM");
int cthread_worker_add(CTHDMGR *mgr)                                                        asm("@@CMWADD");
int cthread_worker_del(CTHDWORK **work)                                                     asm("@@CMWDEL");
int cthread_queue_del(CTHDQUE **queue)                                                      asm("@@CMQDEL");
/* cthread_worker_shutdown() - stop a worker and report whether it really stopped.
** Returns 0 when the worker's subtask has ended (or it never had one), so the
** caller may delete it.  Returns -1 when the subtask is STILL RUNNING: the
** worker is left in CTHDWORK_STATE_STUCK, and neither it, its task nor its
** stack may be freed -- the stack lives inside the CTHDTASK allocation, so
** freeing it pulls the ground out from under a live TCB (#11). */
int cthread_worker_shutdown(CTHDWORK *work)                                                 asm("@@CMWSHU");
int cthread_queue_add(CTHDMGR *mgr, void *data)                                             asm("@@CMQADD");
int cthread_worker_wait(CTHDWORK *work, char **data)                                        asm("@@CMWWAT");

#endif
