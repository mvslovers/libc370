/*
 * tstmktm.c - libc370 #392 on MVS: mktime() leaves the caller's struct tm
 * alone when it fails, difftime() of a negative difference is negative,
 * and mktime() and mktime64() read struct tm as local time.
 *
 *   1 mktime() of a year before 1970 returns -1 and changes no field
 *   2 difftime(0, 1) is -1.0, difftime(1, 0) is 1.0
 *   3 control, zone 0: mktime() of 2026-10-19 12:00:00 gives 1792411200
 *   4 zone UTC-5: mktime(localtime_r(t)) == t, the fields come back as
 *     they were, and "17:01 today" built from 16:58 is 3 minutes later
 *   5 zone UTC+1h: 1970-01-01 00:30 local is before the epoch, -1, no
 *     field changed
 *   6 mktime64() at UTC-5 is timegm64() of the same fields plus 5 hours,
 *     for 2026, 2050 and 2200, and writes the local fields back
 *   7 mktime64() at zone 0 equals timegm64() on the input the JES job
 *     times use (year + day of the year, seconds since midnight)
 *   8 mktime64() out of range (1960; 1970-01-01 00:30 at UTC+1h): -1, no
 *     field changed
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
#include <ext/time64.h>
#include <mvs/wto.h>

static int bad;

static void zones(void);
static void mktime64s(void);

static int same64(const time64_t *a, const time64_t *b)
{
    return a->u32[0] == b->u32[0] && a->u32[1] == b->u32[1];
}

static int is_m1(const time64_t *a)
{
    return a->u32[0] == 0xFFFFFFFFUL && a->u32[1] == 0xFFFFFFFFUL;
}

static void want(int cond, const char *what)
{
    wtof("TSTMKTM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static void zones(void)
{
    struct tm tm, back, keep;
    time_t    t0 = 1792447080UL;    /* 2026-10-19 21:58:00 UTC */
    time_t    t;

    __tzset(-18000);
    localtime_r(&t0, &tm);
    back = tm;
    t = mktime(&back);
    wtof("TSTMKTM   UTC-5: localtime %02d:%02d, mktime %u, want %u",
         tm.tm_hour, tm.tm_min, (unsigned)t, (unsigned)t0);
    want(tm.tm_hour == 16 && tm.tm_min == 58, "UTC-5: localtime is 16:58");
    want(t == t0, "UTC-5: mktime(localtime_r(t)) == t");
    want(memcmp(&tm, &back, sizeof(tm)) == 0, "UTC-5: the fields come back");
    back = tm;
    back.tm_hour = 17;
    back.tm_min = 1;
    back.tm_sec = 0;
    t = mktime(&back);
    want(t == t0 + 180, "UTC-5: 17:01 is 3 minutes after 16:58");

    __tzset(3600);
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 70;
    tm.tm_mday = 1;
    tm.tm_min = 30;
    keep = tm;
    t = mktime(&tm);
    want(t == (time_t)-1, "UTC+1: 1970-01-01 00:30 local gives -1");
    want(memcmp(&tm, &keep, sizeof(tm)) == 0, "UTC+1: the struct tm is unchanged");
    __tzset(0);
}

static void one64(int year, int mon, int mday, int hour, const char *what)
{
    struct tm tm, copy;
    time64_t  got, want_t;

    __tzset(-18000);
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year;
    tm.tm_mon = mon;
    tm.tm_mday = mday;
    tm.tm_hour = hour;
    copy = tm;
    want_t = timegm64(&copy);
    __64_add_i32(&want_t, 18000, &want_t);
    got = mktime64(&tm);
    wtof("TSTMKTM   mktime64 %s: got %08X%08X want %08X%08X, hour now %d",
         what, (unsigned)got.u32[0], (unsigned)got.u32[1],
         (unsigned)want_t.u32[0], (unsigned)want_t.u32[1], tm.tm_hour);
    want(same64(&got, &want_t), what);
    want(tm.tm_hour == hour && tm.tm_mday == mday && tm.tm_year == year,
         "   and the local fields are written back");
    __tzset(0);
}

static void mktime64s(void)
{
    struct tm tm, keep;
    time64_t  a, b;

    one64(126, 9, 19, 16, "mktime64 2026 at UTC-5 = timegm64 + 5h");
    one64(150, 5, 15, 12, "mktime64 2050 at UTC-5 = timegm64 + 5h");
    one64(300, 2, 1, 3, "mktime64 2200 at UTC-5 = timegm64 + 5h");

    __tzset(0);
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 126;
    tm.tm_mday = 292;
    tm.tm_sec = 45296;
    keep = tm;
    a = timegm64(&keep);
    b = mktime64(&tm);
    want(!is_m1(&a) && same64(&a, &b), "zone 0: mktime64 == timegm64, JES input");

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 60;
    tm.tm_mday = 15;
    keep = tm;
    b = mktime64(&tm);
    want(is_m1(&b) && memcmp(&tm, &keep, sizeof(tm)) == 0,
         "mktime64 of 1960: -1, struct tm unchanged");

    __tzset(3600);
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 70;
    tm.tm_mday = 1;
    tm.tm_min = 30;
    keep = tm;
    b = mktime64(&tm);
    want(is_m1(&b) && memcmp(&tm, &keep, sizeof(tm)) == 0,
         "mktime64 of 1970-01-01 00:30 at UTC+1: -1, struct tm unchanged");
    __tzset(0);
}

int main(void)
{
    struct tm   tm, keep;
    time_t      t;
    volatile time_t a = 0, b = 1;
    double      d1, d2;

    /* tzset() first: __tzset() alone is overwritten by the tzset() that the
       first __tzget() runs (a separate defect) */
    tzset();
    __tzset(0);
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

    zones();
    mktime64s();
    return bad ? 8 : 0;
}
