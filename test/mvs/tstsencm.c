/*
 * tstsencm.c - libc370 #475 on MVS: setenv() looks a variable up by its
 * exact name.
 *
 * setenv() fell back to a search that ignored case when no name matched
 * exactly.  getenv() and unsetenv() compare exactly, so the fallback made
 * setenv("tz", ...) rename an existing TZ (getenv("TZ") then NULL), and
 * setenv("tz", v, 0) returned 0 without setting tz.
 *
 *   1 setenv("TSTC", "1", 1), setenv("tstc", "2", 1): two variables,
 *     getenv("TSTC") "1", getenv("tstc") "2"
 *   2 setenv("TSTD", "1", 1), setenv("tstd", "2", 0): tstd is "2"
 *   3 setenv("TSTC", "3", 1) replaces TSTC only; getenvi() still finds
 *     a name regardless of case
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstsencm.c \
 *         -o $S/TSTSENCG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstsencm.c \
 *         -o $S/TSTSENCR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSENCG=TSTSENCG.iebcopy \
 *         TSTSENCR=TSTSENCR.iebcopy -o sc -xmit --dsn IBMUSER.LIBC370.SCSCR
 *
 * then jcl/recvsencm.jcl and jcl/tstsencm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/env.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTSENCM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static int is(const char *v, const char *s)
{
    return v && strcmp(v, s) == 0;
}

int main(void)
{
    const char  *a, *b;
    int         rc;

    setenv("TSTC", "1", 1);
    setenv("tstc", "2", 1);
    a = getenv("TSTC");
    b = getenv("tstc");
    wtof("TSTSENCM   TSTC=\"%s\" tstc=\"%s\"", a ? a : "(null)", b ? b : "(null)");
    want(is(a, "1") && is(b, "2"), "TSTC and tstc are two variables");

    setenv("TSTD", "1", 1);
    rc = setenv("tstd", "2", 0);
    a = getenv("TSTD");
    b = getenv("tstd");
    wtof("TSTSENCM   rc %d TSTD=\"%s\" tstd=\"%s\"", rc,
         a ? a : "(null)", b ? b : "(null)");
    want(rc == 0 && is(a, "1") && is(b, "2"),
         "setenv(\"tstd\", \"2\", 0) beside TSTD sets tstd");

    setenv("TSTC", "3", 1);
    want(is(getenv("TSTC"), "3") && is(getenv("tstc"), "2"),
         "setenv(\"TSTC\", \"3\", 1) replaces TSTC only");
    want(getenvi("tstd") != NULL, "getenvi() still ignores case");
    return bad ? 8 : 0;
}
