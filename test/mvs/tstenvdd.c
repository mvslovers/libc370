/*
 * tstenvdd.c - libc370 size analysis F3: SYSENV read through BSAM, not
 *              fopen().  The environment a program sees must not change.
 *
 * Compiled once per case with -DCASE=n, against this tree (EDxxx) and the
 * installed library (ERxxx); jcl/tstenvdd.jcl runs both with the same DDs
 * and the verdicts must agree.
 *
 *   1 FB    SYSENV DD *: comments ("*", "#"), a line with sequence numbers
 *           in 73-80, trailing blanks, a line without '=' -> FOO=bar,
 *           NUM=12, SP="a b", NOEQ unset
 *   2 FALL  no SYSENV, ENVIRON DD * -> FOO=env
 *   3 DUM   SYSENV DD DUMMY, ENVIRON DD * -> FOO unset: an empty SYSENV
 *           is a SYSENV, ENVIRON is not read
 *   4 MKVB  writes "FOO=vb" and "BAR=two" to DD VBOUT (RECFM=VB) for case 5
 *   5 VB    SYSENV is that VB data set -> FOO=vb, BAR=two
 *   6 BUSY  fopen("dd:SYSENV") holds the spool SYSIN open, then getenv():
 *           SYSENV is not opened a second time (no S013-C0), ENVIRON is
 *           read instead -> FOO=env
 *   7 NONE  no DD -> FOO unset, setenv() works
 *
 * RC 0 = the case behaved as above, 8 = it did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>

#ifndef CASE
#define CASE 1
#endif

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTENVDD %d %s: %s", CASE, cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static int is(const char *name, const char *s) __attribute__((unused));
static int is(const char *name, const char *s)
{
    const char *v = getenv(name);

    wtof("TSTENVDD %d getenv(%s)=%s%s%s", CASE, name,
         v ? "\"" : "", v ? v : "NULL", v ? "\"" : "");
    return s ? (v && strcmp(v, s) == 0) : v == NULL;
}

int main(void)
{
#if CASE == 1
    want(is("FOO", "bar"), "FOO=bar");
    want(is("NUM", "12"), "NUM=12, sequence number removed");
    want(is("SP", "a b"), "SP=a b, trailing blanks removed");
    want(is("NOEQ", NULL), "a line without = sets nothing");
    want(is("X", NULL), "comment lines set nothing");
#elif CASE == 2 || CASE == 6
#if CASE == 6
    FILE    *fp = fopen("dd:SYSENV", "r");

    want(fp != NULL, "fopen(dd:SYSENV) holds the DD");
#endif
    want(is("FOO", "env"), "FOO=env from ENVIRON");
#if CASE == 6
    if (fp) fclose(fp);
#endif
#elif CASE == 3
    want(is("FOO", NULL), "SYSENV DUMMY: FOO unset, ENVIRON not read");
#elif CASE == 4
    FILE    *fp = fopen("dd:VBOUT", "w");

    want(fp != NULL, "fopen(dd:VBOUT)");
    if (fp) {
        fputs("FOO=vb\n# comment\nBAR=two\n", fp);
        want(fclose(fp) == 0, "VB data set written");
    }
#elif CASE == 5
    want(is("FOO", "vb"), "FOO=vb from a VB SYSENV");
    want(is("BAR", "two"), "BAR=two from a VB SYSENV");
#elif CASE == 7
    want(is("FOO", NULL), "no DD: FOO unset");
    setenv("FOO", "set", 1);
    want(is("FOO", "set"), "setenv() works");
#endif
    wtof("TSTENVDD %d VERDICT failed=%d", CASE, bad);
    return bad ? 8 : 0;
}
