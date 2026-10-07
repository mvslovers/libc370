/* @@FLOCK.C - the FILE lock for one stdio call (#453) */
#include "src/internal/fileio.h"
#include "mvs/crt.h"
#include "mvs/lock.h"

int
__flock(FILE *fp)
{
    /* The GRT through the PPA, not __grtget(): that one looks the task's
       CRT up under an ENQ of its own and would cost what it saves.  The
       PPA's GRT is the one __grtget() finds, __grtset() sets both. */
    CLIBPPA     *ppa    = __PPAGET();
    CLIBGRT     *grt    = ppa ? ppa->ppagrt : 0;

    /* grtcthrd stays NULL until cthread_create() records its first
       thread, and the creating task records it before the ATTACH.  Until
       then only this task runs C code on this environment's streams, and
       the ENQ/DEQ pair would cost about 120 microseconds a call for
       nothing.  Not the number of CRTs: cthread_create() returns before
       the subtask has registered its own. */
    if (grt && !grt->grtcthrd) return 0;

    return lock(fp, 0) == 0;    /* rc=8 = caller already holds it (#145) */
}
