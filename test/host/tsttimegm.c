/*
 * tsttimegm.c - libc370 #392: timegm(), the 32-bit UTC inverse of gmtime().
 *
 * Links the REAL src/ext/time64/timegm.c, gmtimer.c and
 * the scalar date helpers, renamed on the command line so they do not meet
 * the host's own timegm() and gmtime_r().  Red before #392:
 * src/ext/time64/timegm.c does not exist and <ext/time64.h> declares no
 * timegm(), so the build fails.
 *
 * What it checks
 *   1 timegm() of fixed dates against an independent calendar computation
 *     (days from the civil date, 64-bit): the epoch, leap days 1972, 2000,
 *     2024, 2100 (no leap day), the 2038 edge either side of 2**31, the
 *     last second of 2105
 *   2 round trip: gmtime_r(t) -> timegm() gives t back and the same fields,
 *     every 86399 s + 1 from 1970 through 2105 (about 49,700 dates)
 *   3 normalisation: seconds, minutes, hours and days beyond their range
 *     (tm_mday as day of the year, as the DSLIST code uses it), tm_wday and
 *     tm_yday filled in, tm_isdst -1
 *   4 out of range: years 1969 and 2106 give -1 and leave every field as it
 *     was
 *   5 the moved callers keep their results: timegm() equals the arithmetic
 *     mktime() had before #392 (copied below as old_mktime()), result and
 *     every field, over a grid that includes the DSLIST and JES inputs
 *     (year + day of the year, seconds since midnight).  mktime() itself
 *     reads local time now and is tested in tstmktime.c
 *
 * time_t is 32 bits on MVS and 64 on the host.  Every result is compared as
 * uint32_t, and the inputs stay inside 1970..2105, where both agree.
 *
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -Wextra -U__LP64__ -D'__asm__(...)=' \
 *        -D'asm(...)=' -D__volatile__= -D__32BIT__ -Dtimegm=lc_timegm \
 *        -Dgmtime_r=lc_gmtime_r -I ../../include \
 *        -I ../.. -o t tsttimegm.c ../../src/ext/time64/timegm.c \
 *        ../../src/time/gmtimer.c \
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

/* string.h's memset() is S/370 inline assembler: zero by assignment */
static const struct tm tm_zero;

extern long __ymdts(unsigned yr, unsigned mo, unsigned day);

/* mktime() as it was before #392, verbatim but for gmtime() -> gmtime_r()
   into a temporary (gmtime() needs the CRT's static buffer) */
static time_t old_mktime(struct tm *timeptr)
{
    time_t tt;
    struct tm tmp;

    if ((timeptr->tm_year < 70) || (timeptr->tm_year > 205))
    {
        return (time_t)-1;
    }
    else
    {
        tt = __ymdts(timeptr->tm_year + 1900,
                     timeptr->tm_mon + 1,
                     timeptr->tm_mday)
             - 719163;
        tt = tt * 24 + timeptr->tm_hour;
        tt = tt * 60 + timeptr->tm_min;
        tt = tt * 60 + timeptr->tm_sec;
    }
    *timeptr = *gmtime_r(&tt, &tmp);
    return (tt);
}

/* days since 1970-01-01 of a proleptic Gregorian date (H. Hinnant) */
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    int64_t  era;
    unsigned yoe, doy, doe;

    y  -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void settm(struct tm *tm, int y, int mo, int d, int h, int mi, int s)
{
    *tm = tm_zero;
    tm->tm_year = y - 1900;
    tm->tm_mon  = mo - 1;
    tm->tm_mday = d;
    tm->tm_hour = h;
    tm->tm_min  = mi;
    tm->tm_sec  = s;
}

static int sametm(const struct tm *a, const struct tm *b)
{
    return a->tm_year == b->tm_year && a->tm_mon == b->tm_mon
        && a->tm_mday == b->tm_mday && a->tm_hour == b->tm_hour
        && a->tm_min == b->tm_min && a->tm_sec == b->tm_sec
        && a->tm_wday == b->tm_wday && a->tm_yday == b->tm_yday
        && a->tm_isdst == b->tm_isdst;
}

static void fixed(void)
{
    static const struct { int y, mo, d, h, mi, s; const char *what; } c[] = {
        { 1970,  1,  1,  0,  0,  0, "the epoch" },
        { 1970, 12, 31, 23, 59, 59, "end of 1970" },
        { 1972,  2, 29, 12,  0,  0, "leap day 1972" },
        { 1972,  3,  1,  0,  0,  0, "day after leap day 1972" },
        { 1999, 12, 31, 23, 59, 59, "end of 1999" },
        { 2000,  2, 29,  0,  0,  0, "leap day 2000 (div by 400)" },
        { 2024,  2, 29,  6, 30, 15, "leap day 2024" },
        { 2026, 10, 19, 12,  0,  0, "2026-10-19 12:00:00" },
        { 2038,  1, 19,  3, 14,  7, "2038 edge: 2**31 - 1" },
        { 2038,  1, 19,  3, 14,  8, "2038 edge: 2**31" },
        { 2100,  2, 28, 23, 59, 59, "2100-02-28 (no leap day)" },
        { 2100,  3,  1,  0,  0,  0, "2100-03-01" },
        { 2105, 12, 31, 23, 59, 59, "last second of 2105" },
    };
    char msg[96];
    size_t i;

    for (i = 0; i < sizeof(c) / sizeof(c[0]); i++) {
        struct tm tm;
        int64_t want = days_from_civil(c[i].y, (unsigned)c[i].mo,
                                       (unsigned)c[i].d) * 86400
                     + c[i].h * 3600 + c[i].mi * 60 + c[i].s;
        uint32_t got;

        settm(&tm, c[i].y, c[i].mo, c[i].d, c[i].h, c[i].mi, c[i].s);
        got = (uint32_t)timegm(&tm);
        snprintf(msg, sizeof(msg), "1 %s: %u, want %u", c[i].what,
                 (unsigned)got, (unsigned)(uint32_t)want);
        CHECK(got == (uint32_t)want, msg);
        CHECK(tm.tm_year == c[i].y - 1900 && tm.tm_mon == c[i].mo - 1
              && tm.tm_mday == c[i].d && tm.tm_hour == c[i].h
              && tm.tm_min == c[i].mi && tm.tm_sec == c[i].s, msg);
    }
    {
        struct tm tm;
        settm(&tm, 2026, 10, 19, 12, 0, 0);
        CHECK((uint32_t)timegm(&tm) == 1792411200u,
              "1 control: 2026-10-19 12:00:00 is 1792411200");
        CHECK(tm.tm_wday == 1 && tm.tm_yday == 291,
              "1 2026-10-19 is a Monday, day 291 of the year");
    }
}

static void roundtrip(void)
{
    uint32_t t, last = 4291747199u;     /* 2105-12-31 23:59:59 */
    unsigned n = 0, bad = 0;
    char msg[96];

    for (t = 0; ; t += 86399u + 1u + (t & 7u)) {
        time_t tt = t, back;
        struct tm tm, ref;

        gmtime_r(&tt, &tm);
        ref = tm;
        back = timegm(&tm);
        n++;
        if ((uint32_t)back != t || !sametm(&tm, &ref)) {
            if (bad++ < 5)
                printf("  t=%u back=%u\n", (unsigned)t, (unsigned)back);
        }
        if (last - t < 86408u) break;
    }
    snprintf(msg, sizeof(msg), "2 round trip over %u dates, %u wrong", n, bad);
    CHECK(bad == 0 && n > 49000, msg);
    printf("  2 round trip: %u dates, %u wrong\n", n, bad);
}

static void normalise(void)
{
    struct tm tm;
    time_t want;

    /* day 366 of 2024 (a leap year), as the DSLIST code sets it */
    tm = tm_zero;
    tm.tm_year = 124;
    tm.tm_mday = 366;
    timegm(&tm);
    CHECK(tm.tm_year == 124 && tm.tm_mon == 11 && tm.tm_mday == 31
          && tm.tm_yday == 365, "3 2024 day 366 is 31 December");

    /* day 60 of 2023 and 2024 */
    tm = tm_zero;
    tm.tm_year = 123;
    tm.tm_mday = 60;
    timegm(&tm);
    CHECK(tm.tm_mon == 2 && tm.tm_mday == 1, "3 2023 day 60 is 1 March");
    tm = tm_zero;
    tm.tm_year = 124;
    tm.tm_mday = 60;
    timegm(&tm);
    CHECK(tm.tm_mon == 1 && tm.tm_mday == 29, "3 2024 day 60 is 29 February");

    /* seconds since midnight, as the JES job times set it */
    tm = tm_zero;
    tm.tm_year = 126;
    tm.tm_mday = 292;
    tm.tm_sec  = 45296;                 /* 12:34:56 */
    want = timegm(&tm);
    CHECK(tm.tm_mon == 9 && tm.tm_mday == 19 && tm.tm_hour == 12
          && tm.tm_min == 34 && tm.tm_sec == 56,
          "3 2026 day 292 + 45296 s is 2026-10-19 12:34:56");
    CHECK((uint32_t)want == 1792411200u - 43200u + 45296u,
          "3 ... and its value");

    /* every field one past its range */
    settm(&tm, 2023, 12, 31, 23, 59, 60);
    timegm(&tm);
    CHECK(tm.tm_year == 124 && tm.tm_mon == 0 && tm.tm_mday == 1
          && tm.tm_hour == 0 && tm.tm_min == 0 && tm.tm_sec == 0,
          "3 2023-12-31 23:59:60 is 2024-01-01 00:00:00");
    settm(&tm, 2024, 3, 0, 24, 60, 0);
    timegm(&tm);
    CHECK(tm.tm_mon == 2 && tm.tm_mday == 1 && tm.tm_hour == 1
          && tm.tm_min == 0, "3 2024-03-00 24:60 is 2024-03-01 01:00");

    /* tm_wday, tm_yday, tm_isdst written back */
    settm(&tm, 2000, 1, 1, 0, 0, 0);
    tm.tm_wday = 99;
    tm.tm_yday = 99;
    tm.tm_isdst = 1;
    timegm(&tm);
    CHECK(tm.tm_wday == 6 && tm.tm_yday == 0 && tm.tm_isdst == -1,
          "3 2000-01-01 is a Saturday, day 0, isdst -1");
}

static void outofrange(void)
{
    static const int years[] = { 1969, 1900, 2106, 2200 };
    size_t i;
    char msg[96];

    for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
        struct tm tm, before;

        settm(&tm, years[i], 6, 15, 1, 2, 3);
        tm.tm_wday = 4;
        tm.tm_yday = 5;
        tm.tm_isdst = 7;
        before = tm;
        snprintf(msg, sizeof(msg), "4 year %d gives -1, tm unchanged",
                 years[i]);
        CHECK(timegm(&tm) == (time_t)-1 && memcmp(&tm, &before,
                                                  sizeof(tm)) == 0, msg);
    }
}

/* 1 when old_mktime() and timegm() disagree on one input */
static int differs(int year, int mon, int mday, int sec)
{
    struct tm t0, t1;
    uint32_t r0, r1;

    t0 = tm_zero;
    t0.tm_year = year;
    t0.tm_mon  = mon;
    t0.tm_mday = mday;
    t0.tm_sec  = sec;
    t0.tm_isdst = -1;
    t1 = t0;
    r0 = (uint32_t)old_mktime(&t0);
    r1 = (uint32_t)timegm(&t1);
    return r0 != r1 || memcmp(&t0, &t1, sizeof(t0)) != 0;
}

static void equivalence(void)
{
    static const int yr[]  = { 70, 71, 72, 99, 100, 124, 126, 138, 200, 205 };
    static const int mon[] = { 0, 1, 2, 11 };
    static const int day[] = { 1, 28, 29, 31, 60, 292, 365, 366 };
    static const int sec[] = { 0, 59, 3600, 45296, 86399, 86400 };
    unsigned a, b, i, n = 0, bad = 0;
    const unsigned nday = sizeof(day) / sizeof(day[0]);
    const unsigned nsec = sizeof(sec) / sizeof(sec[0]);
    char msg[96];

    for (a = 0; a < sizeof(yr) / sizeof(yr[0]); a++)
        for (b = 0; b < sizeof(mon) / sizeof(mon[0]); b++)
            for (i = 0; i < nday * nsec; i++) {
                bad += (unsigned)differs(yr[a], mon[b], day[i / nsec],
                                         sec[i % nsec]);
                n++;
            }
    snprintf(msg, sizeof(msg),
             "5 timegm() == old mktime() on %u inputs, %u differ",
             n, bad);
    CHECK(bad == 0, msg);
    printf("  5 equivalence: %u inputs, %u differ\n", n, bad);
}

int main(void)
{
    printf("=== tsttimegm: timegm() (#392) ===\n");
    fixed();
    roundtrip();
    normalise();
    outofrange();
    equivalence();
    printf("=== tsttimegm: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
