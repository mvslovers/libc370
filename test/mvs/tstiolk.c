/*
 * tstiolk.c - libc370 #145: nested public stdio releases the FILE lock;
 * concurrent printf corrupts the stream (the ftpd#117 S001 shape).
 *
 * ISSUE #145: vvprintf() takes the FILE lock, but the %s / %eEgGfF
 * branches and every __examin() conversion call the PUBLIC fputs()/putc(),
 * which lock and then UNCONDITIONALLY unlock the same FILE.  lock()/
 * unlock() are ENQ/DEQ RET=HAVE with no nesting count, so the inner
 * unlock() releases the outer hold at the first conversion and the rest
 * of the line - including the \n-triggered __fflush()/QSAM PUT - runs
 * unserialized.  Two tasks printf'ing the same FILE merge/truncate
 * records; ftpd#117 shows the end of that road as ABEND S001-1.
 *
 * What this probe pins, in three rounds:
 *
 *   (1) the lock-primitive premise the hardening rests on: a nested
 *       lock() on a held resource answers rc=8 (cliblock.h documents it,
 *       nothing in the tree ever measured it), unlock() of a resource
 *       not held answers nonzero rather than abending, and testlock()
 *       distinguishes held(8)/free(0) from the owning task.  These are
 *       MEASUREMENTS of ENQ/DEQ RET=HAVE on the real system - if (1)
 *       fails, the ownership-aware fix is built on sand and must not
 *       ship.
 *
 *   (2) the mechanism, deterministically, one task: lock(fp) then a
 *       public fputs() - after it returns the caller must STILL own the
 *       lock (testlock=8, final unlock=0).  RED today: the inner
 *       unlock() gave the lock away (testlock=0, final unlock!=0).
 *
 *   (3) the ftpd#117 reproduction: two cthread subtasks vfprintf()
 *       "[%s] %s\n" lines to one FILE, N times each, distinct fill
 *       characters, then the dataset is read back: every record must be
 *       exactly one task's line and each task's count must be N.
 *       GREEN is deterministic (the line is written under one ENQ, the
 *       waiter sits in ENQ until the PUT is done).  RED reproduced the
 *       full ftpd#117 failure on the very first run (JOB02235,
 *       2026-08-26): IEC020I 001-1 on OUTDD + "NO SYNAD EXIT SPECIFIED"
 *       seconds after start, one writer dead, the survivor wedged in
 *       the corrupted QSAM state until the job was cancelled S222.
 *       The second run (JOB02237) showed the OTHER terminal symptom of
 *       the same corruption: no S001, but writer B wedged in a QSAM
 *       wait after 8 of 400 lines while A wrote all 400 - the collision
 *       needs seconds, not luck.
 *       The first death also threw away everything SYSPRINT had buffered -
 *       which is why every verdict below goes out via wtof() as well:
 *       WTOs land in the JES2 job log immediately and survive any
 *       S001/S222/hang that follows.  The writers run under try(), the
 *       join is bounded, and fclose() on the possibly-broken FILE runs
 *       under try() too, so the probe reports the wreckage instead of
 *       becoming part of it.
 *
 * vfprintf() is used deliberately: fprintf() formats into a private
 * buffer and fwrite()s once (one closed critical section) and does NOT
 * reproduce #145; printf()/vprintf()/vfprintf() go through vvprintf()
 * and do.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstiolk.c -flinker-output=iebcopy -o TSTIOLK
 *     ld370 --pack TSTIOLK.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstiolk.jcl (OUTDD/OUTDD2 are the scratch datasets the
 * rounds write and read back).  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all checks passed, 8 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <cliblock.h>
#include <clibthrd.h>
#include <clibecb.h>
#include <clibwto.h>
#include <clibtry.h>

#define NLINES  400             /* lines per writer task in round (3)   */
#define JOINMAX 6000            /* join bound: 6000 x 10ms yield = 60s  */
#define TAGA    "AAAAAAAA"
#define TAGB    "BBBBBBBB"
#define MSGA    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define MSGB    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"

static int  bad = 0;

static int check(const char *what, int ok)
{
    printf("  %-52s %s\n", what, ok ? "ok" : "*** FAIL");
    if (!ok) bad = 1;
    return ok;
}

/* ---- the ftpd#117 call shape: vfprintf, NOT fprintf ------------------ */

static int logline(FILE *fp, const char *fmt, ...)
{
    va_list ap;
    int     rc;

    va_start(ap, fmt);
    rc = vfprintf(fp, fmt, ap);
    va_end(ap);
    return rc;
}

/* ---- helpers that must survive their own failure --------------------- */

typedef struct xcall XCALL;
struct xcall {
    void    *arg;
    int     rc;                 /* callee rc, -1 = never reached        */
};

static int dounlk(void *p)
{
    XCALL   *x = (XCALL *)p;

    x->rc = unlock(x->arg, 0);
    return 0;
}

static int doclose(void *p)
{
    XCALL   *x = (XCALL *)p;

    x->rc = fclose((FILE *)x->arg);
    return 0;
}

/* returns the abend code (0 = none), *c_rc = callee rc */
static int trycall(int (*func)(void *), void *arg, int *c_rc)
{
    XCALL   x;
    int     t;

    x.arg = arg;
    x.rc = -1;
    t = try(func, &x);
    *c_rc = x.rc;
    return t;
}

/* ---- round (3) worker ------------------------------------------------ */

typedef struct wrk WRK;
struct wrk {
    ECB         go;             /* main posts: start writing            */
    FILE        *fp;
    const char  *tag;
    const char  *msg;
    int         wrote;          /* lines the task believes it wrote     */
    int         abend;          /* abend code around the write loop     */
};

static int writeloop(void *p)
{
    WRK     *w = (WRK *)p;
    int     i;

    for (i = 0; i < NLINES; i++) {
        logline(w->fp, "[%s] %s\n", w->tag, w->msg);
        w->wrote++;
    }
    return 0;
}

static int writer(void *arg1, void *arg2)
{
    WRK     *w = (WRK *)arg1;

    ecb_wait(&w->go);
    w->abend = try(writeloop, w);
    return 0;
}

/* bounded join: 0 = joined, -2 = still running after JOINMAX yields */
static int join(CTHDTASK **t)
{
    int     i;

    if (!*t) return -1;
    for (i = 0; i < JOINMAX && !((*t)->termecb & ECB_POSTED_BIT); i++) {
        cthread_yield();
    }
    if (!((*t)->termecb & ECB_POSTED_BIT)) return -2;
    cthread_delete(t);
    return 0;
}

/* strip trailing blanks (FB padding) and the newline */
static void rtrim(char *s)
{
    int     n = strlen(s);

    while (n > 0 && (s[n-1] == '\n' || s[n-1] == ' ')) n--;
    s[n] = 0;
}

int main(void)
{
    static int  thing = 0;      /* round (1) lock target                */
    FILE        *fp;
    int         rc, t_rc, u_rc;
    int         r1_nest, r1_test, r1_free;
    int         r2_test;

    printf("TSTIOLK - libc370 #145: FILE lock nesting + concurrent printf\n\n");
    wtof("TSTIOLK start, main TCB=%08X", ((unsigned *)0)[0x21C / 4]);

    /* ---- (1) ENQ/DEQ RET=HAVE semantics, measured ------------------- */
    printf("(1) lock()/unlock() RET=HAVE semantics on this system:\n");

    rc = lock(&thing, 0);
    check("(1) first lock() acquires (rc=0)", rc == 0);

    r1_nest = lock(&thing, 0);
    check("(1) nested lock() answers rc=8, not a wait", r1_nest == 8);

    r1_test = testlock(&thing, 0);
    check("(1) testlock() while held answers 8", r1_test == 8);

    t_rc = trycall(dounlk, &thing, &u_rc);
    check("(1) unlock() releases (rc=0, no abend)", t_rc == 0 && u_rc == 0);

    t_rc = trycall(dounlk, &thing, &u_rc);
    printf("      [unlock-not-held: try=%d rc=%d]\n", t_rc, u_rc);
    check("(1) unlock() when not held: nonzero rc, no abend",
          t_rc == 0 && u_rc != 0);

    r1_free = testlock(&thing, 0);
    check("(1) testlock() after release answers 0", r1_free == 0);

    wtof("TSTIOLK r1: nest=%d test=%d notheld=%d/%d free=%d",
         r1_nest, r1_test, t_rc, u_rc, r1_free);

    /* ---- (2) nested public fputs() must not release the hold -------- */
    printf("\n(2) lock(fp) + public fputs(): caller must still own the lock:\n");

    fp = fopen("dd:OUTDD2", "w");
    if (!check("(2) fopen dd:OUTDD2 for write", fp != NULL)) goto verdict;

    rc = lock(fp, 0);
    check("(2) outer lock(fp) acquires (rc=0)", rc == 0);

    fputs("TSTIOLK round 2\n", fp);

    r2_test = testlock(fp, 0);
    printf("      [testlock after nested fputs: rc=%d]\n", r2_test);
    check("(2) lock still held after nested fputs (testlock=8)",
          r2_test == 8);

    t_rc = trycall(dounlk, fp, &u_rc);
    printf("      [outer unlock: try=%d rc=%d]\n", t_rc, u_rc);
    check("(2) outer unlock() succeeds (rc=0, no abend)",
          t_rc == 0 && u_rc == 0);

    wtof("TSTIOLK r2: testlock=%d unlock=%d/%d", r2_test, t_rc, u_rc);

    fclose(fp);

    /* ---- (3) two tasks vfprintf one FILE: records must stay whole --- */
    printf("\n(3) concurrent vfprintf, %d lines x 2 tasks:\n", NLINES);
    {
        static WRK  wa, wb;
        CTHDTASK    *ta, *tb;
        char        expa[80], expb[80], line[100];
        int         ca = 0, cb = 0, junk = 0, total = 0;
        int         ja, jb, c_try, c_rc;

        memset(&wa, 0, sizeof(wa));
        memset(&wb, 0, sizeof(wb));

        fp = fopen("dd:OUTDD", "w");
        if (!check("(3) fopen dd:OUTDD for write", fp != NULL)) goto verdict;

        wa.fp = fp; wa.tag = TAGA; wa.msg = MSGA;
        wb.fp = fp; wb.tag = TAGB; wb.msg = MSGB;

        ta = cthread_create((void *)writer, &wa, (void *)0);
        tb = cthread_create((void *)writer, &wb, (void *)0);
        if (!check("(3) both writer tasks created", ta && tb)) goto verdict;

        ecb_post(&wa.go, 1);
        ecb_post(&wb.go, 1);
        ja = join(&ta);
        jb = join(&tb);

        wtof("TSTIOLK r3: A wrote=%d abend=%03X join=%d; "
             "B wrote=%d abend=%03X join=%d",
             wa.wrote, wa.abend, ja, wb.wrote, wb.abend, jb);

        check("(3) both writers ended (no hang)", ja == 0 && jb == 0);
        check("(3) no writer abended", wa.abend == 0 && wb.abend == 0);
        if (ja != 0 || jb != 0) {
            /* a wedged writer still owns the FILE; report and get the
               verdict out - the step will abend at termination, the
               job log already carries the whole story */
            goto verdict;
        }

        c_try = trycall(doclose, fp, &c_rc);
        wtof("TSTIOLK r3: fclose try=%03X rc=%d", c_try, c_rc);
        check("(3) fclose survived (no abend)", c_try == 0);

        check("(3) task A wrote all its lines", wa.wrote == NLINES);
        check("(3) task B wrote all its lines", wb.wrote == NLINES);

        sprintf(expa, "[%s] %s", TAGA, MSGA);
        sprintf(expb, "[%s] %s", TAGB, MSGB);

        fp = fopen("dd:OUTDD", "r");
        if (!check("(3) reopen dd:OUTDD for read", fp != NULL)) goto verdict;

        while (fgets(line, sizeof(line), fp) != NULL) {
            total++;
            rtrim(line);
            if      (strcmp(line, expa) == 0) ca++;
            else if (strcmp(line, expb) == 0) cb++;
            else {
                junk++;
                if (junk <= 5) {
                    printf("      corrupt record %d: \"%s\" (len %d)\n",
                           total, line, (int)strlen(line));
                }
            }
        }
        fclose(fp);

        printf("      [records: total=%d A=%d B=%d corrupt=%d]\n",
               total, ca, cb, junk);
        wtof("TSTIOLK r3: records total=%d A=%d B=%d corrupt=%d",
             total, ca, cb, junk);
        check("(3) no corrupt records", junk == 0);
        check("(3) every A line intact", ca == NLINES);
        check("(3) every B line intact", cb == NLINES);
        check("(3) record count adds up", total == 2 * NLINES);
    }

verdict:
    printf("\nTSTIOLK %s\n", bad ? "FAILED" : "PASSED");
    wtof("TSTIOLK %s", bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
