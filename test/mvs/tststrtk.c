/*
 * tststrtk.c - libc370: library functions must not move the caller's
 * strtok() position.
 *
 * strtok() keeps its position in the CRT (crt->crtstrtk), one per task.  A
 * library function that tokenises with strtok() itself resets it, so a
 * caller looping with strtok() and calling that function in the loop loses
 * its place: the next strtok(NULL, ...) continues inside the library's
 * string.  #301 fixed jesjob(); __dsalc() already saved and restored the
 * position.  This probe checks the rest, each in the shape
 *
 *     t1 = strtok(list, ",");      -> "ONE"
 *     <library call>
 *     t2 = strtok(NULL, ",");      -> must be "TWO"
 *
 * CASES
 *   (1) fopen() creating a data set with DCB keywords (__fpnew(), and
 *       __txspac()/__txvols() behind it)
 *   (2) fopen("*X", "w,recfm=...,lrecl=...") - a SYSOUT data set with DCB
 *       keywords (__fpstar())
 *   (3) __listds()
 *   (4) __listvl() with a VATLST member (its comment parsing)
 *   (5) __dsalc() - the control: it saved the position before this change
 *
 * Built twice: TSTSTK against this tree's libc.a, TSTSTKR against the
 * installed sysroot libc.a - the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Iinclude -L build/sdk \
 *                test/mvs/tststrtk.c -o TSTSTK -flinker-output=iebcopy
 *          cc370 -O1 -Iinclude \
 *                test/mvs/tststrtk.c -o TSTSTKR -flinker-output=iebcopy
 *          ld370 --pack TSTSTK=TSTSTK.iebcopy TSTSTKR=TSTSTKR.iebcopy \
 *                -o tststrtk -xmit --dsn IBMUSER.LIBC370.STKSCR
 * Install: jcl/recvstk.jcl.   Run: jcl/tststrtk.jcl.
 *
 * The work data set IBMUSER.LIBC370.STRTK.WORK is created and deleted by
 * case (1); it must not exist when the job starts.
 *
 * mvsdev JOB01360, 2026-10-04 (RECEIVE JOB01359): GREEN CC 0000, 5/5.  RED
 * (the installed library) CC 0001, 1/5: after (1), (2) and (4) the caller's
 * next strtok() returned NULL, after (3) "FUNCTION C" - a piece of the
 * IDCAMS output; (5) __dsalc() passed on both.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include "mvs/dslist.h"
#include "mvs/dynalloc.h"

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define DSN "IBMUSER.LIBC370.STRTK.WORK"

static char list[32];

static void before(void)
{
    char *t;

    strcpy(list, "ONE,TWO,THREE");
    t = strtok(list, ",");
    if (!t || strcmp(t, "ONE")) printf("    (strtok itself: %s)\n", t ? t : "NULL");
}

static void after(const char *what)
{
    char *t = strtok(NULL, ",");
    char  msg[96];

    sprintf(msg, "%s: the caller's next token is TWO", what);
    CHECK(t && strcmp(t, "TWO") == 0, msg);
    if (!t || strcmp(t, "TWO")) printf("    got %s%s%s\n", t ? "\"" : "",
                                       t ? t : "NULL", t ? "\"" : "");
}

int main(void)
{
    FILE    *fp;
    DSLIST  **dl;
    VOLLIST **vl;
    char     dd[9];

    printf("=== tststrtk: library calls keep the caller's strtok() ===\n\n");

    before();
    fp = fopen("'" DSN "'", "w,recfm=fb,lrecl=80,blksize=800,"
               "space=trk(1,1),unit=sysda,volser=pub001");
    after("(1) fopen() creating a data set");
    if (fp) {
        fclose(fp);
        if (__dsalcf(dd, "DSN=%s;DISP=(OLD,DELETE)", DSN) == 0) __dsfree(dd);
    }
    else printf("    (fopen() returned NULL - the parse ran all the same)\n");

    before();
    fp = fopen("*STKOUT", "w,recfm=fb,lrecl=121,blksize=1210");
    after("(2) fopen() of SYSOUT with DCB keywords");
    if (fp) fclose(fp);

    before();
    dl = __listds("SYS2", "NONVSAM VOLUME", NULL);
    after("(3) __listds()");
    if (dl) __freeds(&dl);

    before();
    vl = __listvl(NULL, 0, "VATLST00");
    after("(4) __listvl() with VATLST00");
    if (vl) __freevl(&vl);

    before();
    if (__dsalcf(dd, "DSN=SYS1.PARMLIB;DISP=SHR") == 0) __dsfree(dd);
    after("(5) __dsalc() (control)");

    printf("\n=== tststrtk: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
