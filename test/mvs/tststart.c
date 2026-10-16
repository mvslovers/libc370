/*
 * tststart.c - libc370 #564: where does the C start-up spend its CPU?
 *
 * A measurement, not a test: it decides nothing and asserts nothing but
 * that the timer works.  The task CPU timer (STIMER TASK, read with
 * TTIMER in timer units of 26.04166 microseconds) counts only while this
 * task executes, so it measures CPU time, not elapsed time; clock()
 * returns -1 on MVS and cannot.
 *
 *   (s) __rtinit() as a whole: the timer starts in __premain(), which
 *       @@START calls before __rtinit(), and is read first thing in main()
 *   per operation, N times each, CPU per call:
 *   (o1) fopen("DD:OUT1","w") of a SYSOUT DD + one line + fclose - what
 *        stdout costs (open and close)
 *   (o2) fopen("*TERMX","w") without a TERMX DD: dynamic SYSOUT, open,
 *        close, free - what stderr costs when SYSTERM has no DD
 *   (o3) fopen("dd:INX","r") without the DD, then fopen("'NULLFILE'","r")
 *        + fclose - what stdin costs when SYSIN has no DD
 *   (o4) fopen("dd:IN1","r") of an instream DD + fclose - stdin with SYSIN
 *   (o5) loadenv("dd:NOENV") without the DD - SYSENV absent
 *   (o6) tzset()
 *   (o7) malloc(256K) + free - the size of the default stack
 *   (o0) an empty loop, for the timer's own cost
 *
 * Results go to SYSPRINT and to the console (TSTSTART lines).  RC 0 if
 * the timer ran, 8 if not.  DDs: OUT1 SYSOUT, IN1 instream; INX, TERMX
 * and NOENV must NOT be allocated.
 *
 * Build:   cc370 -O1 -Wall -Werror test/mvs/tststart.c -o TSTSTART \
 *                -flinker-output=iebcopy   (and EMPTY/EMPTYS, see the JCL)
 * Run:     jcl/recvst.jcl, jcl/tststart.jcl.  Results: issue #564.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mvs/wto.h>
#include <mvs/env.h>

#define N       10
#define TU_NS   26042           /* one timer unit, in nanoseconds (rounded) */

static unsigned intvl = 100000; /* BINTVL: 1000 s, in 0.01 s */
static unsigned t_pre;          /* timer at __premain()       */
static int      started;

static unsigned ttimer(void)
{
    unsigned left;

    __asm__("TTIMER\n\t"
            "LR\t%0,0"
            : "=r"(left) : : "0", "1", "14", "15", "memory");
    return left;
}

int __premain(char *parm, char *pgmname, void **pgmr1)
{
    (void)parm; (void)pgmname; (void)pgmr1;
    __asm__("STIMER TASK,BINTVL=(%0)"
            : : "r"(&intvl) : "0", "1", "14", "15", "memory");
    started = 1;
    t_pre = ttimer();
    return 0;
}

static void report(const char *what, unsigned tu, int n)
{
    unsigned long us = (unsigned long)tu * TU_NS / 1000 / (unsigned)n;

    printf("  %-44s %8lu us\n", what, us);
    wtof("TSTSTART %-40s %lu us", what, us);
}

int main(void)
{
    unsigned    t_main = ttimer();
    unsigned    a, b;
    FILE        *fp;
    void        *p;
    int         i;

    printf("=== tststart: #564 - where the start-up CPU goes ===\n");
    if (!started || t_pre == 0) {
        printf("  the task timer did not start - CANNOT MEASURE\n");
        wtof("TSTSTART timer did not start");
        return 8;
    }
    report("(s) __rtinit(): streams, SYSENV, tzset", t_pre - t_main, 1);

    a = ttimer();
    for (i = 0; i < N; i++) ;
    b = ttimer();
    report("(o0) empty loop", a - b, N);

    a = ttimer();
    for (i = 0; i < N; i++) {
        fp = fopen("DD:OUT1", "w");
        if (fp) { fputs("line\n", fp); fclose(fp); }
    }
    b = ttimer();
    report("(o1) SYSOUT DD: open + 1 line + close", a - b, N);

    a = ttimer();
    for (i = 0; i < N; i++) {
        fp = fopen("*TERMX", "w");
        if (fp) fclose(fp);
    }
    b = ttimer();
    report("(o2) no DD: dynamic SYSOUT + open + close", a - b, N);

    a = ttimer();
    for (i = 0; i < N; i++) {
        fp = fopen("dd:INX", "r");
        if (!fp) fp = fopen("'NULLFILE'", "r");
        if (fp) fclose(fp);
    }
    b = ttimer();
    report("(o3) no SYSIN: NULLFILE alloc + open + close", a - b, N);

    a = ttimer();
    for (i = 0; i < N; i++) {
        fp = fopen("dd:IN1", "r");
        if (fp) fclose(fp);
    }
    b = ttimer();
    report("(o4) instream DD: open + close", a - b, N);

    a = ttimer();
    for (i = 0; i < N; i++) loadenv("dd:NOENV");
    b = ttimer();
    report("(o5) loadenv() without the DD", a - b, N);

    a = ttimer();
    for (i = 0; i < N; i++) tzset();
    b = ttimer();
    report("(o6) tzset()", a - b, N);

    a = ttimer();
    for (i = 0; i < N; i++) {
        p = malloc(256 * 1024);
        free(p);
    }
    b = ttimer();
    report("(o7) malloc(256K) + free", a - b, N);

    __asm__("TTIMER CANCEL" : : : "0", "1", "14", "15", "memory");
    printf("=== tststart: done ===\n");
    return 0;
}
