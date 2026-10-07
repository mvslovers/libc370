#import "../bookmaster/bookmaster.typ": *

= \<time.h\> — Date and Time <std-time>

#idx("time.h")
The header #cmd("<time.h>") declares the functions that read the clock and
convert between calendar time and broken-down time. It defines these types
and macros:

#tab(caption: [Types and macros of \<time.h\>])[
  #table(columns: (1.3in, 1fr),
    [Name], [Definition on this target],
    [#cmd("time_t")], [#cmd("unsigned long"), 4 bytes: seconds since
      1 January 1970, 00:00:00 UTC.],
    [#cmd("clock_t")], [#cmd("unsigned int"), 4 bytes.],
    [#cmd("CLOCKS_PER_SEC")], [1000],
    [#cmd("size_t")], [#cmd("unsigned long"), 4 bytes.],
    [#cmd("struct tm")], [the nine #cmd("int") members of C99, in this
      order: #cmd("tm_sec"), #cmd("tm_min"), #cmd("tm_hour"),
      #cmd("tm_mday"), #cmd("tm_mon"), #cmd("tm_year"), #cmd("tm_wday"),
      #cmd("tm_yday"), #cmd("tm_isdst")\; 36 bytes.],
    [#cmd("NULL")], [#cmd("((void *)0)")],
  )
] <std-time-types>

=== The Clock <std-time-clock-source>

#idx("TOD clock")#idx("time", "source of")
The calendar time comes from the System/370 time-of-day (TOD) clock, which
MVS keeps on Greenwich Mean Time. #cmd("time()") reads it with the
#cmd("STCK") instruction and converts it to seconds since 1970. There is no
processor-time clock: #cmd("clock()") reports that the processor time is not
available.

Because #cmd("time_t") is an unsigned 32-bit number, it can represent the
dates from 1970 to 7 February 2106. #cmd("time()") itself is more limited:
see its Notes. For dates outside that range, use the 64-bit time functions
of #cmd("<ext/time64.h>") (@ext).

=== The Time Zone <std-time-tz>

#idx("time zone")#idx("TZ environment variable")#idx("CVTTZ")
libc370 keeps one time-zone offset for each MVS task: the number of seconds
by which local time is ahead of UTC (negative west of Greenwich).
#cmd("localtime()"), #cmd("localtime_r()") and #cmd("ctime()") add it to the
calendar time. There are no daylight saving time rules: the offset stays the
same all year, and #cmd("tm_isdst") is never set to 0 or 1.

At program start-up the library calls #cmd("tzset()"), which takes the
offset from the environment variable #cmd("TZ") if it is set and otherwise
from the system's own time-zone value, #cmd("CVTTZ") in the communications
vector table. The format of #cmd("TZ") is described under #cmd("tzset()")
(@std-time-tzset)\; it is not the POSIX format. Environment variables are
described in @std-stdlib.

=== Static Buffers <std-time-static>

#cmd("gmtime()"), #cmd("localtime()") and #cmd("mktime()") return or use one
#cmd("struct tm") that belongs to the calling task, and #cmd("asctime()") and
#cmd("ctime()") one character buffer. Each call overwrites the previous
result of the same task\; the tasks of a program do not disturb each other.
#cmd("gmtime_r()") and #cmd("localtime_r()") use a structure that the caller
provides.

== asctime, ctime <std-time-asctime>

#idx("asctime")#idx("ctime")

=== Format

```
#include <time.h>

char *asctime(const struct tm *timeptr);
char *ctime(const time_t *timer);
```

=== Description

#cmd("asctime()") converts the broken-down time #var("timeptr") into a string
of the form

```
Sun Oct  4 14:05:09 2026\n
```

which is the form C99 prescribes: the abbreviated day and month names in
English, the day of the month in a field of three characters, the time with
two digits each, the year and a new-line character.

#cmd("ctime(timer)") is #cmd("asctime(localtime(timer))"): it converts a
calendar time to the same string in local time.

=== Returns

A pointer to the string, in a buffer of the task that the next call of
#cmd("asctime()") or #cmd("ctime()") overwrites.

=== Notes

The members #cmd("tm_wday") and #cmd("tm_mon") are used as indexes without
a check: they must lie in their ranges, 0 to 6 and 0 to 11.

=== Related

#cmd("strftime()") (@std-time-strftime), #cmd("localtime()")
(@std-time-localtime).

== clock <std-time-clock>

#idx("clock")#idx("processor time")

=== Format

```
#include <time.h>

clock_t clock(void);
```

=== Description

C99 #cmd("clock()") returns the processor time used by the program.

=== Returns

Always #cmd("(clock_t)-1"), the value C99 specifies when the processor time
is not available.

=== Notes

#cmd("CLOCKS_PER_SEC") is defined (1000), but no function uses it. To measure
elapsed time, call #cmd("time()") twice\; the resolution is one second.

=== Related

#cmd("time()") (@std-time-time).

== difftime <std-time-difftime>

#idx("difftime")

=== Format

```
#include <time.h>

double difftime(time_t time1, time_t time0);
```

=== Description

#cmd("difftime()") computes #var("time1") - #var("time0") in seconds.

=== Returns

The difference, as a #cmd("double").

=== Notes

The subtraction is done in #cmd("time_t"), which is unsigned, before the
result is converted. When #var("time1") is earlier than #var("time0") the
result is therefore not negative, but 4294967296 minus the true difference.
Pass the later time first, or compare the two values before you call
#cmd("difftime()").

=== Example

#code(read("../ex/std-time/elapsed.c"))

=== Related

#cmd("time()") (@std-time-time).

== gmtime, gmtime_r <std-time-gmtime>

#idx("gmtime")#idx("gmtime_r")#idx("UTC")

=== Format

```
#include <time.h>

struct tm *gmtime(const time_t *timer);
struct tm *gmtime_r(const time_t *timer, struct tm *p);
```

=== Description

#cmd("gmtime()") converts the calendar time #var("timer") into broken-down
time in UTC, in the #cmd("struct tm") of the calling task.
#cmd("gmtime_r()") stores the result in the structure #var("p") instead.

All members are set. #cmd("tm_year") counts from 1900, #cmd("tm_mon") from 0
(January), #cmd("tm_wday") from 0 (Sunday) and #cmd("tm_yday") from 0
(1 January).

=== Returns

#cmd("gmtime()") returns a pointer to the task's structure.
#cmd("gmtime_r()") returns #var("p").

=== Notes

- #cmd("tm_isdst") is set to -1, "not known", where C99 expects 0 for UTC.
- Every value of #cmd("time_t") converts correctly, up to
  7 February 2106, 06:28:15 UTC.
- #cmd("gmtime_r()") is a POSIX function. In the object module its name is
  #cmd("GMTIMER"), which the header assigns because external names on MVS are
  at most eight characters long.

=== Related

#cmd("localtime()") (@std-time-localtime), #cmd("mktime()")
(@std-time-mktime).

== localtime, localtime_r <std-time-localtime>

#idx("localtime")#idx("localtime_r")#idx("local time")

=== Format

```
#include <time.h>

struct tm *localtime(const time_t *timer);
struct tm *localtime_r(const time_t *timer, struct tm *p);
```

=== Description

#cmd("localtime()") converts the calendar time #var("timer") into broken-down
local time: it adds the time-zone offset of the task (see
@std-time-tz) and converts the sum as #cmd("gmtime()") does.
#cmd("localtime_r()") stores the result in #var("p").

=== Returns

#cmd("localtime()") returns a pointer to the task's structure, the same
structure #cmd("gmtime()") uses. #cmd("localtime_r()") returns #var("p").

=== Notes

- No daylight saving time is applied, and #cmd("tm_isdst") is -1.
- #cmd("localtime_r()") is a POSIX function. Its external name is
  #cmd("LOCALTMR").

=== Example

#code(read("../ex/std-time/now.c"))

=== Related

#cmd("gmtime()") (@std-time-gmtime), #cmd("tzset()") (@std-time-tzset).

== mktime <std-time-mktime>

#idx("mktime")

=== Format

```
#include <time.h>

time_t mktime(struct tm *timeptr);
```

=== Description

#cmd("mktime()") converts the broken-down time #var("timeptr") into a
calendar time. It uses #cmd("tm_year"), #cmd("tm_mon"), #cmd("tm_mday"),
#cmd("tm_hour"), #cmd("tm_min") and #cmd("tm_sec")\; #cmd("tm_wday"),
#cmd("tm_yday") and #cmd("tm_isdst") are ignored. Then it replaces the whole
structure with the broken-down form of the result, as #cmd("gmtime()")
produces it, which sets #cmd("tm_wday") and #cmd("tm_yday") and brings the
other members into their ranges.

=== Returns

The calendar time, or #cmd("(time_t)-1") if #cmd("tm_year") is less than 70
or greater than 205 (the years 1970 to 2105).

=== Notes

- *The fields are taken as UTC, not as local time.* C99 says that
  #cmd("mktime()") interprets them as local time\; libc370 applies no
  time-zone offset. #cmd("mktime()") is therefore the inverse of
  #cmd("gmtime()"), not of #cmd("localtime()"). To convert local time,
  subtract #cmd("__tzget()") from the result.
- Values of #cmd("tm_mday"), #cmd("tm_hour"), #cmd("tm_min") and
  #cmd("tm_sec") outside their ranges are carried by the arithmetic, so
  #cmd("tm_mday") = 32 in January gives 1 February. #cmd("tm_mon") must lie
  in the range 0 to 11.
- When the year is out of range, the structure is still replaced: it then
  holds the broken-down form of #cmd("(time_t)-1"), 7 February 2106.

=== Related

#cmd("gmtime()") (@std-time-gmtime), #cmd("__tzget()")
(@std-time-tzset).

== strftime <std-time-strftime>

#idx("strftime")#idx("date", "formatting")

=== Format

```
#include <time.h>

size_t strftime(char *s, size_t maxsize,
                const char *format, const struct tm *timeptr);
```

=== Description

#cmd("strftime()") formats the broken-down time #var("timeptr") into the
array #var("s") under control of the string #var("format"), writing at most
#var("maxsize") characters including the terminating null character.
Characters other than conversion specifications are copied unchanged. The
conversions are those of @std-time-strftime-tab, all in the #cmd("\"C\"")
locale (English names).

#tab(caption: [strftime() conversions])[
  #table(columns: (0.55in, 1fr),
    [Spec.], [Replaced by],
    [#cmd("%a")], [the abbreviated weekday name, #cmd("Sun") to #cmd("Sat")],
    [#cmd("%A")], [the full weekday name, #cmd("Sunday") to #cmd("Saturday")],
    [#cmd("%b")], [the abbreviated month name, #cmd("Jan") to #cmd("Dec")],
    [#cmd("%B")], [the full month name, #cmd("January") to #cmd("December")],
    [#cmd("%c")], [the date and time as #cmd("%a %b %d %H:%M:%S %Y"), for
      example #cmd("Sun Oct 04 14:05:09 2026")],
    [#cmd("%d")], [the day of the month, #cmd("01") to #cmd("31")],
    [#cmd("%H")], [the hour, 24-hour clock, #cmd("00") to #cmd("23")],
    [#cmd("%I")], [the hour, 12-hour clock, #cmd("01") to #cmd("12")],
    [#cmd("%j")], [the day of the year, #cmd("001") to #cmd("366")],
    [#cmd("%m")], [the month, #cmd("01") to #cmd("12")],
    [#cmd("%M")], [the minute, #cmd("00") to #cmd("59")],
    [#cmd("%p")], [#cmd("AM") or #cmd("PM")],
    [#cmd("%S")], [the second, #cmd("00") to #cmd("59")],
    [#cmd("%U")], [the week of the year, the first Sunday starting week 1,
      #cmd("00") to #cmd("53")\; see the Notes],
    [#cmd("%w")], [the weekday, #cmd("0") (Sunday) to #cmd("6")],
    [#cmd("%W")], [the week of the year, the first Monday starting week 1,
      #cmd("00") to #cmd("53")\; see the Notes],
    [#cmd("%x")], [*does not work*, see the Notes],
    [#cmd("%X")], [the time as #cmd("%H:%M:%S")],
    [#cmd("%y")], [the year without the century, #cmd("00") to #cmd("99")],
    [#cmd("%Y")], [the year with the century, four digits],
    [#cmd("%Z")], [*do not use*, see the Notes],
    [#cmd("%%")], [#cmd("%")],
  )
] <std-time-strftime-tab>

=== Returns

The number of characters placed into #var("s"), not counting the null
character, or 0 if the result, with its null character, does not fit into
#var("maxsize") characters. In that case #var("s") holds as much of the
result as fits, terminated by a null character.

=== Notes

- #cmd("%x") does not produce a date: it writes two numbers with an
  #cmd("s") after each, followed by the day and the year. Build the date
  from #cmd("%a %b %d %Y") instead.
- #cmd("%Z") reads a time-zone name that the library does not have. When
  #cmd("tm_isdst") is 0 it produces nothing\; when #cmd("tm_isdst") is not 0
  it reads through a null pointer and produces unpredictable characters.
  Every structure that #cmd("gmtime()"), #cmd("localtime()") and
  #cmd("mktime()") return has #cmd("tm_isdst") = -1.
- #cmd("%U") is one too small on every day of a year that begins on a
  Sunday, and #cmd("%W") on every day of a year that begins on a Monday: 1
  January 2023, a Sunday, gives #cmd("%U") #cmd("00") where C99 gives
  #cmd("01"). In other years both are correct. Compute the week as
  #cmd("(tm_yday + 7 - tm_wday) / 7") for #cmd("%U"), or with
  #cmd("(tm_wday + 6) % 7") in place of #cmd("tm_wday") for #cmd("%W").
- #cmd("%c") writes the day of the month with a leading zero, where C99's
  #cmd("\"C\"") locale uses a space.
- The C99 conversions #cmd("%C"), #cmd("%D"), #cmd("%e"), #cmd("%F"),
  #cmd("%g"), #cmd("%G"), #cmd("%h"), #cmd("%n"), #cmd("%r"), #cmd("%R"),
  #cmd("%t"), #cmd("%T"), #cmd("%u"), #cmd("%V") and #cmd("%z") and the
  modifiers #cmd("E") and #cmd("O") are not supported. An unsupported
  conversion is copied into the result unchanged, percent sign included.
- #var("maxsize") must be at least 1.
- The members used are not checked: #cmd("tm_wday") and #cmd("tm_mon") must
  lie in their ranges.

=== Example

See the example under #cmd("localtime()") (@std-time-localtime).

=== Related

#cmd("asctime()") (@std-time-asctime).

== time <std-time-time>

#idx("time")

=== Format

```
#include <time.h>

time_t time(time_t *timer);
```

=== Description

#cmd("time()") returns the current calendar time: the seconds elapsed since
1 January 1970, 00:00:00 UTC, by the TOD clock. If #var("timer") is not
#cmd("NULL"), the value is also stored there.

=== Returns

The calendar time.

=== Notes

- The TOD clock is read with #cmd("STCK"), so the value is UTC only if the
  clock of the system is set to Greenwich Mean Time, as MVS expects. On a
  system whose clock runs on local time, the result is off by the
  time-zone offset.
- The fraction of a second is truncated.
- The conversion divides the clock value with the 32-bit signed
  #cmd("DIVIDE") instruction, whose quotient cannot exceed 2147483647. From
  19 January 2038, about 02:54 UTC, onward the quotient does not fit, and
  #cmd("time()") ends in a fixed-point divide exception, abend #cmd("S0C9").
  Programs that must run beyond that date need the 64-bit time functions
  (@ext).

=== Related

#cmd("localtime()") (@std-time-localtime), #cmd("difftime()")
(@std-time-difftime).

== tzset, \_\_tzset, \_\_tzget <std-time-tzset>

#idx("tzset")#idx("__tzset")#idx("__tzget")#idx("time zone", "setting")

=== Format

```
#include <time.h>

void tzset(void);
int __tzset(int tzoffset);
int __tzget(void);
```

=== Description

#cmd("tzset()") sets the time-zone offset of the calling task. It reads the
environment variable #cmd("tz") and, if that is not set, #cmd("TZ"). The
value has the form

```
[-]hh[:mm[:ss]]
```

the hours, minutes and seconds by which local time is #emph[ahead] of UTC: a
#cmd("-") marks a zone west of Greenwich. #cmd("TZ=1") is Central European
Time, #cmd("TZ=-5") is Eastern Standard Time in North America, and
#cmd("TZ=5:30") is India.

If neither variable is set, #cmd("tzset()") takes the offset from
#cmd("CVTTZ"), the time-zone value of the system, converts it to seconds and
rounds it to a multiple of 10 seconds.

#cmd("__tzset()") sets the offset of the task directly, to #var("tzoffset")
seconds ahead of UTC. #cmd("__tzget()") returns the current offset.

=== Returns

#cmd("__tzset()") returns 0, or 1 if the task has no C run-time environment.
#cmd("__tzget()") returns the offset in seconds, or 0 if the task has no C
run-time environment.

=== Notes

- *The sign is the opposite of POSIX.* A POSIX #cmd("TZ") such as
  #cmd("EST5EDT") gives the hours #emph[behind] UTC and names the zone. libc370
  does not accept it: a value that does not begin with a digit or a minus
  sign is reported with three console messages, and the offset is taken
  from #cmd("CVTTZ"). A leading #cmd("+") is not accepted either.
- The library calls #cmd("tzset()") once at program start-up. Call it again
  after #cmd("setenv(\"TZ\", ...)") for the new value to take effect.
- The offset does not change with daylight saving time. A program that
  needs it sets the offset itself with #cmd("__tzset()").
- #cmd("__tzset()") and #cmd("__tzget()") are extensions of libc370.

=== Related

#cmd("localtime()") (@std-time-localtime)\; #cmd("getenv()") and
#cmd("setenv()") in @std-stdlib.
