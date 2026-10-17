/*
 * tstf14.c - libc370 size analysis F1/F4: names and messages built
 *            without printf must be the same as before.
 *
 * Compiled once per case with -DCASE=n, against this tree (F14xxx) and
 * the installed library (F1Rxxx); the JCL runs both, and the verdicts and
 * the console lines must agree.
 *
 *   1 APP  fopen("DD:PDS(MEM)", "a") on an existing member is refused with
 *          EOPNOTSUPP (fopen's probe name, F1); on a new member it opens
 *   2 MTX  main holds a mutex; a thread's mtxtry() does not get it (rc 4),
 *          after mtxunlk() a second thread's does (rc 0) - lock and try
 *          build the same ENQ name (F4)
 *   3 DUM  cthread_create(NULL, ...): the report of the dummy thread on the
 *          console (compare the lines)
 *   4 ABR  abendrpt() and an S0C1: the abend report on the console
 *          (compare the lines); the step ends S0C1 by design
 *
 * RC 0 = the case behaved as above, 8 = it did not (case 4 abends).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/mutex.h>
#include <mvs/thread.h>
#include <mvs/recovery.h>

#ifndef CASE
#define CASE 1
#endif

static int bad __attribute__((unused));

static void want(int cond, const char *what) __attribute__((unused));
static void want(int cond, const char *what)
{
    wtof("TSTF14 %d %s: %s", CASE, cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

#if CASE == 2
static int trycode;

static int tryit(void *arg1, void *arg2)
{
    CLIBMUTX *m = arg1;

    (void)arg2;
    trycode = mtxtry(m);
    if (trycode == 0) mtxunlk(m);
    return 0;
}

static int runtry(CLIBMUTX *m)
{
    CTHDTASK    *t;

    trycode = -1;       /* before the create: the thread may run first */
    t = cthread_create(tryit, m, 0);
    if (!t) return -2;
    cthread_wait(&t->termecb);
    cthread_delete(&t);
    return trycode;
}
#endif

#if CASE == 4
static void boom(void)
{
    __asm__("DC\tH'0'");
}
#endif

int main(void)
{
#if CASE == 1
    FILE    *fp = fopen("DD:PDS(MEM)", "w");

    want(fp != NULL, "member MEM written");
    if (fp) { fputs("L1\n", fp); fclose(fp); }
    errno = 0;
    fp = fopen("DD:PDS(MEM)", "a");
    wtof("TSTF14 1 append to MEM: %s errno=%d", fp ? "opened" : "NULL", errno);
    want(fp == NULL && errno == EOPNOTSUPP, "\"a\" on an existing member refused");
    if (fp) fclose(fp);
    fp = fopen("DD:PDS(NEWM)", "a");
    want(fp != NULL, "\"a\" on a new member opens");
    if (fp) { fputs("N1\n", fp); fclose(fp); }
#elif CASE == 2
    CLIBMUTX    *m = mtxnew();
    int         rc;

    want(m != NULL, "mtxnew");
    mtxlock(m);
    want(mtxheld(m), "main holds the mutex");
    rc = runtry(m);
    wtof("TSTF14 2 thread mtxtry while held rc=%d", rc);
    want(rc == 4, "a thread cannot take it (rc 4)");
    mtxunlk(m);
    rc = runtry(m);
    wtof("TSTF14 2 thread mtxtry after unlock rc=%d", rc);
    want(rc == 0, "after mtxunlk a thread takes it (rc 0)");
    mtxfree(m);
#elif CASE == 3
    CTHDTASK    *t = cthread_create(NULL, (void *)0x11111111, (void *)0x22222222);

    want(t != NULL, "cthread_create(NULL) made a thread");
    if (t) {
        cthread_wait(&t->termecb);
        cthread_delete(&t);
    }
#elif CASE == 4
    abendrpt(ESTAE_CREATE, DUMP_SUPPRESS);
    boom();
#endif
    wtof("TSTF14 %d VERDICT failed=%d", CASE, bad);
    return bad ? 8 : 0;
}
