#import "../bookmaster/bookmaster.typ": *

// Narrow cells: ragged right.
#show table: set par(justify: false)

= Introducing libc370 <pg-intro>

#idx("libc370")
libc370 is the C library for programs that run on MVS 3.8j. It is the
run-time library of the cc370 cross-toolchain: a program is compiled and
linked on a workstation with cc370, and the load module that results
carries, linked into it, every part of libc370 that it uses. Nothing of the
library is installed on MVS.

The library gives a C program three things: the C standard library, a set
of interfaces from POSIX, and access to the services of MVS -- data sets
and DD statements, dynamic allocation, the console, tasks, recovery from
abends, JES2 and more. It is reentrant: it keeps its state in storage that
it obtains for each program, not in the load module, so a program built with
it can be reentrant too (@pg-rent).

This chapter describes what the library covers, how it relates to the
version of cc370, where it is installed, and how a program finds its
headers and its functions.

== What the Library Covers <pg-intro-cover>

=== The C Standard Library

#idx("C99", "library")#idx("standard library")
libc370 implements the library of the C standard as cc370 compiles it,
C99 in the GNU dialect, with these headers:

#tab(caption: [Standard headers])[
  #table(columns: (1.6in, 1fr),
    [Header], [Notes],
    [#cmd("<assert.h>"), #cmd("<ctype.h>"), #cmd("<errno.h>"),
      #cmd("<float.h>"), #cmd("<iso646.h>"), #cmd("<limits.h>"),
      #cmd("<locale.h>"), #cmd("<setjmp.h>"), #cmd("<signal.h>"),
      #cmd("<stdarg.h>"), #cmd("<stdbool.h>"), #cmd("<stddef.h>"),
      #cmd("<stdint.h>"), #cmd("<inttypes.h>"), #cmd("<string.h>"),
      #cmd("<time.h>")], [Complete, with the restrictions that MVS imposes:
      only the C locale, signals only from #cmd("raise()")
      (@pg-errors). #cmd("<inttypes.h>") lacks #cmd("wcstoimax()") and
      #cmd("wcstoumax()"), for want of the wide-character functions.],
    [#cmd("<stdio.h>")], [Complete. Files are data sets, members and SYSOUT,
      reached through DD statements (@pg-io).],
    [#cmd("<stdlib.h>")], [Complete. #cmd("EXIT_FAILURE") is 12, the MVS
      convention.],
    [#cmd("<math.h>")], [The 22 functions of C89. The functions that C99
      added, such as #cmd("round()") and the #cmd("float") variants, are
      not provided.],
    [#cmd("<wchar.h>")], [The types and limits only. Wide characters are
      handled with the multibyte functions of #cmd("<stdlib.h>").],
  )
] <pg-intro-std-tab>

#cmd("<complex.h>"), #cmd("<fenv.h>"), #cmd("<tgmath.h>") and
#cmd("<wctype.h>") do not exist. Floating point is the hexadecimal floating
point of System/370, which has no infinity, no NaN and no rounding modes for
#cmd("<fenv.h>") to control.

#idx("long long")
#cmd("long long") is a 64-bit type. System/370 has no 64-bit arithmetic,
so cc370 calls routines of its own for multiplication, division and
conversions; they are in #cmd("libcc370rt.a"), which belongs to cc370 and
is linked together with libc370.

=== Interfaces from POSIX

#idx("POSIX")
MVS is not a POSIX system, and libc370 takes from POSIX only what has a
meaning on MVS:

- #cmd("<strings.h>"): #cmd("strcasecmp()") and its relatives.
- #cmd("<unistd.h>"): #cmd("sleep()") and #cmd("usleep()"), and nothing
  else. There is no #cmd("fork()"), no #cmd("exec()"), and there are no
  file descriptors below the C streams.
- #cmd("setenv()"), #cmd("putenv()") and #cmd("unsetenv()") in
  #cmd("<stdlib.h>"), #cmd("strdup()") in #cmd("<string.h>"), and the
  #cmd("gmtime_r()") and #cmd("localtime_r()") in #cmd("<time.h>").
- Sockets: #cmd("<sys/socket.h>"), #cmd("<sys/select.h>"),
  #cmd("<netinet/in.h>"), #cmd("<arpa/inet.h>") and #cmd("<netdb.h>"), for
  TCP/IP over IPv4 (@pg-sockets).

=== MVS Extensions

#idx("MVS extensions")
The interfaces to MVS are in subdirectories of the header directory, so
that their names cannot collide with those of other programs:

#tab(caption: [Header directories])[
  #table(columns: (0.9in, 1fr),
    [Directory], [Contents],
    [#cmd("mvs/")], [The services of MVS: data sets, DD statements and
      catalogs (#cmd("dd.h"), #cmd("dslist.h"), #cmd("pds.h")), dynamic
      allocation and IDCAMS (#cmd("dynalloc.h"), #cmd("idcams.h")), record
      and block I/O (#cmd("rfile.h"), #cmd("osio.h")), VSAM (#cmd("vsam.h")),
      the run-time anchors and environment (#cmd("crt.h"), #cmd("env.h")),
      LINK and LOAD (#cmd("link.h")), storage (#cmd("storage.h")), recovery
      (#cmd("recovery.h")), authorization (#cmd("apf.h")), the console
      (#cmd("console.h"), #cmd("wto.h")), tasks, locks and timers
      (#cmd("thread.h"), #cmd("lock.h"), #cmd("ecb.h"), #cmd("timer.h")),
      security (#cmd("racf.h")), SMF (#cmd("smf.h")), subsystems, JES2 and
      ISPF (#cmd("subsys.h"), #cmd("jes2.h"), #cmd("ispf.h")) and TSO
      (#cmd("tso.h")).],
    [#cmd("ext/")], [Extensions that are not tied to MVS: dynamic arrays,
      64-bit integer arithmetic as a library type, 64-bit time, string
      helpers, and #cmd("libc370_version()").],
    [#cmd("s370/")], [The machine: atomic updates with
      #cmd("COMPARE AND SWAP"), channel command words, save areas.],
    [#cmd("ibm/")], [C mappings of MVS and JES2 control blocks, such as the
      CVT, the TCB, the DCB and the JFCB.],
  )
] <pg-intro-dirs-tab>

The _libc370 Library Reference_ describes every function of every header.
This book shows how they are used together: @pg-services for the MVS
services, @pg-tasks for multitasking, @pg-tso for TSO.

== libc370 and cc370 <pg-intro-cc370>

#idx("cc370", "version required")#idx("__CC370__")
libc370 is built with cc370 and only with cc370, and the two are released
as a pair. cc370 provides the compiler, the routines the compiler calls
(#cmd("libcc370rt.a")) and the assembler macros of the function prologue
and epilogue; libc370 provides the headers, #cmd("libc.a"), the start-up
modules and the other macros. Each depends on the other's version:

- This release of libc370 needs cc370 1.1.0 or later, below 2.0.
- cc370 1.1.0 and later need libc370 2.1.0 or later.

The library checks the first rule itself. Every libc370 header includes
#cmd("<sys/_cc370.h>"), which compares the macro #cmd("__CC370__") that cc370
predefines -- the version as #var("major")#cmd(" * 10000 + ")#var("minor")#cmd(" * 100 + ")#var("patch"),
10100 for 1.1.0 -- with the minimum, and stops the compilation of a program
built with an older compiler:

```
#error "libc370 needs cc370 1.1.0 or later"
```

The installation procedures of the toolchain install a matching pair; the
_cc370 User's Guide_, Chapter 2, “Installing the Toolchain”, describes them.

== Where the Library Is Installed <pg-intro-install>

#idx("sysroot")#idx("libc.a")
The library is installed with the toolchain, in a directory #cmd("cc370")
under the installation prefix -- #cmd("$HOME/.local") unless another prefix
was chosen:

#deflist(width: 1.4in,
  [#cmd("cc370/include")], [the headers, with the subdirectories of
    @pg-intro-dirs-tab.],
  [#cmd("cc370/lib")], [#cmd("libc.a"), the library itself, which also holds
    the start-up routine #cmd("@@CRT0")\; the start-up objects
    #cmd("crt0.o"), #cmd("crt1.o") (two copies of #cmd("@@CRT0")) and
    #cmd("crtm.o")\; and cc370's #cmd("libcc370rt.a").],
  [#cmd("cc370/macros")], [the assembler macros: libc370's own and the MVS
    system macros that the library needs, for programs that include
    assembler source (@pg-asm).],
)

#idx("-I option", "and libc370 headers")
A program needs no option to find any of this. The compiler searches
#cmd("cc370/include") for #cmd("#include <...>") after the directories
given with #cmd("-I"), and the driver names the start-up module and the
libraries itself when it links. #cmd("-print-file-name") shows where a file
is: in @pg-intro-build-fig, #cmd("$PREFIX/bin/../lib/cc370/1.2.0/../../..")
is the prefix itself, so the library is #cmd("$PREFIX/cc370/lib/libc.a").

#fig(caption: [Building a program and finding the library (the
  installation prefix is shown as \$PREFIX)])[
  #screen(raw(read("../ex/pg-intro/build.txt")))
] <pg-intro-build-fig>

@pg-intro-link-fig shows the link step of the same build: the start-up
module #cmd("crt0.o") first, then the program, then the libraries
#cmd("-lcc370rt -lc -lcc370rt").

#fig(caption: [The link step (the prefix is shown as \$PREFIX, the temporary
  directory as \$TMP, and the long line is broken)])[
  #screen(raw(read("../ex/pg-intro/link.txt")))
] <pg-intro-link-fig>

== How a Program Finds Its Functions <pg-intro-autocall>

#idx("autocall")#idx("ar370")
#cmd("libc.a") is an archive of object modules with an index of the external
names each one defines. When ld370 links a program, it looks up every name
that the program refers to and does not define, and takes from the archive
the object module that defines it -- and then the ones that module needs,
and so on. This is the automatic library call of the MVS linkage editor,
done on the workstation.

The unit that ld370 takes is the whole object module. libc370 therefore
puts nearly every function in a source file of its own, so that a program
that calls #cmd("strlen()") gets #cmd("strlen()") and not a group of
string functions with it. A program that is small on MVS follows the same
rule in its own libraries.

Even so, a C program carries a run-time environment. The program of
@pg-intro-hello-fig is a load module of 71,312 bytes, nearly all of it the
library: the start-up, the standard streams, the data set I/O beneath them
and the formatted output of #cmd("printf()"). Each further function adds
only what it needs.

#fig(caption: [A first program])[
  #code(read("../ex/pg-intro/hello.c"), numbers: true)
] <pg-intro-hello-fig>

#idx("libc370_version")
The program writes the version of the library it was linked with. A load
module keeps the library it was linked with, whatever is installed later, so
a program that writes this line when it starts records in its output which
library it runs. @pg-intro-hello-jcl runs it; the _cc370 User's Guide_,
Chapter 7, “Getting Programs onto MVS”, describes how the load module
reaches #cmd("MYUSER.LOADLIB").

#fig(caption: [Running HELLO])[
  #code(read("../ex/pg-intro/hello.jcl"))
] <pg-intro-hello-jcl>

#fig(caption: [Output of HELLO])[_Output to be captured on MVS._]
<pg-intro-hello-out>

=== External Names Have Eight Characters <pg-intro-names>

#idx("external name")#idx("name", "external, eight characters")
An object module of MVS records the name of an external symbol in eight
characters. The compiler forms the external name of a C function or variable
from its C name: it changes each underscore to #cmd("@"), puts the name in
uppercase and cuts it to eight characters. #cmd("libc370_version") becomes
#cmd("LIBC370@"), and #cmd("printf") becomes #cmd("PRINTF"). A header can
also give a function an external name of its own with #cmd("asm"):

```
CTHDTASK *cthread_create(void *func, void *arg1, void *arg2) asm("@@CTCRTE");
```

The libc370 headers do this for many of their functions. The external
name is the one that a link map, a message about an unresolved reference
and an abend report show; to find the C function behind such a name,
search the headers for it.

#idx("doubly defined name")
In a program of your own, two external names that agree in their first
eight characters are the same name on MVS. Within one source file the
compiler warns and the assembly fails. In different source files nothing
stops the build: ld370 keeps the first definition, warns, and every call of
the second function reaches the first, as @pg-intro-dup-fig shows for the
functions #cmd("process_input") and #cmd("process_output").

#fig(caption: [Two functions with the same external name (the temporary
  directory is shown as \$TMP, and the long line is broken)])[
  #screen(raw(read("../ex/pg-intro/dupname.txt")))
] <pg-intro-dup-fig>

Treat this warning as an error. To avoid the collision:

+ Make every function that is not called from another source file
  #cmd("static"). A static function has an internal name that the linker
  never sees.
+ Give an external function a name whose first eight characters are
  unique, or
+ give it an external name of its own with #cmd("asm"), in the header that
  declares it, as in @pg-intro-names-fig.

#fig(caption: [External names chosen with asm])[
  #code(read("../ex/pg-intro/names.c"), numbers: true)
] <pg-intro-names-fig>
