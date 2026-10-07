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
 *   LOCKED  after cthread_create() the cost is back over 30 us: the lock
 *           is taken again
 *   SHARED  a thread and main() each fputs() 300 lines to one stream;
 *           every record read back is one writer's line, never a mix
 *
 * Built twice from this source: TSTFLK against this tree's libc.a, TSTFLKR
 * against the installed one, the red control (FAST fails there).
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstflock.c -o TSTFLK -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstflock.c -o TSTFLKR -flinker-output=iebcopy
 *          ld370 --pack TSTFLK=TSTFLK.iebcopy TSTFLKR=TSTFLKR.iebcopy \
 *                -o tstflock -xmit --dsn IBMUSER.LIBC370.FLKSCR
 * Install: jcl/recvflk.jcl.   Run: jcl/tstflock.jcl.
 *
 * mvsdev JOB01598, 2026-10-07 (RECEIVE JOB01597): GREEN CC 0000, 11/11,
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

#define N       20000
#define LINES   300

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

int main(void)
{
    static const unsigned things[4] = { 0x00000000, 0x0000ABCD,
                                        0x00F0E1D2, 0x7FFFFFFF };
    char        rname[LOCKRNAMESZ];
    char        msg[80], rec[100];
    double      get, put;
    CTHDTASK    *task;
    int         i, rc, mixed = 0, nt = 0, nm = 0;

    printf("=== tstflock: the FILE lock (#453) ===\n\n");
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

    cost(&get, &put);
    printf("  after a thread: fgetc %.2f us, fputc %.2f us\n", get, put);
    CHECK(get > 30, "LOCKED: fgetc() takes the lock again");
    CHECK(put > 30, "LOCKED: fputc() takes the lock again");

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
