#import "../bookmaster/bookmaster.typ": *

= \<unistd.h\> — System Interfaces <std-unistd>

#idx("unistd.h")
On a POSIX system #cmd("<unistd.h>") declares the process, file-descriptor
and directory interfaces. MVS has none of them in that form: there is no
#cmd("fork()") or #cmd("exec()"), no file descriptors below the C streams, no
current directory and no process identifier in the POSIX sense. LIBC/370's
#cmd("<unistd.h>") therefore declares only the two functions that have an MVS
equivalent, #cmd("sleep()") and #cmd("usleep()"). The header defines no
macros or types.

A program that uses other names from #cmd("<unistd.h>"), such as
#cmd("getpid()"), #cmd("read()"), #cmd("close()") or #cmd("access()"),
compiles: the functions are not declared, and cc370, like the GCC it is
based on, declares them implicitly, with a warning under #cmd("-Wall") (an error only under
#cmd("-Werror")). The link then fails, because the library does not define
them: ld370 reports them as unresolved external references. Files are read and written through the streams of
@std-stdio\; data sets and DD statements are described in @mvs-datasets.

== sleep, usleep <std-unistd-sleep>

#idx("sleep")#idx("usleep")#idx("wait", "for a time interval")

=== Format

```
#include <unistd.h>

int sleep(unsigned seconds);
int usleep(unsigned usec);
```

=== Description

#cmd("sleep()") suspends the calling task for #var("seconds") seconds. It
waits on an event control block that a timer posts when the interval has
elapsed\; the interval is set in hundredths of a second. When #var("seconds")
is 0, #cmd("sleep()") returns at once.

#cmd("usleep()") suspends the calling task for about #var("usec")
microseconds. It issues #cmd("STIMER WAIT") with the interval in timer units
of 26.04 microseconds: #var("usec") is divided by 26, and an interval of less
than one unit is raised to one unit.

Only the calling task waits\; other tasks of the address space keep running.

=== Returns

Both functions always return 0.

=== Notes

- Nothing interrupts the wait: there are no signals that end it early, so
  #cmd("sleep()") never returns the unslept time that POSIX specifies.
- If the timer for #cmd("sleep()") cannot be set, which happens when the
  system is short of storage, #cmd("sleep()") returns at once without
  waiting, still with 0. The library writes one console message per task
  the first time this happens.
- The #cmd("usleep()") interval is converted to timer units of 26.04
  microseconds by dividing by 26, truncated. A short wait can therefore be
  up to 26 microseconds shorter than requested, and a long one comes out
  longer: #cmd("usleep(1000000)") waits about 1.0016 seconds.
  The actual end of the wait also depends on the dispatching of the task.
- A value of #var("seconds") greater than 42,949,672 (about 497 days)
  overflows the interval in hundredths of a second.
- To wait for an event #emph[or] a time-out, use the timed waits on event
  control blocks described in @mvs-sync.

=== Example

#code(read("../ex/std-unistd/sleep.c"))

=== Related

#cmd("time()") in @std-time\; the timed event waits in @mvs-sync.
