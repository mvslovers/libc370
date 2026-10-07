/* @@FLOCK.C - the FILE lock for one stdio call (#453, #470) */
#include "src/internal/fileio.h"
#include <string.h>
#include "mvs/crt.h"
#include "mvs/lock.h"

#define TCBFSAB(t)  ((unsigned)(t)[0x71] << 16 | (unsigned)(t)[0x72] << 8 \
                     | (unsigned)(t)[0x73])         /* first save area   */
#define TCBJSTCB(t) (*(unsigned *)((t) + 0x7C) & 0x00FFFFFF)
#define TCBOTC(t)   (*(unsigned *)((t) + 0x84) & 0x00FFFFFF)  /* mother  */
#define TCBLTC(t)   (*(unsigned *)((t) + 0x88) & 0x00FFFFFF)  /* daughter*/

/* Does task tcb run C code: is the "next" of its first save area a
   CLIBPPA, the way @@PPAGET recognizes one? */
static int
c_task(const unsigned char *tcb)
{
    unsigned    sa  = TCBFSAB(tcb);
    unsigned    ppa;

    if (!sa) return 0;
    ppa = *(unsigned *)(sa + 8);
    if (!ppa || ppa > 0x00FFFFFF) return 0;
    return memcmp((const void *)ppa, PPAEYE, 4) == 0;
}

/* Can another task of this job step run C code on a stream of ours?
   Yes when this task has a subtask, or when a task above it, up to the
   job step task, is a C task: a thread, or a module LINKed into one.
   Not the caller's GRT: a module with a startup of its own has a GRT
   without threads, and still writes to its caller's streams (#470). */
static int
shared(void)
{
    const unsigned      *psatold = (const unsigned *)0x21C; /* PSATOLD */
    const unsigned char *tcb     = (const unsigned char *)*psatold;
    const unsigned char *t;
    unsigned            js;

    if (TCBLTC(tcb)) return 1;
    js = TCBJSTCB(tcb);
    if ((unsigned)tcb == js) return 0;  /* the job step task itself */
    t = (const unsigned char *)TCBOTC(tcb);
    for (int n = 0; n < 64; n++) {
        if (!t) return 0;
        if (c_task(t)) return 1;
        if ((unsigned)t == js) return 0;
        t = (const unsigned char *)TCBOTC(t);
    }
    return 0;
}

int
__flock(FILE *fp)
{
    /* Without another task that can reach the stream, the ENQ/DEQ pair
       would cost about 120 microseconds a call for nothing (#453).  Read
       from the TCB tree: no SVC, and right after a DETACH as well. */
    if (!shared()) return 0;

    return lock(fp, 0) == 0;    /* rc=8 = caller already holds it (#145) */
}
