/*
 * tstsubp.c - libc370 #89 T0 gate probe (MVS target, batch).
 *
 * ISSUE #89 wants malloc's subpool to become a runtime value so httpd#154
 * can reclaim an abended CGI's storage with one FREEMAIN SP=n.  The whole
 * design rests on an unverified claim about MVS 3.8 storage ownership:
 * that subpool n (1-127) obtained by TCB A is DIFFERENT storage from
 * subpool n obtained by TCB B - per-task SPQEs, freed at task end, not
 * one shared pool.  If that is wrong, two concurrent httpd workers using
 * the same subpool number would destroy each other's live storage, which
 * is worse than the leak being fixed.
 *
 * This probe MEASURES that, before any #89 code is written:
 *
 *   (1) own-task round trips in SP5 on two concurrent subtasks
 *   (2) a subtask attempts FREEMAIN on the main task's SP5 block
 *   (3) a subtask releases its whole SP5 (FREEMAIN SP=) - the actual
 *       httpd#154 reclaim - while another subtask and the main task
 *       hold SP5 blocks of their own
 *   (4) a subtask ends without freeing its SP5 block - is the storage
 *       auto-released at task termination?
 *   (5) SP0 controls for (2) and (4): ATTACH defaults to SZERO=YES,
 *       so subpool 0 is expected SHARED and is the contrast case.
 *       There is deliberately NO release round for SP0 - FREEMAIN SP=0
 *       would release the region's shared pool out from under the C
 *       runtime (stacks, stdio, this probe).
 *
 * The oracle is the FREEMAIN return code, not reading the storage:
 * released storage often stays readable, an eyecatcher check alone can
 * lie.  All GETMAIN/FREEMAIN here are the conditional (RC) forms issued
 * raw - no getmain()/freemain() prefix semantics in the way - and every
 * free that may legitimately fail runs under try().
 *
 * RC 0 requires only the mechanical invariants (allocations work, own
 * round trips are clean, the release round actually released).  The
 * cross-task outcomes are measurements; the verdict WTO line
 *
 *     TSTSUBP verdict: SP5 PER-TASK   (or SHARED, or MIXED ...)
 *
 * is the fact #89 is built on and gets recorded on the issue either way.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstsubp.c -flinker-output=iebcopy -o TSTSUBP
 *     ld370 --pack TSTSUBP.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstsubp.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all mechanical invariants met, 8 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <clibwto.h>
#include <clibthrd.h>
#include <clibecb.h>
#include <clibtry.h>

#define LV      256             /* raw block size, no rounding surprises */
#define TESTSP  5               /* the measured subpool */
#define EYE     "TSTSUBP5"

/* ---- raw conditional storage services ------------------------------- */

static int rawget(unsigned lv, unsigned sp, void **out)
{
    int     rc = 0;
    void    *r1 = (void *)0;

    __asm__("GETMAIN RC,LV=(%2),SP=(%3)\n\t"
            "LR\t%0,15\n\t"
            "LR\t%1,1"
            : "=r"(rc), "=r"(r1)
            : "r"(lv), "r"(sp)
            : "0", "1", "14", "15");
    *out = rc ? (void *)0 : r1;
    return rc;
}

static int rawfree(void *a, unsigned lv, unsigned sp)
{
    int     rc = 0;

    __asm__("FREEMAIN RC,A=(%1),LV=(%2),SP=(%3)\n\t"
            "LR\t%0,15"
            : "=r"(rc)
            : "r"(a), "r"(lv), "r"(sp)
            : "0", "1", "14", "15");
    return rc;
}

static int rawrel(unsigned sp)
{
    int     rc = 0;

    __asm__("FREEMAIN RC,SP=(%1)\n\t"
            "LR\t%0,15"
            : "=r"(rc)
            : "r"(sp)
            : "0", "1", "14", "15");
    return rc;
}

/* every free that may legitimately blow up runs through this under try() */
typedef struct xfree XFREE;
struct xfree {
    void        *a;
    unsigned    sp;
    int         rc;             /* FREEMAIN rc, -1 = never reached */
};

static int dofreex(void *p)
{
    XFREE   *x = (XFREE *)p;

    x->rc = rawfree(x->a, LV, x->sp);
    return 0;
}

/* tryfree: t_rc = abend code (0 = none), x.rc = FREEMAIN rc */
static int tryfree(void *a, unsigned sp, int *f_rc)
{
    XFREE   x;
    int     t;

    x.a  = a;
    x.sp = sp;
    x.rc = -1;
    t = try(dofreex, &x);
    *f_rc = x.rc;
    return t;
}

/* ---- worker shapes --------------------------------------------------- */

typedef struct sub SUB;
struct sub {
    ECB         ready;          /* worker posts: block allocated       */
    ECB         go;             /* main posts: verify + free, then end */
    unsigned    sp;
    void        *blk;           /* worker's own block                  */
    int         g_rc;           /* worker's GETMAIN rc                 */
    int         f_rc;           /* worker's final FREEMAIN rc          */
    int         f_try;          /* abend code around that free         */
    int         eye_ok;         /* eyecatcher intact at verify time    */
    void        *target;        /* someone else's block to free        */
    int         x_rc;           /* FREEMAIN rc on the foreign block    */
    int         x_try;          /* abend code around it                */
    int         rel_rc;         /* FREEMAIN SP= release rc             */
};

/* allocate, report, park until told, verify own block, free it */
static int holder(void *arg1, void *arg2)
{
    SUB     *w = (SUB *)arg1;

    w->g_rc = rawget(LV, w->sp, &w->blk);
    if (!w->g_rc) memcpy(w->blk, EYE, 8);
    ecb_post(&w->ready, 1);
    ecb_wait(&w->go);
    w->eye_ok = w->blk && !memcmp(w->blk, EYE, 8);
    w->f_try = tryfree(w->blk, w->sp, &w->f_rc);
    return 0;
}

/* allocate + free own block, all on this TCB */
static int roundtrip(void *arg1, void *arg2)
{
    SUB     *w = (SUB *)arg1;

    w->g_rc = rawget(LV, w->sp, &w->blk);
    w->f_try = tryfree(w->blk, w->sp, &w->f_rc);
    return 0;
}

/* attempt to free a block some OTHER task allocated */
static int freer(void *arg1, void *arg2)
{
    SUB     *w = (SUB *)arg1;

    w->x_try = tryfree(w->target, w->sp, &w->x_rc);
    return 0;
}

/* allocate, release the whole subpool on this TCB, retest own block */
static int releaser(void *arg1, void *arg2)
{
    SUB     *w = (SUB *)arg1;

    w->g_rc = rawget(LV, w->sp, &w->blk);
    w->rel_rc = rawrel(w->sp);
    w->f_try = tryfree(w->blk, w->sp, &w->f_rc);   /* expect: gone */
    return 0;
}

/* allocate and end without freeing */
static int leaver(void *arg1, void *arg2)
{
    SUB     *w = (SUB *)arg1;

    w->g_rc = rawget(LV, w->sp, &w->blk);
    if (!w->g_rc) memcpy(w->blk, EYE, 8);
    return 0;
}

/* ---- harness --------------------------------------------------------- */

static int check(const char *what, int ok)
{
    printf("  %-52s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}

static void subinit(SUB *w, unsigned sp)
{
    memset(w, 0, sizeof(*w));
    w->sp = sp;
    w->g_rc = -1;
    w->f_rc = -1;
    w->f_try = -1;
    w->x_rc = -1;
    w->x_try = -1;
    w->rel_rc = -1;
}

/* join = spin on the posted bit + delete (or the step ends SA03) */
static int join(CTHDTASK **t)
{
    if (!*t) return -1;
    while (!((*t)->termecb & 0x40000000)) cthread_yield();
    cthread_delete(t);
    return 0;
}

int main(void)
{
    static SUB  h1, a1, f1, h2, r2, l1, f2, l2;
    CTHDTASK    *t;
    void        *m5 = 0, *m5b = 0, *m0 = 0;
    int         m5_rc, m5b_rc, m0_rc, l1_rc, l2_rc, t_rc;
    int         bad = 0;
    int         pertask = 0, shared = 0;

    printf("TSTSUBP - libc370 #89 T0 gate: subpool ownership across TCBs\n\n");
    wtof("TSTSUBP start, main TCB=%08X", ((unsigned *)0)[0x21C / 4]);

    /* (1) own-task round trips in SP5, two concurrent subtasks.
       H1 allocates and parks; A1 does a full round trip while H1's
       block exists; then H1 verifies its block and frees it. */
    subinit(&h1, TESTSP);
    subinit(&a1, TESTSP);
    t = cthread_create((void *)holder, &h1, (void *)0);
    if (!t) { wtof("TSTSUBP: cthread_create H1 failed"); return 8; }
    ecb_wait(&h1.ready);
    {
        CTHDTASK    *ta = cthread_create((void *)roundtrip, &a1, (void *)0);
        if (!ta) { wtof("TSTSUBP: cthread_create A1 failed"); return 8; }
        join(&ta);
    }
    ecb_post(&h1.go, 1);
    join(&t);
    bad += check("(1a) both SP5 GETMAINs worked", !h1.g_rc && !a1.g_rc);
    bad += check("(1b) A1 round trip clean (rc 0, no abend)",
                 !a1.f_rc && !a1.f_try);
    bad += check("(1c) H1 block intact after A1's free", h1.eye_ok);
    bad += check("(1d) H1 round trip clean (rc 0, no abend)",
                 !h1.f_rc && !h1.f_try);

    /* (2) cross-task free: a subtask attempts FREEMAIN on MAIN's SP5
       block.  Then main frees it itself - exactly one of the two rcs
       must be 0 unless the subtask's attempt was a silent no-op. */
    m5_rc = rawget(LV, TESTSP, &m5);
    bad += check("(2a) main SP5 GETMAIN worked", !m5_rc);
    subinit(&f1, TESTSP);
    f1.target = m5;
    t = cthread_create((void *)freer, &f1, (void *)0);
    if (!t) { wtof("TSTSUBP: cthread_create F1 failed"); return 8; }
    join(&t);
    t_rc = tryfree(m5, TESTSP, &m5_rc);
    wtof("TSTSUBP (2) cross-free SP5: worker rc=%d abend=%06X, then main rc=%d abend=%06X",
         f1.x_rc, f1.x_try, m5_rc, t_rc);
    if (f1.x_rc != 0 && !m5_rc && !t_rc)  pertask++;   /* worker failed, owner freed */
    if (f1.x_rc == 0 && (m5_rc || t_rc))  shared++;    /* worker freed it for real */

    /* (3) THE httpd#154 RECLAIM: subtask R2 releases its whole SP5
       while H2 (another subtask) and main both hold SP5 blocks. */
    subinit(&h2, TESTSP);
    subinit(&r2, TESTSP);
    t = cthread_create((void *)holder, &h2, (void *)0);
    if (!t) { wtof("TSTSUBP: cthread_create H2 failed"); return 8; }
    ecb_wait(&h2.ready);
    m5b_rc = rawget(LV, TESTSP, &m5b);
    bad += check("(3a) main + H2 SP5 GETMAINs worked", !m5b_rc && !h2.g_rc);
    {
        CTHDTASK    *tr = cthread_create((void *)releaser, &r2, (void *)0);
        if (!tr) { wtof("TSTSUBP: cthread_create R2 failed"); return 8; }
        join(&tr);
    }
    ecb_post(&h2.go, 1);
    join(&t);
    bad += check("(3b) R2's FREEMAIN SP=5 release rc 0", !r2.rel_rc);
    bad += check("(3c) release efficacy: R2's own block gone",
                 r2.f_rc != 0 || r2.f_try != 0);
    t_rc = tryfree(m5b, TESTSP, &m5b_rc);
    wtof("TSTSUBP (3) after R2 release: H2 rc=%d abend=%06X, main rc=%d abend=%06X",
         h2.f_rc, h2.f_try, m5b_rc, t_rc);
    if (!h2.f_rc && !h2.f_try && !m5b_rc && !t_rc)  pertask++;
    if ((h2.f_rc || h2.f_try) && (m5b_rc || t_rc))  shared++;

    /* (4) task termination: L1 ends owing an SP5 block.  If subpools
       are per-task the storage is auto-released and main's free fails. */
    subinit(&l1, TESTSP);
    t = cthread_create((void *)leaver, &l1, (void *)0);
    if (!t) { wtof("TSTSUBP: cthread_create L1 failed"); return 8; }
    join(&t);
    bad += check("(4a) L1 SP5 GETMAIN worked", !l1.g_rc);
    t_rc = tryfree(l1.blk, TESTSP, &l1_rc);
    wtof("TSTSUBP (4) SP5 after task end: main free rc=%d abend=%06X",
         l1_rc, t_rc);
    if (l1_rc != 0 || t_rc != 0)  pertask++;    /* auto-released at task end */
    else                          shared++;     /* survived the owning task */

    /* (5) SP0 controls - ATTACH default SZERO=YES shares subpool 0,
       so these two legs are expected to come out the other way. */
    m0_rc = rawget(LV, 0, &m0);
    bad += check("(5a) main SP0 GETMAIN worked", !m0_rc);
    subinit(&f2, 0);
    f2.target = m0;
    t = cthread_create((void *)freer, &f2, (void *)0);
    if (!t) { wtof("TSTSUBP: cthread_create F2 failed"); return 8; }
    join(&t);
    t_rc = tryfree(m0, 0, &m0_rc);
    wtof("TSTSUBP (5) cross-free SP0: worker rc=%d abend=%06X, then main rc=%d abend=%06X",
         f2.x_rc, f2.x_try, m0_rc, t_rc);

    subinit(&l2, 0);
    t = cthread_create((void *)leaver, &l2, (void *)0);
    if (!t) { wtof("TSTSUBP: cthread_create L2 failed"); return 8; }
    join(&t);
    bad += check("(5b) L2 SP0 GETMAIN worked", !l2.g_rc);
    t_rc = tryfree(l2.blk, 0, &l2_rc);
    wtof("TSTSUBP (5) SP0 after task end: main free rc=%d abend=%06X",
         l2_rc, t_rc);

    /* ---- verdict ---- */
    printf("\n  per-task evidence %d/3, shared evidence %d/3\n",
           pertask, shared);
    if (pertask == 3 && shared == 0) {
        wtof("TSTSUBP verdict: SP5 PER-TASK - one constant subpool number is enough");
        printf("\n  verdict: SP5 PER-TASK\n");
    }
    else if (shared == 3 && pertask == 0) {
        wtof("TSTSUBP verdict: SP5 SHARED - httpd needs per-worker subpool numbers");
        printf("\n  verdict: SP5 SHARED\n");
    }
    else {
        wtof("TSTSUBP verdict: MIXED (per-task %d, shared %d) - read the round WTOs",
             pertask, shared);
        printf("\n  verdict: MIXED - read the round lines above\n");
    }

    printf("\nTSTSUBP %s\n", bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
