/*
 * tstlazy.c - libc370 #564: the standard streams open on first use.
 *
 * Compiled once per case with -DCASE=n; each case is its own program,
 * because __premain is per program.  Every case reads the GRT slots
 * directly - the stdin/stdout/stderr macros would open the streams - and
 * reports which streams are open, so the job log says what the start-up
 * opened behind the program's back.  The JCL runs the steps without a
 * SYSTERM and without a SYSIN DD unless a case needs one: an eager start-up
 * allocates a SYSOUT data set for stderr and 'NULLFILE' for stdin there,
 * which JESYSMSG shows.
 *
 *   1 LZOUT   printf() only            -> stdout open, stderr, stdin not
 *   2 LZERR   fprintf(stderr) only     -> stderr open, stdout, stdin not;
 *             with a SYSENV DD giving SYSOUT_LRECL=99 the dynamic SYSOUT of
 *             stderr gets LRECL 99, since SYSENV is now read first
 *   3 LZIN    fgets(stdin), SYSIN DD   -> the SYSIN record read; stdin open
 *   4 LZPRE   __premain sets stdout to DD:MYOUT -> it is kept, SYSPRINT is
 *             not opened, printf() goes to MYOUT
 *   5 LZFRE   freopen("DD:OUT2", "w", stdout) -> printf() goes to OUT2
 *   6 LZEXIT  main touches nothing, an atexit() handler prints -> works
 *   7 LZNONE  main touches nothing    -> no stream open at the end of main
 *
 * RC 0 = the case behaved as above, 8 = it did not.  Built twice: against
 * this tree's libc.a (LZxxx) and the installed one (LRxxx, red: the
 * start-up opens all three).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/crt.h>
#include <mvs/wto.h>

#ifndef CASE
#define CASE 1
#endif

static int bad;

static void state(const char *when)
{
    CLIBGRT *grt = __grtget();

    wtof("TSTLAZY %d %s: stdin %s stdout %s stderr %s", CASE, when,
         grt->grtin ? "open" : "-", grt->grtout ? "open" : "-",
         grt->grterr ? "open" : "-");
}

static int is(void *slot) __attribute__((unused));
static int is(void *slot)
{
    return slot != 0;
}

static void want(int cond, const char *what)
{
    wtof("TSTLAZY %d %s: %s", CASE, cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

#if CASE == 4
static FILE *mine;
int __premain(char *parm, char *pgmname, void **pgmr1)
{
    (void)parm; (void)pgmname; (void)pgmr1;
    mine = fopen("DD:MYOUT", "w");
    stdout = mine;
    return 0;
}
#endif

#if CASE == 6
static void bye(void)
{
    printf("written from atexit\n");
    wtof("TSTLAZY 6 atexit printf done");
}
#endif

int main(void)
{
    CLIBGRT *grt = __grtget();
    char    line[100];

    state("at main");
#if CASE == 1
    printf("stdout only\n");
    state("after printf");
    want(is(grt->grtout) && !is(grt->grterr) && !is(grt->grtin),
         "stdout open, stderr and stdin not");
#elif CASE == 2
    fprintf(stderr, "stderr only\n");
    state("after fprintf(stderr)");
    if (grt->grterr)    /* SYSENV is read before stderr opens (#564) */
        wtof("TSTLAZY 2 stderr LRECL=%u", ((FILE *)grt->grterr)->lrecl);
    want(is(grt->grterr) && !is(grt->grtout) && !is(grt->grtin),
         "stderr open, stdout and stdin not");
#elif CASE == 3
    memset(line, 0, sizeof(line));
    want(fgets(line, sizeof(line), stdin) != 0
         && strncmp(line, "SYSINREC", 8) == 0, "the SYSIN record read");
    state("after fgets(stdin)");
    want(is(grt->grtin) && !is(grt->grtout) && !is(grt->grterr),
         "stdin open, stdout and stderr not");
#elif CASE == 4
    want(mine && grt->grtout == mine, "__premain's stdout kept");
    printf("to MYOUT\n");
    want(grt->grtout == mine, "printf did not replace it");
#elif CASE == 5
    want(freopen("DD:OUT2", "w", stdout) != 0, "freopen(DD:OUT2, stdout)");
    printf("to OUT2\n");
#elif CASE == 6
    atexit(bye);
    want(!is(grt->grtout), "nothing open before exit");
#elif CASE == 7
    want(!is(grt->grtin) && !is(grt->grtout) && !is(grt->grterr),
         "no stream open");
#endif
    (void)line;
    (void)grt;
    state("at return");
    wtof("TSTLAZY %d VERDICT failed=%d", CASE, bad);
    return bad ? 8 : 0;
}
