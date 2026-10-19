/*
 * tstmktm.c - libc370 #392 on MVS: mktime() leaves the caller's struct tm
 * alone when it fails, and difftime() of a negative difference is
 * negative.
 *
 *   1 mktime() of a year before 1970 returns -1 and changes no field
 *   2 difftime(0, 1) is -1.0, difftime(1, 0) is 1.0
 *   3 control: mktime() of 2026-10-19 12:00:00 gives 1792411200
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstmktm.c \
 *         -o $S/TSTMKTG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstmktm.c \
 *         -o $S/TSTMKTR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTMKTG=TSTMKTG.iebcopy \
 *         TSTMKTR=TSTMKTR.iebcopy -o mk -xmit --dsn IBMUSER.LIBC370.MKSCR
 *
 * then jcl/recvmktm.jcl and jcl/tstmktm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <time.h>
#include <string.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTMKTM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    struct tm   tm, keep;
    time_t      t;
    volatile time_t a = 0, b = 1;
    double      d1, d2;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 60;
    tm.tm_mon = 5;
    tm.tm_mday = 15;
    tm.tm_hour = 7;
    keep = tm;
    t = mktime(&tm);
    wtof("TSTMKTM   mktime(1960-06-15) %d, year now %d, mday %d",
         (int)t, tm.tm_year, tm.tm_mday);
    want(t == (time_t)-1, "mktime() of 1960 returns -1");
    want(memcmp(&tm, &keep, sizeof(tm)) == 0, "the struct tm is unchanged");

    d1 = difftime(a, b);
    d2 = difftime(b, a);
    wtof("TSTMKTM   difftime(0,1)*1000 %d, difftime(1,0)*1000 %d",
         (int)(d1 * 1000), (int)(d2 * 1000));
    want(d1 == -1.0, "difftime(0, 1) is -1");
    want(d2 == 1.0, "difftime(1, 0) is 1");

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 126;
    tm.tm_mon = 9;
    tm.tm_mday = 19;
    tm.tm_hour = 12;
    t = mktime(&tm);
    want(t == (time_t)1792411200UL, "control: 2026-10-19 12:00:00");
    return bad ? 8 : 0;
}
