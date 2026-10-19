/*
 * tstsenvm.c - libc370 #475 on MVS: setenv() looks the variable up by
 * its trimmed name.
 *
 * setenv() trimmed the blanks around the name for the entry it built, but
 * looked up the untrimmed string.  A SYSENV record "NAME = value" reaches
 * it as "NAME " (putenv() splits at '='), which never matched NAME, so a
 * second such record added a second entry and getenv() kept the first.
 *
 *   1 SYSENV "TSTA = 1" then "TSTA = 2": getenv("TSTA") is "2" and the
 *     environment holds one TSTA
 *   2 setenv("TSTB ", "x", 1) then setenv("TSTB", "y", 1): one TSTB, "y"
 *   3 setenv(" TSTB ", "z", 0) leaves "y" (no rewrite), still one TSTB
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstsenvm.c \
 *         -o $S/TSTSENVG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstsenvm.c \
 *         -o $S/TSTSENVR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSENVG=TSTSENVG.iebcopy \
 *         TSTSENVR=TSTSENVR.iebcopy -o se -xmit --dsn IBMUSER.LIBC370.SESCR
 *
 * then jcl/recvsenvm.jcl and jcl/tstsenvm.jcl (SYSENV DD * per step).
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/crt.h>
#include <mvs/env.h>
#include <ext/array.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTSENVM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* entries in the environment whose name is exactly name */
static int entries(const char *name)
{
    CLIBGRT     *grt = __grtget();
    unsigned    i, n;
    int         hits = 0;

    if (!grt || !grt->grtenv) return 0;
    n = arraycount(&grt->grtenv);
    for (i = 0; i < n; i++) {
        __ENVVAR *e = grt->grtenv[i];
        if (e && e->name && strcmp(e->name, name) == 0) hits++;
    }
    return hits;
}

int main(void)
{
    const char  *v;

    v = getenv("TSTA");     /* loads SYSENV */
    wtof("TSTSENVM   TSTA=\"%s\", %d entries", v ? v : "(null)", entries("TSTA"));
    want(v && strcmp(v, "2") == 0, "SYSENV TSTA = 1, TSTA = 2: getenv() is \"2\"");
    want(entries("TSTA") == 1, "SYSENV TSTA twice: one entry");

    setenv("TSTB ", "x", 1);
    setenv("TSTB", "y", 1);
    v = getenv("TSTB");
    want(v && strcmp(v, "y") == 0 && entries("TSTB") == 1,
         "setenv(\"TSTB \") then setenv(\"TSTB\"): one entry, \"y\"");

    setenv(" TSTB ", "z", 0);
    v = getenv("TSTB");
    want(v && strcmp(v, "y") == 0 && entries("TSTB") == 1,
         "setenv(\" TSTB \", \"z\", 0) does not rewrite");
    return bad ? 8 : 0;
}
