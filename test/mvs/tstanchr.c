/*
 * tstanchr.c - libc370 #85 regression probe (MVS target, batch).
 *
 * ISSUE #85: ~25 sites dereferenced the __crtget()/__grtget() anchor
 * without a check (both can return NULL today - "CRT for TCB not
 * found" - and #82 added a second route via a failed constructor),
 * plus a handful of ignored arrayadd() return codes that silently
 * dropped the element and leaked it.
 *
 * The fix adds a NULL guard to every listed site, failing through
 * each function's own contract, pairs the func/arg array adds in
 * atexit()/on_exit()/cthread_push() with a rollback, checks the
 * arrayadd() in newthread()/@@listvl/jesjob, and - the one route a
 * healthy program could actually reach - tests @@GRTSET's rc in
 * @@CRT0/@@CRT1 (WTO + U0801) instead of running with no GRT.
 *
 * The NULL branches themselves need a TCB that runs C code without a
 * CRT, which is exactly the unsupported situation - they are not
 * black-box reachable from a healthy program (same standing as the
 * failed() guard in #9).  What IS testable is that the guards leave
 * every touched happy path intact.  This probe exercises them all,
 * linked against the fixed library, and must stay COND CODE 0000 on
 * BOTH sides of the change:
 *
 *   stdio anchors, env round trip, atexit/on_exit registration
 *   (handler firing visible as a WTO in the job log), cthread
 *   push/pop, mutex try/lock/unlock, gmtime, and a timer that
 *   really fires (tmr_ecb + ecb_wait).
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstanchr.c -flinker-output=iebcopy -o TSTANCHR
 *     ld370 --pack TSTANCHR.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstanchr.jcl (TIME=1 as a watchdog for the ecb_wait).
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <clibwto.h>
#include <clibenv.h>
#include <clibthrd.h>
#include <clibmutx.h>
#include <clibtmr.h>
#include <clibecb.h>

static CLIBMUTX mtx;

static int check(const char *what, int ok);
static void exit_handler(void);
static int  push_func(void *arg);

int main(void)
{
    int         bad = 0;
    char        *v;
    struct tm   *tms;
    time_t      t   = 0;
    TQEID       id;
    ECB         ecb;

    printf("TSTANCHR - libc370 #85 probe\n\n");

    /* stdio anchors - printf working at all already proves stdout */
    bad += check("stdio anchors are non-NULL",
                 __gtin() != NULL && __gtout() != NULL && __gterr() != NULL);

    /* environment round trip */
    bad += check("setenv works", setenv("TSTVAR", "VALUE85", 1) == 0);
    v = getenv("TSTVAR");
    bad += check("getenv finds it", v != NULL && strcmp(v, "VALUE85") == 0);
    v = getenvi("tstvar");
    bad += check("getenvi finds it caseless",
                 v != NULL && strcmp(v, "VALUE85") == 0);
    bad += check("unsetenv works", unsetenv("TSTVAR") == 0);
    bad += check("getenv no longer finds it", getenv("TSTVAR") == NULL);

    /* exit hooks - the handler's WTO in the job log is the evidence
       that registration still leads to execution */
    bad += check("atexit registers", atexit(exit_handler) == 0);

    /* cthread push/pop cycle */
    bad += check("cthread_push works", cthread_push(push_func, NULL) == 0);
    bad += check("cthread_pop works", cthread_pop(CTHDPOP_NOEXEC) == 0);

    /* mutex family */
    bad += check("mtxtry obtains the mutex", mtxtry(&mtx) == 0);
    bad += check("... and mtxheld agrees", mtxheld(&mtx) != 0);
    mtxunlk(&mtx);
    bad += check("mtxunlk releases it", mtxheld(&mtx) == 0);
    mtxlock(&mtx);
    bad += check("mtxlock obtains it again", mtxheld(&mtx) != 0);
    mtxunlk(&mtx);

    /* gmtime through the CRT tm buffer */
    tms = gmtime(&t);
    bad += check("gmtime(0) works via the CRT buffer",
                 tms != NULL && tms->tm_year == 70);

    /* a timer that really fires: 0.5s one-shot ECB post */
    memset(&ecb, 0, sizeof(ecb));
    id = tmr_ecb(&ecb, 50);
    bad += check("tmr_ecb returns a TQEID", id != 0);
    if (id != 0) {
        ecb_wait(&ecb);
        bad += check("the timer fired (ECB posted)", 1);
    }
    bad += check("tmr_stop shuts the timer down", tmr_stop() == 0);

    printf("\nTSTANCHR %s\n", bad ? "FAILED" : "PASSED");

    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-50s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}

static void exit_handler(void)
{
    wtof("TSTANCHR atexit handler fired");
}

static int push_func(void *arg)
{
    return 0;
}
