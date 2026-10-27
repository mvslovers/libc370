/*
 * tstmktime.c - libc370 #392: mktime() reads struct tm as local time.
 *
 * Links the REAL src/time/mktime.c, src/ext/time64/timegm.c, gmtimer.c and
 * localtmr.c with the scalar date helpers, renamed on the command line so
 * they do not meet the host's own mktime(), timegm(), gmtime_r() and
 * localtime_r().  __tzget() is the test's: the zone offset is a variable
 * here, set per case.  Red before the fix: mktime() was timegm(), so
 * mktime(localtime_r(t)) was t + zone offset for every offset but 0.
 *
 * What it checks, for the offsets 0, -5h, +1h, +5h30, -12h, +14h
 *   1 round trip: localtime_r(t) -> mktime() gives t back and the same
 *     fields, every 86400 s from 1970 through 2105 (as far as the
 *     local fields stay in 1970..2105)
 *   2 the report behind #392: the zone at UTC-5, local 16:58, hour and
 *     minute changed to "17:01 today", mktime() is 3 minutes ahead of
 *     the time of day it started from, not 5 hours behind
 *   3 out of range: a year before 1970 or after 2105, a result before the
 *     epoch (1970-01-01 00:30 local at UTC+1h) and a result past 2**32-2
 *     (2105-12-31 23:59:59 local, zone 46 days west: no real zone gets
 *     there, __tzset() takes any int) give -1 and leave every field
 *     as it was
 *   4 normalisation: hour 25 of a day carries into the next local day
 *   5 offset 0: mktime() equals timegm() on a grid
 *
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -Wextra -U__LP64__ -D'__asm__(...)=' \
 *        -D'asm(...)=' -D__volatile__= -D__32BIT__ -Dtimegm=lc_timegm \
 *        -Dmktime=lc_mktime -Dgmtime_r=lc_gmtime_r \
 *        -Dlocaltime_r=lc_localtime_r -D__tzget=lc_tzget \
 *        -I ../../include -I ../.. -o t tstmktime.c \
 *        ../../src/time/mktime.c ../../src/ext/time64/timegm.c \
 *        ../../src/time/gmtimer.c ../../src/time/localtmr.c \
 *        ../../src/time/@@ymdts.c ../../src/time/@@stymd.c \
 *        ../../src/time/@@mtd.c ../../src/time/@@ytd.c \
 *        ../../src/time/@@isleap.c
 *     ./t                                             # rc 0 when green
 */
#include <time.h>
#include <ext/time64.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; }                                       \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static int zone;

int __tzget(void) { return zone; }

static const struct tm tm_zero;
static const int zones[] = { 0, -18000, 3600, 19800, -43200, 50400 };
#define NZONES ((int)(sizeof(zones) / sizeof(zones[0])))

static void roundtrip(void)
{
    int      z;
    unsigned long bad = 0, n = 0;
    char     msg[96];

    for (z = 0; z < NZONES; z++) {
        uint32_t t;

        zone = zones[z];
        /* up to 2105-12-31: later local fields are year 2106, out of range */
        for (t = 100000UL; t < 4291700000UL; t += 86400UL) {
            struct tm a, b;
            time_t    tt = t, back;

            localtime_r(&tt, &a);
            b = a;
            back = mktime(&b);
            n++;
            if ((uint32_t)back != t || memcmp(&a, &b, sizeof(a)) != 0) {
                if (bad < 3)
                    printf("  zone %d t %lu: back %lu\n", zone,
                           (unsigned long)t, (unsigned long)back);
                bad++;
            }
        }
    }
    snprintf(msg, sizeof(msg),
             "1 mktime(localtime_r(t)) == t on %lu inputs, %lu differ", n, bad);
    CHECK(bad == 0, msg);
    printf("  1 round trip: %lu inputs, %lu differ\n", n, bad);
}

static void report(void)
{
    struct tm a, b;
    time_t    t = 1792447080UL;  /* 2026-10-19 21:58:00 UTC = 16:58 at UTC-5 */
    time_t    r;

    zone = -18000;
    localtime_r(&t, &a);
    CHECK(a.tm_hour == 16 && a.tm_min == 58, "2 setup: local 16:58");
    b = a;
    b.tm_hour = 17;
    b.tm_min = 1;
    b.tm_sec = 0;
    r = mktime(&b);
    CHECK(r == t + 180, "2 17:01 is 3 minutes after 16:58, not 5 hours before");
    CHECK(b.tm_hour == 17 && b.tm_min == 1 && b.tm_mday == 19,
          "2 fields stay local");
}

static int unchanged(int y, int mo, int d, int h, int mi, int s, int tz)
{
    struct tm a, keep;
    time_t    r;

    a = tm_zero;
    a.tm_year = y;
    a.tm_mon = mo;
    a.tm_mday = d;
    a.tm_hour = h;
    a.tm_min = mi;
    a.tm_sec = s;
    a.tm_wday = 3;          /* garbage that must survive */
    a.tm_yday = 99;
    keep = a;
    zone = tz;
    r = mktime(&a);
    return r == (time_t)-1 && memcmp(&a, &keep, sizeof(a)) == 0;
}

static void outofrange(void)
{
    CHECK(unchanged(69, 5, 15, 7, 0, 0, 0), "3 year 1969 at UTC");
    CHECK(unchanged(69, 5, 15, 7, 0, 0, -18000), "3 year 1969 at UTC-5");
    CHECK(unchanged(206, 0, 1, 0, 0, 0, 0), "3 year 2106 at UTC");
    CHECK(unchanged(70, 0, 1, 0, 30, 0, 3600),
          "3 1970-01-01 00:30 local at UTC+1h is before the epoch");
    CHECK(unchanged(205, 11, 31, 23, 59, 59, -4000000),
          "3 2105-12-31 23:59:59 local, zone 46 days west, is past 2**32-2");
}

static void normalise(void)
{
    struct tm a, b;
    time_t    ra, rb;

    zone = -18000;
    a = tm_zero;
    a.tm_year = 126; a.tm_mon = 9; a.tm_mday = 19; a.tm_hour = 25;
    b = tm_zero;
    b.tm_year = 126; b.tm_mon = 9; b.tm_mday = 20; b.tm_hour = 1;
    ra = mktime(&a);
    rb = mktime(&b);
    CHECK(ra == rb && ra != (time_t)-1, "4 hour 25 is hour 1 of the next day");
    CHECK(a.tm_mday == 20 && a.tm_hour == 1 && a.tm_wday == 2,
          "4 fields carried, tm_wday set (2026-10-20 is a Tuesday)");
    /* 2026-10-20 01:00 at UTC-5 is 06:00 UTC */
    CHECK(ra == 1792476000UL, "4 value is 2026-10-20 06:00 UTC");
}

static void utc(void)
{
    unsigned bad = 0, n = 0;
    int      y, d;
    char     msg[96];

    zone = 0;
    for (y = 70; y <= 205; y += 5)
        for (d = 1; d <= 366; d += 13) {
            struct tm a, b;
            time_t    ra, rb;

            a = tm_zero;
            a.tm_year = y;
            a.tm_mday = d;
            a.tm_sec = 12345;
            b = a;
            ra = mktime(&a);
            rb = timegm(&b);
            n++;
            if (ra != rb || memcmp(&a, &b, sizeof(a)) != 0) bad++;
        }
    snprintf(msg, sizeof(msg), "5 offset 0: mktime() == timegm() on %u, %u differ",
             n, bad);
    CHECK(bad == 0, msg);
}

int main(void)
{
    printf("=== tstmktime: mktime() local time (#392) ===\n");
    roundtrip();
    report();
    outofrange();
    normalise();
    utc();
    printf("=== tstmktime: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
