/*
 * tstfopn.c - libc370 size analysis F5: the standard streams open through
 *             __fpopn(), fopen() without the handlers for new and
 *             temporary data sets and the TSO streams.  Nothing may change.
 *
 * Compiled once per case with -DCASE=n, against this tree (FPxxx) and the
 * installed library (FRxxx); jcl/tstfopn.jcl runs both, and the verdicts
 * and the output data sets must agree.
 *
 *   1 STD   SYSPRINT, SYSTERM and SYSIN DDs: puts(), fputs(stderr) and
 *           fgets(stdin) use them
 *   2 DYN   no SYSPRINT, SYSTERM or SYSIN: stdout and stderr go to SYSOUT
 *           data sets allocated on the fly, stdin is 'NULLFILE' (EOF)
 *   3 FOP   fopen()'s other names still work: a &temp data set (written;
 *           it is deleted at fclose()), a new
 *           data set by name (written, read back, removed), "a" on an
 *           existing PDS member (EOPNOTSUPP) and on a new one, 'NULLFILE'
 *   4 LEAN  (this tree only) __fpopn() without handlers: 'NULLFILE' for
 *           reading and writing, the standard streams' fallback
 *
 * RC 0 = the case behaved as above, 8 = it did not.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>

#ifndef CASE
#define CASE 1
#endif

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTFOPN %d %s: %s", CASE, cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

#if CASE == 3
static int roundtrip(FILE *w, FILE *(*reopen)(void), const char *text)
{
    char    line[80];
    FILE    *r;
    int     ok;

    if (!w) return 0;
    fputs(text, w);
    fclose(w);
    r = reopen();
    if (!r) return 0;
    ok = fgets(line, sizeof(line), r) != NULL && strcmp(line, text) == 0;
    fclose(r);
    return ok;
}

static FILE *new_r(void) { return fopen("'IBMUSER.LIBC370.F5NEW'", "r"); }
#endif

#if CASE == 4
extern FILE *__fpopn(const char *fn, const char *mode,
                     int (*tsoname)(FILE *fp, const char *fn),
                     int (*dsalloc)(FILE *fp));
#endif

int main(void)
{
#if CASE == 1 || CASE == 2
    char    line[80];
    char    *got;

    want(puts("stdout line") >= 0, "puts()");
    want(fputs("stderr line\n", stderr) >= 0, "fputs(stderr)");
    got = fgets(line, sizeof(line), stdin);
#if CASE == 1
    want(got && strncmp(line, "SYSIN LINE", 10) == 0, "fgets(stdin) reads SYSIN");
#else
    want(got == NULL && feof(stdin), "fgets(stdin) on 'NULLFILE' is EOF");
#endif
#elif CASE == 3
    FILE    *fp;

    /* a &temp data set goes with its fclose(): written only */
    fp = fopen("&&F5TMP", "w");
    want(fp != NULL && fputs("temp\n", fp) >= 0 && fclose(fp) == 0,
         "&temp data set written");
    want(roundtrip(fopen("'IBMUSER.LIBC370.F5NEW'", "w"), new_r, "new\n"),
         "new data set by name");
    want(remove("'IBMUSER.LIBC370.F5NEW'") == 0, "remove() it");

    fp = fopen("dd:PDS(MEM)", "w");
    if (fp) { fputs("m\n", fp); fclose(fp); }
    errno = 0;
    fp = fopen("dd:PDS(MEM)", "a");
    want(fp == NULL && errno == EOPNOTSUPP, "\"a\" on an existing member refused");
    if (fp) fclose(fp);
    fp = fopen("dd:PDS(NEWM)", "a");
    want(fp != NULL, "\"a\" on a new member opens");
    if (fp) fclose(fp);

    fp = fopen("'NULLFILE'", "w");
    want(fp != NULL && fputs("x\n", fp) >= 0, "fopen('NULLFILE', w)");
    if (fp) fclose(fp);
#elif CASE == 4
    char    line[80];
    FILE    *fp = __fpopn("'NULLFILE'", "w", 0, 0);

    want(fp != NULL && fputs("x\n", fp) >= 0, "__fpopn('NULLFILE', w)");
    if (fp) fclose(fp);
    fp = __fpopn("'NULLFILE'", "r", 0, 0);
    want(fp != NULL && fgets(line, sizeof(line), fp) == NULL,
         "__fpopn('NULLFILE', r) is EOF");
    if (fp) fclose(fp);
#endif
    wtof("TSTFOPN %d VERDICT failed=%d", CASE, bad);
    return bad ? 8 : 0;
}
