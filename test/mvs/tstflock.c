/*
 * tstflock.c - libc370 #453 on MVS: the FILE lock costs nothing until a
 * thread exists, and the lock name is built without sprintf().
 *
 * fgetc()/fputc() took an ENQ and a DEQ per byte, 126 microseconds a call
 * on mvsdev against 0.9 for the work itself.  Now stdio skips the lock
 * while no thread was created through cthread_create(), and lock() builds
 * its resource name by hand.  The checks:
 *
 *   NAME    lock(p) and lock_res(<sprintf'd LOCK.%08X of p>) name the same
 *           resource: the second returns 8 (already held), for 4 addresses
 *   FAST    20000 fgetc() and 20000 fputc() cost under 20 us each while
 *           no thread exists                             (was about 126)
 *   LOCKED  while a thread exists the cost is over 30 us: the lock is
 *           taken
 *   AFTER   once the thread has ended and is detached, under 20 us again
 *           (#470: the task tree decides, not the GRT's thread table)
 *   BLOCK   main() holds a stream's lock for 2 s; a thread's fputs() to it
 *           has to wait, and so does that of TSTFLKM, a module with a
 *           startup of its own LINKed on a thread (#470; its GRT has no
 *           thread table, so 2.5.0 and 2.6.0 skipped the lock there)
 *   SHARED  a thread and main() each fputs() 300 lines to one stream;
 *           every record read back is one writer's line, never a mix
 *
 * Built twice from this source: TSTFLK against this tree's libc.a, TSTFLKR
 * against the installed one, the red control (AFTER and the module's
 * BLOCK fail there).  TSTFLKM is test/mvs/tstflkm.c.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstflock.c -o TSTFLK -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -DMODNAME='"TSTFLKMR"' \
 *                test/mvs/tstflock.c -o TSTFLKR -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstflkm.c -o TSTFLKM -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstflkm.c -o TSTFLKMR -flinker-output=iebcopy
 *          ld370 --pack TSTFLK=TSTFLK.iebcopy TSTFLKR=TSTFLKR.iebcopy \
 *                TSTFLKM=TSTFLKM.iebcopy TSTFLKMR=TSTFLKMR.iebcopy \
 *                -o tstflock -xmit --dsn IBMUSER.LIBC370.FLKSCR
 * Install: jcl/recvflk.jcl.   Run: jcl/tstflock.jcl.
 *
 * mvsdev JOB01639, 2026-10-07 (RECEIVE JOB01638), #470: GREEN 17/17, also
 * CALLed under a batch TMP 17/17 (fgetc/fputc about 3 us without a thread,
 * 81-89 us with one, about 2-4 us after it ended); RED (installed 2.6.0)
 * 14/17: AFTER stays at 80-90 us and the module's BLOCK returns at once
 * (rc 1).  JOB01637 the same without the TSO step.
 *
 * Before #470: mvsdev JOB01598, 2026-10-07 (RECEIVE JOB01597): GREEN CC 0000, 11/11,
 * fgetc/fputc 2.46/2.47 us without a thread, 87/78 us after one; RED
 * (installed 2.4.1) CC 0001, 9/11, 128/130 us without a thread.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <mvs/lock.h>
#include <mvs/thread.h>
#include <mvs/crt.h>
#include <mvs/link.h>

#define N       20000
#define LINES   300

/* the module module_writer() LINKs: TSTFLKM, or TSTFLKMR built against
   the installed library for the red control */
#ifndef MODNAME
#define MODNAME "TSTFLKM"
#endif

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static unsigned long long now(void)
{
    unsigned long long t;
    __asm__ __volatile__("STCK %0" : "=m"(t) : : "cc", "memory");
    return t;
}

/* microseconds per call of fgetc() over DD:TXT and of fputc() into it */
static void cost(double *get, double *put)
{
    FILE                *fp;
    unsigned long long  a, b;
    long                i, n = 0;

    fp = fopen("DD:TXT", "w");
    a = now();
    for (i = 0; i < N; i++) fputc(i % 80 == 79 ? '\n' : 'x', fp);
    b = now();
    fclose(fp);
    *put = (double)((b - a) >> 12) / N;

    fp = fopen("DD:TXT", "r");
    a = now();
    while (fgetc(fp) != EOF) n++;
    b = now();
    fclose(fp);
    *get = n ? (double)((b - a) >> 12) / n : 0;
}

static FILE *shared;

static void lines(char c)
{
    char    line[64];
    int     i;

    memset(line, c, 60);
    line[60] = '\n';
    line[61] = '\0';
    for (i = 0; i < LINES; i++) fputs(line, shared);
}

static int writer(void *a, void *b)
{
    (void)a;
    (void)b;
    lines('T');
    return 0;
}

static ECB  go;
static FILE *blk;

/* a thread that only waits, so that one exists while main() measures */
static int waiter(void *a, void *b)
{
    (void)a;
    (void)b;
    cthread_wait(&go);
    return 0;
}

/* one line to blk while main() holds blk's lock: the call has to wait for
   main()'s unlock().  0 = it waited a second or more, 1 = it did not. */
static int thread_writer(void *a, void *b)
{
    unsigned long long t0 = now();

    (void)a;
    (void)b;
    fputs("thread line under main's lock\n", blk);
    return ((now() - t0) >> 12) >= 1000000ULL ? 0 : 1;
}

/* the same from TSTFLKM, a module with a startup of its own, LINKed on a
   thread: its GRT has no thread table (#470).  Its rc is the verdict. */
static int module_writer(void *a, void *b)
{
    struct { unsigned short len; char text[10]; } parm;
    unsigned    plist[1];
    int         rc = -1;

    (void)a;
    (void)b;
    sprintf(parm.text, "%08X", (unsigned)blk);
    parm.len = 8;
    plist[0] = (unsigned)&parm | 0x80000000;
    __link(MODNAME, NULL, plist, &rc);
    return rc;
}

/* run fn on a thread while main() holds blk's lock for two seconds */
static int under_lock(int (*fn)(void *, void *))
{
    CTHDTASK            *t;
    unsigned long long  t0;
    int                 rc;

    lock(blk, 0);
    t = cthread_create((void *)fn, NULL, NULL);
    if (!t) {
        unlock(blk, 0);
        return -1;
    }
    t0 = now();
    while (((now() - t0) >> 12) < 2000000ULL) ;
    unlock(blk, 0);
    cthread_wait(&t->termecb);
    rc = t->rc;
    cthread_delete(&t);
    return rc;
}

int main(void)
{
    static const unsigned things[4] = { 0x00000000, 0x0000ABCD,
                                        0x00F0E1D2, 0x7FFFFFFF };
    char        rname[LOCKRNAMESZ];
    char        msg[80], rec[100];
    double      get, put;
    CTHDTASK    *task;
    int         i, rc, mixed = 0, nt = 0, nm = 0;

    printf("=== tstflock: the FILE lock (#453, #470) ===\n\n");
    {
        CLIBGRT *grt = __grtget();
        printf("  GRT %08X, thread table %08X\n", (unsigned)grt,
               grt ? (unsigned)grt->grtcthrd : 0);
    }

    for (i = 0; i < 4; i++) {
        void *p = (void *)things[i];
        sprintf(rname, LOCKRNAME, things[i]);
        lock(p, 0);
        rc = lock_res(rname, 0);
        unlock(p, 0);
        sprintf(msg, "NAME: lock(%08X) is %s", things[i], rname);
        CHECK(rc == 8, msg);
    }

    cost(&get, &put);
    printf("  no thread: fgetc %.2f us, fputc %.2f us\n", get, put);
    CHECK(get > 0 && get < 20, "FAST: fgetc() under 20 us without a thread");
    CHECK(put > 0 && put < 20, "FAST: fputc() under 20 us without a thread");

    go = 0;
    task = cthread_create((void *)waiter, NULL, NULL);
    CHECK(task != NULL, "cthread_create() of a waiting thread");
    if (!task) goto done;
    cost(&get, &put);
    printf("  while a thread exists: fgetc %.2f us, fputc %.2f us\n", get, put);
    CHECK(get > 30, "LOCKED: fgetc() takes the lock while a thread exists");
    CHECK(put > 30, "LOCKED: fputc() takes the lock while a thread exists");
    cthread_post(&go, 0);
    cthread_wait(&task->termecb);
    cthread_delete(&task);

    cost(&get, &put);
    printf("  after the thread ended: fgetc %.2f us, fputc %.2f us\n", get, put);
    CHECK(get > 0 && get < 20, "AFTER: no lock once the thread is gone");
    CHECK(put > 0 && put < 20, "AFTER: no lock once the thread is gone (fputc)");

    blk = fopen("DD:BLK", "w");
    CHECK(blk != NULL, "open DD:BLK");
    if (!blk) goto done;
    rc = under_lock(thread_writer);
    printf("  BLOCK thread: rc %d\n", rc);
    CHECK(rc == 0, "BLOCK: a thread's fputs() waits for main's lock");
    rc = under_lock(module_writer);
    printf("  BLOCK module: rc %d\n", rc);
    CHECK(rc == 0, "BLOCK: a LINKed module's fputs() waits for main's lock");
    fclose(blk);

    shared = fopen("DD:LOG", "w");
    CHECK(shared != NULL, "open DD:LOG");
    if (!shared) goto done;
    task = cthread_create((void *)writer, NULL, NULL);
    CHECK(task != NULL, "cthread_create()");
    if (!task) goto done;
    lines('M');
    cthread_wait(&task->termecb);
    cthread_delete(&task);
    fclose(shared);

    shared = fopen("DD:LOG", "r");
    if (shared) {
        while (fgets(rec, sizeof(rec), shared)) {
            size_t len = strcspn(rec, "\n");
            while (len > 0 && rec[len - 1] == ' ') len--;   /* FB padding */
            if (len == 60 && strspn(rec, "T") == 60) nt++;
            else if (len == 60 && strspn(rec, "M") == 60) nm++;
            else mixed++;
        }
        fclose(shared);
    }
    printf("  SHARED: %d thread lines, %d main lines, %d mixed\n", nt, nm, mixed);
    CHECK(nt == LINES && nm == LINES && mixed == 0,
          "SHARED: every line is one writer's");

done:
    printf("\n=== tstflock: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
