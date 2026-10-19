/*
 * tststfm.c - libc370 #391 on MVS: strftime().
 *
 *   1 %Z with tm_isdst -1 (every library struct tm) gives an empty string;
 *     it read through a null pointer
 *   2 maxsize 0 writes nothing and returns 0; it wrote the whole result
 *   3 %x is the C99 "C" locale date %m/%d/%y; it printed pointer bits
 *   4 %U on 1 January 2023 (a Sunday) is 01, %W on 1 January 2024 (a
 *     Monday) is 01; both were 00
 *   5 controls: %Y-%m-%d %H:%M:%S and %a %b, as before
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tststfm.c \
 *         -o $S/TSTSTFG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tststfm.c \
 *         -o $S/TSTSTFR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSTFG=TSTSTFG.iebcopy \
 *         TSTSTFR=TSTSTFR.iebcopy -o sf -xmit --dsn IBMUSER.LIBC370.SFSCR
 *
 * then jcl/recvstfm.jcl and jcl/tststfm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <time.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what, const char *got)
{
    wtof("TSTSTFM %s: %s [%s]", cond ? "PASS" : "FAIL", what, got);
    if (!cond) bad++;
}

/* a struct tm for y-m-d (yday and wday given) 13:05:09 */
static struct tm mk(int y, int m, int d, int yday, int wday, int isdst)
{
    struct tm   t;

    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d;
    t.tm_yday = yday; t.tm_wday = wday; t.tm_isdst = isdst;
    t.tm_hour = 13; t.tm_min = 5; t.tm_sec = 9;
    return t;
}

int main(void)
{
    char        buf[64];
    char        guard[16];
    size_t      n;
    struct tm   t;

    /* 4 October 2026 is a Sunday, day 276 of the year */
    t = mk(2026, 10, 4, 276, 0, -1);

    memset(buf, 'X', sizeof(buf));
    n = strftime(buf, sizeof(buf), "<%Z>", &t);
    want(n == 2 && strcmp(buf, "<>") == 0, "%Z with tm_isdst -1 is empty", buf);

    memset(guard, 'G', sizeof(guard));
    memcpy(buf, guard, sizeof(guard));
    n = strftime(buf, 0, "ABCDEF", &t);
    want(n == 0 && memcmp(buf, guard, sizeof(guard)) == 0,
         "maxsize 0 writes nothing, returns 0", n ? "wrote" : "-");

    n = strftime(buf, sizeof(buf), "%x", &t);
    want(n == 8 && strcmp(buf, "10/04/26") == 0, "%x is %m/%d/%y", buf);

    t = mk(2023, 1, 1, 0, 0, 0);
    strftime(buf, sizeof(buf), "%U", &t);
    want(strcmp(buf, "01") == 0, "%U of 2023-01-01 (Sunday) is 01", buf);

    t = mk(2024, 1, 1, 0, 1, 0);
    strftime(buf, sizeof(buf), "%W", &t);
    want(strcmp(buf, "01") == 0, "%W of 2024-01-01 (Monday) is 01", buf);

    /* 2 January 2023 is a Monday: week 1 by %U, week 1 by %W */
    t = mk(2023, 1, 2, 1, 1, 0);
    strftime(buf, sizeof(buf), "%U %W", &t);
    want(strcmp(buf, "01 01") == 0, "%U %W of 2023-01-02 is 01 01", buf);

    t = mk(2026, 10, 4, 276, 0, 0);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %a %b", &t);
    want(strcmp(buf, "2026-10-04 13:05:09 Sun Oct") == 0, "control", buf);

    return bad ? 8 : 0;
}
