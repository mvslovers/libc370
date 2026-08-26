/*
 * tstslk.c - libc370 #147 item 4: sysunlock() must actually release
 * what syslock() took.
 *
 * ISSUE #147: @@enqdeq.c's DEQ branch overwrote pl.opt with
 * ENQ_OPT_HAVE, throwing the scope bits away - every DEQ went to
 * SVC 48 as SCOPE=STEP.  Scope is part of the resource identity, so
 * sysunlock()'s DEQ (SCOPE=SYSTEM at the ENQ) addressed a different
 * resource, answered rc=8, and the system-scope lock stayed held until
 * task termination.  Silent, because rc=8 is also the harmless
 * "you didn't have it" answer - the same silent-rc-8 shape as #145.
 * test/host/tstenqdq.c pins the parameter list on the host; this probe
 * measures the consequence on the real system:
 *
 *   (1) syslock() acquires (rc=0) and systestlock() answers 8 (held);
 *   (2) sysunlock() releases - RED: rc=8, the DEQ misses;
 *   (3) a SECOND syslock() acquires again - the sharpest signal:
 *       RED: rc=8 (still held from (1)), GREEN: rc=0 (fresh acquire,
 *       so (2) really released);
 *   (4) final sysunlock() + systestlock()=0 leave the system clean.
 *
 * Every verdict goes out via wtof() as well as printf() (the #145
 * probe lesson: WTOs land in the job log immediately); the unlocks
 * run under try() so an unexpected S530/S730 is reported, not fatal.
 * SCOPE=SYSTEM cleanup is by MVS at task end, so even a RED run leaves
 * nothing behind.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstslk.c -flinker-output=iebcopy -o TSTSLK
 *     ld370 --pack TSTSLK.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstslk.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU
 * automatically.
 *
 * RC: 0 = all checks passed, 8 = at least one did not.
 */
#include <stdio.h>
#include <cliblock.h>
#include <clibwto.h>
#include <clibtry.h>

static int  bad = 0;

static int check(const char *what, int ok)
{
    printf("  %-52s %s\n", what, ok ? "ok" : "*** FAIL");
    if (!ok) bad = 1;
    return ok;
}

/* sysunlock under try(): report an abend instead of dying of it */
typedef struct xunlk XUNLK;
struct xunlk {
    void    *thing;
    int     rc;                 /* sysunlock rc, -1 = never reached     */
};

static int dounlk(void *p)
{
    XUNLK   *x = (XUNLK *)p;

    x->rc = sysunlock(x->thing, 0);
    return 0;
}

static int tryunlk(void *thing, int *u_rc)
{
    XUNLK   x;
    int     t;

    x.thing = thing;
    x.rc = -1;
    t = try(dounlk, &x);
    *u_rc = x.rc;
    return t;
}

int main(void)
{
    static int  thing = 0;
    int         r1, r2, r3, r4, r5, t3, t5;

    printf("TSTSLK - libc370 #147 item 4: DEQ scope bits, measured\n\n");
    wtof("TSTSLK start, main TCB=%08X", ((unsigned *)0)[0x21C / 4]);

    r1 = syslock(&thing, 0);
    check("(1) syslock() acquires (rc=0)", r1 == 0);

    r2 = systestlock(&thing, 0);
    check("(1) systestlock() while held answers 8", r2 == 8);

    t3 = tryunlk(&thing, &r3);
    printf("      [sysunlock: try=%d rc=%d]\n", t3, r3);
    check("(2) sysunlock() releases (rc=0, no abend)", t3 == 0 && r3 == 0);

    r4 = syslock(&thing, 0);
    printf("      [re-syslock: rc=%d]\n", r4);
    check("(3) second syslock() acquires fresh (rc=0)", r4 == 0);

    t5 = tryunlk(&thing, &r5);
    check("(4) final sysunlock() releases (rc=0, no abend)",
          t5 == 0 && r5 == 0);

    r5 = systestlock(&thing, 0);
    check("(4) systestlock() after release answers 0", r5 == 0);

    wtof("TSTSLK r: lock=%d test=%d unlk=%d/%d relock=%d final=%d/%d",
         r1, r2, t3, r3, r4, t5, r5);

    printf("\nTSTSLK %s\n", bad ? "FAILED" : "PASSED");
    wtof("TSTSLK %s", bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
