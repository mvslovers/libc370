#import "../bookmaster/bookmaster.typ": *

= Porting Programs to MVS <pg-porting>

#idx("porting")
A C program written for a POSIX system usually compiles with cc370 after a
few changes, and then still has to be adapted to MVS: its names are too
long, its characters are EBCDIC, it has no processes to start and no
directories to open, and it runs in a 16-megabyte address space with a
fixed stack. This chapter goes through the obstacles in the order in which
they usually turn up, and says what to do about each. @pg-porting-tab
summarizes them.

#tab(caption: [Porting obstacles at a glance])[
  #table(columns: (1.25in, 1fr, 1.1in), align: left,
    [*Obstacle*], [*What to do*], [*See*],
    [Missing POSIX functions], [Compile with #cmd("-Werror") so that each
      one stops the compile\; replace it.], [@pg-porting-compile],
    [Names longer than 8], [Make internal functions #cmd("static")\; give
      the rest distinct names or #cmd("asm") labels.], [@pg-porting-names],
    [ASCII assumptions], [Use #cmd("<ctype.h>"), never code values or
      ranges\; translate data at the boundary.], [@pg-porting-ebcdic],
    [#cmd("fork()"), #cmd("exec()"), pipes], [Run programs with
      #cmd("system()") or #cmd("__link()"), work in parallel with
      threads.], [@pg-porting-process],
    [Paths and directories], [Use #cmd("DD:") names, data set names and
      members.], [@pg-porting-files],
    [Large memory], [Allocate less and fewer blocks\; mind 24-bit
      addresses.], [@pg-porting-storage],
    [Deep recursion, big locals], [Measure the frames, raise
      #cmd("__stklen"), or make it iterative.], [@pg-porting-stack],
    [Library differences], [Check #cmd("printf()"), #cmd("scanf()"),
      floating point and time zones.], [@pg-porting-lib],
  )
] <pg-porting-tab>

== Compile with Warnings as Errors <pg-porting-compile>

#idx("-Werror")#idx("implicit declaration")
Compile the ported sources with #cmd("-std=gnu99 -Wall -Werror"), as the
projects of the mvslovers ecosystem do. The reason is specific to porting:
a function that the library does not have is not an error by itself. The
header that would declare it is present -- #cmd("<unistd.h>") exists, but
declares only #cmd("sleep()") and #cmd("usleep()") -- so the call is
compiled as an implicit declaration, with a warning, and the failure comes
only at the link, as an unresolved external name:

#fig(caption: [A POSIX call compiles, and the link fails])[
  #code(read("../ex/pg-porting/posix.c"), numbers: true)
  #screen(raw(read("../ex/pg-porting/posix.txt")))
] <pg-porting-posix-fig>

With #cmd("-Werror") the compile stops at the first such call, in the
source, where it can be fixed. The link error names only the external
name, which for a library function is not always the C name
(@pg-porting-names).

The POSIX interfaces the library has are listed in @pg-intro-cover. For the
rest, the sections below say what MVS offers instead.

== Names <pg-porting-names>

#idx("external name", "collisions")
An external name has at most eight characters on MVS (@pg-asm-names).
cc370 cuts a longer C name to eight characters and changes the underscore
to #cmd("@"), so #cmd("record_count") and #cmd("record_check") both become
#cmd("RECORD@C"). Inside one source, cc370 warns and the assembly fails.
In two sources, the link takes the first definition, warns about the
second, and ends with return code 0:

#fig(caption: [Two functions and one external name])[
  #grid(columns: (1fr, 1fr), column-gutter: 0.2in,
    code(read("../ex/pg-porting/rec.c"), size: 7.5pt),
    code(read("../ex/pg-porting/count.c") + "\n" + read("../ex/pg-porting/check.c"), size: 7.5pt))
  #screen(raw(read("../ex/pg-porting/dup.txt")))
] <pg-porting-dup-fig>

Both calls in #cmd("rec.c") reach #cmd("record_count()"), so the program
returns 11 where 12 was meant. A warning that scrolls past in a large build is
easy to miss, so make the names safe before the first link:

+ *Make every function and variable that is used in only one source
  #cmd("static").* A static name never reaches the object module, and most
  of the names of a ported program are of this kind.
+ *For the remaining external names, compare the first eight characters.*
  Two names that agree there, ignoring case, need a change: rename one, or
  give one an #cmd("asm") label of eight characters of your choice in the
  header that declares it.
+ *Look out for the library's names.* A program that brings its own copy of
  a function the library also has -- #cmd("inet_addr()"), a #cmd("strdup()")
  of its own -- collides with it. Drop the copy and use the library's, or
  rename the copy. The library's external names are those of the
  #cmd("asm") labels in its headers, and of the C names folded as above.

#idx("link map", "finding a function")
The link map and abend output show the external name, not the C name. When
a name like #cmd("@@CTCRTE") turns up, search the library headers for it.

== EBCDIC <pg-porting-ebcdic>

#idx("EBCDIC", "porting")
Character constants and string literals are compiled to EBCDIC (code page
037), so #cmd("'A'") is #cmd("X'C1'") and #cmd("'\\n'") is #cmd("X'15'").
Code that names characters by their character constants works unchanged.
Code that assumes ASCII values does not, and nothing warns about it.
@pg-charset describes the character set in full\; when porting, look for
these:

- *Numeric character codes:* #cmd("0x41"), #cmd("65"), #cmd("0x0A") in
  place of #cmd("'A'") and #cmd("'\\n'"). Replace them with the character
  constant.
- *Letter ranges and arithmetic:* #cmd("c >= 'a' && c <= 'z'"),
  #cmd("c - 'a'"), #cmd("c | 0x20"), #cmd("c - 32"). The letters of EBCDIC
  are in three groups with other characters between them
  (@pg-charset-order). Use #cmd("islower()"), #cmd("toupper()") and the
  other functions of #cmd("<ctype.h>"). Digits are contiguous, so
  #cmd("c - '0'") is still right.
- *Sort order:* lowercase letters sort before uppercase, and both before
  digits. A program whose output order matters, or that searches a table
  sorted on another system, needs a look.
- *Data that crosses the boundary:* a network protocol, a file transferred
  in binary, a checksum computed over text. Translate between ASCII and
  EBCDIC where the data enters and leaves the program, never in the middle
  (@pg-charset-xlate).

== Processes <pg-porting-process>

#idx("fork", "not available")#idx("exec", "not available")
MVS has no #cmd("fork()") and no #cmd("exec()"), no pipes between
processes and no process identifiers in the POSIX sense. What a POSIX
program does with them is usually one of four things, and each has an MVS
form:

#deflist(width: 1.6in,
  [Run another program and wait], [#cmd("system(\"PGM PARM\")") runs a
    load module as a subtask and returns its completion code. There is no
    shell: the string is a program name and a parameter. Do not call
    #cmd("system(NULL)"), which the library does not check for.
    #cmd("__link()") and #cmd("__linkt()") in #cmd("<mvs/link.h>") call a
    program with #cmd("LINK"), on the same task.],
  [Work in parallel], [Threads, which are MVS subtasks (@pg-tasks). They
    share the address space, so the isolation that #cmd("fork()") gives is
    not there\; share data deliberately, under a lock.],
  [Pass data to another program], [A data set, or a temporary data set
    (#cmd("&&")#var("name"), or #cmd("tmpfile()")) written by one step and
    read by the next. Within one address space, storage passed by
    pointer.],
  [Run as a daemon], [A started task: a procedure started from the console,
    which receives #cmd("MODIFY") and #cmd("STOP") commands
    (@pg-services).],
)

*Signals* are not a means of communication between programs on MVS.
#cmd("signal()") and #cmd("raise()") work within the program, and the
handlers are shared by all its tasks. An abend is not a signal: recovery
from one is the business of #cmd("try()") (@pg-errors).

== Files <pg-porting-files>

#idx("file name", "porting")
There are no directories and no paths. A file is a data set with a name of
up to 44 characters in qualifiers of up to eight, a member of a
partitioned data set, or whatever a DD statement of the job step stands
for. @pg-porting-files-tab shows how the usual POSIX uses map.

#tab(caption: [POSIX file uses on MVS])[
  #table(columns: (1.6in, 1fr), align: left,
    [*POSIX*], [*MVS*],
    [a configuration file such as #cmd("/etc/app.conf")], [#cmd("DD:CONFIG"):
      the job decides which data set\; or settings in the #cmd("SYSENV")
      data set read as environment variables (@pg-startup-env)],
    [a file named on the command line], [the name as given: a data set
      name in apostrophes, or #cmd("DD:")#var("ddname") (@pg-io-names-tab)],
    [a directory of files], [a partitioned data set and its members\; list
      them with #cmd("__walkpd()") (@pg-services)],
    [#cmd("/tmp")], [a temporary data set: #cmd("&&")#var("name") or
      #cmd("tmpfile()")],
    [standard output and error], [#cmd("SYSPRINT") and #cmd("SYSTERM"), or
      the terminal under TSO (@pg-io-std)],
    [#cmd("open()"), #cmd("read()"), file descriptors], [the streams of
      #cmd("<stdio.h>"), or record I/O (@pg-io-beyond)],
  )
] <pg-porting-files-tab>

Three properties of data sets catch ported code:

- *Data sets hold records, not bytes.* A text stream turns records into
  lines, and a line longer than the record length of the output data set
  continues in the next record. Positioning works differently from a byte
  stream.
  @pg-io-records describes the rules.
- *Names are case-insensitive.* #cmd("fopen()") folds a name to uppercase.
- *A file opened for writing is held.* A data set opened for output is
  allocated #cmd("DISP=OLD") and is not available to other jobs until it
  is closed.

Prefer #cmd("DD:") names for anything a batch program reads or writes: they
leave the choice of data set, and its disposition, to the JCL.

== Storage and Addresses <pg-porting-storage>

#idx("24-bit addressing", "porting")
All the storage of a program -- its code, its stack, its heap, its buffers
-- lies below 16 MB, in the region of the job step (@pg-storage-space).
A program written for a machine with gigabytes may simply not fit:

- *Allocate less.* Read a file a record at a time rather than into
  storage, keep tables bounded, and free what is no longer needed.
- *Allocate fewer blocks.* Each #cmd("malloc()") is a #cmd("GETMAIN") of
  its own, rounded up to 64 bytes together with an 8-byte prefix, so a list
  of a million 16-byte nodes takes 64 MB, not 16 (@pg-storage-heap).
  Allocate small objects in arrays.
- *Check every allocation.* #cmd("malloc()") returns #cmd("NULL") when the
  region is exhausted, and on MVS that happens.

The types are 32 bits: #cmd("int"), #cmd("long"), #cmd("size_t") and
pointers. Only the low 24 bits of an address are significant. Do not keep
flags in the high-order byte of a pointer: MVS uses that bit for its own
purposes, as the last-entry flag of a parameter list (@pg-asm-oslink).

#idx("long long")
#cmd("long long") and #cmd("uint64_t") are 64 bits and fully supported,
including division and #cmd("printf(\"%lld\")"). The division is done by a
helper routine of the compiler, which the cc370 driver links
automatically\; a build that runs ld370 itself adds #cmd("-lcc370rt").

== Stack Depth <pg-porting-stack>

#idx("stack", "porting")#idx("recursion")
The C stack has a fixed size -- 256 KB for a program, 64 KB for a thread --
and nothing checks it: a program that needs more overwrites the storage
behind the stack and fails somewhere else (@pg-startup-stack). Programs
from systems with a stack of many megabytes need checking:

- *Large automatic arrays.* A #cmd("char buf[65536]") in a function takes
  a quarter of the stack. Allocate it with #cmd("malloc()").
- *Recursion.* A recursive descent parser, a tree walk, a recursive sort:
  estimate the depth for the largest input and multiply by the frame size.
  Where the depth depends on the input, change the algorithm to a loop with
  an explicit stack on the heap.
- *Measure.* Compile with #cmd("-S") and read #cmd("FRAME=") on the
  #cmd("PDPPRLG") of each function (@pg-startup-stack).

If the program needs more and is otherwise sound, give it a larger stack
with #cmd("__stklen"), and its threads with #cmd("cthread_create_ex()").
@pg-storage-stack describes how to use less.

== Library Differences <pg-porting-lib>

#idx("printf", "differences")#idx("scanf", "field width")
The library follows C99, with differences that a ported program can walk
into. The ones that most often matter:

- *#cmd("scanf()") has no field width.* A digit after the #cmd("%") is not
  a width: the conversion fails, and with it the rest of the format. A
  format such as #cmd("\"%2d%2d\"") must be replaced, for example by
  #cmd("strtol()") on the fields.
- *#cmd("printf()")* ignores #cmd("h") and #cmd("hh") on the integer
  conversions, so a value is printed as the #cmd("int") it was passed as,
  and does not implement #cmd("%a"): the conversion is printed as written.
- *Floating point is hexadecimal* (HFP), not IEEE: a #cmd("float") holds
  21 to 24 bits of precision, a #cmd("double") 53 to 56, both reach about
  #cmd("7.2E+75"), and there is no infinity and no NaN. Results that are
  compared with those of another system to the last digit will differ.
- *Time zone.* #cmd("TZ") has a format of its own, not the POSIX one\;
  without it the system's offset is used.
- *Environment variables* come from the #cmd("SYSENV") data set
  (@pg-startup-env), not from a parent process.
- *The exit status is a return code.* JCL tests values from 0 to 4095, and
  by convention 0 is success, 4 a warning, 8 an error, 12 and 16 severe.
- *#cmd("errno") values* are the library's own, and a function that fails
  in an MVS service may return the service's code. Check each function's
  description.

Each function's entry in the _LIBC/370 Library Reference_ lists its
deviations under _Notes_. Read them for the functions a ported program
relies on most.
