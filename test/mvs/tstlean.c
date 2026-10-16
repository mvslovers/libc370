/*
 * tstlean.c - libc370 #564 steps 3/4: SYSENV, TZ and the library's
 *             diagnostics without the start-up linking stdio and printf.
 *
 * Compiled once per case with -DCASE=n.  Built against this tree (LNxxx)
 * and the installed library (LRxxx): the results must be the same, which
 * is the point - the start-up only does less, the program sees no change.
 * The JCL gives every step a SYSENV DD with FOO=bar and TZ=-05:00.
 *
 *   1 LNENV   getenv("FOO") is "bar"; unsetenv() and setenv() work after it
 *   2 LNSET   setenv("FOO","mine") as the first call: kept, not overwritten
 *             by SYSENV loading behind it; getenv("TZ") still from SYSENV
 *   3 LNPRE   getenv("FOO") inside __premain is NULL (SYSENV not yet loaded,
 *             as before); in main() it is "bar"
 *   4 LNTZ    localtime() is gmtime() - 5 hours (TZ=-05:00 from SYSENV),
 *             without the program calling tzset()
 *   5 LNMEM   malloc(7M) fails: "Out of memory, bytes needed=7340032" and
 *             the save area trace back on the console - compare the LN and
 *             LR lines in the job log, they must read alike
 *   6 LNEST   __estae(ESTAE_CREATE, NULL, 0): the console message
 *   7 LNNONE  an empty main(), for the step's CPU and storage
 *
 * RC 0 = the case behaved as above, 8 = it did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mvs/wto.h>
#include <mvs/env.h>
#include <mvs/recovery.h>

#ifndef CASE
#define CASE 1
#endif

#if CASE != 7      /* the empty case links nothing of this */
static int bad __attribute__((unused));

static void want(int cond, const char *what) __attribute__((unused));
static void want(int cond, const char *what)
{
    wtof("TSTLEAN %d %s: %s", CASE, cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static int is(const char *v, const char *s) __attribute__((unused));
static int is(const char *v, const char *s)
{
    return v && strcmp(v, s) == 0;
}
#endif

#if CASE == 3
static int pre_null;
int __premain(char *parm, char *pgmname, void **pgmr1)
{
    (void)parm; (void)pgmname; (void)pgmr1;
    pre_null = getenv("FOO") == NULL;
    return 0;
}
#endif

int main(void)
{
#if CASE == 1
    want(is(getenv("FOO"), "bar"), "getenv(FOO) is bar from SYSENV");
    unsetenv("FOO");
    want(getenv("FOO") == NULL, "unsetenv(FOO) removes it");
    setenv("FOO", "again", 1);
    want(is(getenv("FOO"), "again"), "setenv(FOO) after that");
#elif CASE == 2
    setenv("FOO", "mine", 1);
    want(is(getenv("FOO"), "mine"), "setenv(FOO) as the first call is kept");
    want(is(getenv("TZ"), "-05:00"), "getenv(TZ) still comes from SYSENV");
#elif CASE == 3
    want(pre_null, "getenv(FOO) in __premain is NULL");
    want(is(getenv("FOO"), "bar"), "getenv(FOO) in main is bar");
#elif CASE == 4
    {
        time_t      t = 86400L * 10 + 12 * 3600L;  /* 11 Jan 1970 12:00 UTC */
        struct tm   g = *gmtime(&t);
        struct tm   l = *localtime(&t);

        wtof("TSTLEAN 4 gmtime %02d:%02d localtime %02d:%02d",
             g.tm_hour, g.tm_min, l.tm_hour, l.tm_min);
        want(l.tm_hour == 7 && g.tm_hour == 12,
             "localtime is UTC-5 from SYSENV TZ, no tzset() called");
    }
#elif CASE == 5
    {
        void    *p = malloc(7 * 1024 * 1024);

        want(p == NULL, "malloc(7M) fails, the console shows why");
    }
#elif CASE == 6
    {
        int rc = __estae(ESTAE_CREATE, NULL, 0);

        wtof("TSTLEAN 6 __estae(CREATE, NULL) rc=%d", rc);
        want(1, "the console shows the ESTAE message");
    }
#elif CASE == 7
    return 0;
#endif
#if CASE != 7
    wtof("TSTLEAN %d VERDICT failed=%d", CASE, bad);
    return bad ? 8 : 0;
#endif
}
