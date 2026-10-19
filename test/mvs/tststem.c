/*
 * tststem.c - libc370 #395 on MVS: stdio and stdlib edge cases C99
 * defines.
 *
 *   1 fflush(NULL) flushes every output stream and returns 0; it read low
 *     storage as a FILE
 *   2 system(NULL) returns 1 (there is a command processor); it
 *     dereferenced the null pointer and LINKed whatever name it read
 *   3 ungetc() after the end of file: the next fgetc() returns the
 *     character, not EOF
 *   4 freopen() discards a character pushed back onto the old stream
 *
 * 1 and 2 run under try(), so that the red can fail instead of ending the
 * step.  DD RECS1, RECS2 and RECS3 are in-stream data.  Of 1 the red
 * shows nothing here: reading low storage as a FILE returned 0.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tststem.c \
 *         -o $S/TSTSTEG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tststem.c \
 *         -o $S/TSTSTER -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSTEG=TSTSTEG.iebcopy \
 *         TSTSTER=TSTSTER.iebcopy -o se -xmit --dsn IBMUSER.LIBC370.STSCR
 *
 * then jcl/recvstem.jcl and jcl/tststem.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <mvs/wto.h>
#include <mvs/recovery.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTSTEM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static int do_fflush(int *out)
{
    *out = fflush(NULL);
    return 0;
}

static int do_system(int *out)
{
    *out = system(NULL);
    return 0;
}

int main(void)
{
    FILE    *a, *b, *f;
    int     out, rc, c;

    a = fopen("&&SA", "w");
    b = fopen("&&SB", "w");
    if (a) fputs("one", a);
    if (b) fputs("two", b);
    out = -99;
    rc = try(do_fflush, &out);
    wtof("TSTSTEM   fflush(NULL): try rc %08X, result %d", (unsigned)rc, out);
    want(rc == 0 && out == 0, "fflush(NULL) returns 0");
    if (a) fclose(a);
    if (b) fclose(b);

    out = -99;
    rc = try(do_system, &out);
    wtof("TSTSTEM   system(NULL): try rc %08X, result %d", (unsigned)rc, out);
    want(rc == 0 && out == 1, "system(NULL) is 1, without a LINK");

    f = fopen("dd:RECS3", "r");
    if (f) {
        while (fgetc(f) != EOF) ;       /* to the end of file */
        ungetc('Z', f);
        c = fgetc(f);
        wtof("TSTSTEM   after EOF and ungetc('Z'): fgetc %d, feof %d", c, feof(f));
        want(c == 'Z', "ungetc() after EOF is read back");
        fclose(f);
    }
    else want(0, "fopen dd:RECS3");

    f = fopen("dd:RECS1", "r");
    if (f) {
        c = fgetc(f);
        ungetc('Q', f);
        f = freopen("dd:RECS2", "r", f);
        c = f ? fgetc(f) : -2;
        wtof("TSTSTEM   fgetc after freopen: '%c'", c > 0 ? c : '?');
        want(c == 'S', "freopen() discards the pushed-back character");
        if (f) fclose(f);
    }
    else want(0, "fopen dd:RECS1");
    return bad ? 8 : 0;
}
