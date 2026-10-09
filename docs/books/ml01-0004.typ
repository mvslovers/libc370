#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "LIBC/370 for MVS 3.8j",
  subtitle: "Library Reference",
  short-title: "LIBC/370 Library Reference",
  product: "LIBC/370",
  number: "ML01-0004-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[First Edition (October 2026)]

    This edition applies to Version 2 Release 6 of LIBC/370
    (libc370 2.6.3), as built with CC/370 1.4.0, and to all subsequent
    releases and modifications until otherwise indicated in new editions.

    Comments on this book may be addressed to the issue tracker of
    the mvslovers/libc370 repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#part("index.html", title: [LIBC/370 Library Reference])[
#titlepage()
#contents(depth: 2)
#figures()

#heading(numbering: none)[About This Book] <about>

This book describes the functions, macros and types of LIBC/370 (libc370), the C
library for programs that run on MVS 3.8j and are built with the CC/370 (cc370)
cross-toolchain. For each function it gives the header that declares it,
its prototype, what it does, what it returns and how it reports an error.

The library implements the C standard library, a set of interfaces from
POSIX, and a large group of extensions that give a C program access to MVS
services: data sets and DD statements, dynamic allocation, VSAM, the console,
tasks and synchronization, subsystems and JES2.

How to write, build and run a program with LIBC/370 -- start-up, the
different kinds of files, the run-time environment -- is the subject of the
companion volume, the _LIBC/370 Programmer's Guide_.

LIBC/370 is built with, and only with, the CC/370 toolchain. Every header
checks the compiler: compiled with a CC/370 older than 1.4.0 it stops with
#cmd("#error \"libc370 needs cc370 1.4.0 or later\"").

== Who Should Use This Book

This book is for programmers who write C programs for MVS 3.8j. It assumes
that you know the C language and the basic concepts of MVS. It does not
teach C: where a function behaves as the C standard prescribes, the entry
says so briefly and concentrates on what is particular to MVS.

== How This Book Is Organized

The chapters follow the headers. The standard and POSIX headers come first,
in alphabetical order, then sockets, then the MVS extensions grouped by
subject, then the remaining extensions. Appendix A lists the mappings of MVS
control blocks that the library provides as C structures.

== How to Read a Function Entry

Each function is described in a section of its own, in this order:

#deflist(width: 1.35in,
  [Format], [the #cmd("#include") directive and the prototype, as the
    header declares it.],
  [Description], [what the function does.],
  [Returns], [the value returned, for success and for failure.],
  [Errors], [the values the function stores in #cmd("errno"), where it sets
    it.],
  [Notes], [behavior particular to MVS or to this library, and
    restrictions.],
  [Example], [a short program or fragment, where one helps.],
  [Related], [functions to compare.],
)

A part that has nothing to say is left out.

== Related Publications

#deflist(width: 1.35in,
  [ML01-0001], [_CC/370 User's Guide_],
  [ML01-0002], [_CC/370 Command Reference_],
  [ML01-0003], [_LIBC/370 Programmer's Guide_],
)

#mainmatter()
]
#set page(numbering: "1")

#part("std-assert.html", include "ref/std-assert.typ")
#part("std-ctype.html", include "ref/std-ctype.typ")
#part("std-errno.html", include "ref/std-errno.typ")
#part("std-float.html", include "ref/std-float.typ")
#part("std-inttypes.html", include "ref/std-inttypes.typ")
#part("std-iso646.html", include "ref/std-iso646.typ")
#part("std-limits.html", include "ref/std-limits.typ")
#part("std-locale.html", include "ref/std-locale.typ")
#part("std-math.html", include "ref/std-math.typ")
#part("std-setjmp.html", include "ref/std-setjmp.typ")
#part("std-signal.html", include "ref/std-signal.typ")
#part("std-stdarg.html", include "ref/std-stdarg.typ")
#part("std-stdbool.html", include "ref/std-stdbool.typ")
#part("std-stddef.html", include "ref/std-stddef.typ")
#part("std-stdint.html", include "ref/std-stdint.typ")
#part("std-stdio.html", include "ref/std-stdio.typ")
#part("std-stdlib.html", include "ref/std-stdlib.typ")
#part("std-string.html", include "ref/std-string.typ")
#part("std-strings.html", include "ref/std-strings.typ")
#part("std-time.html", include "ref/std-time.typ")
#part("std-unistd.html", include "ref/std-unistd.typ")
#part("std-wchar.html", include "ref/std-wchar.typ")
#part("sockets.html", include "ref/sockets.typ")
#part("mvs-datasets.html", include "ref/mvs-datasets.typ")
#part("mvs-dynalloc.html", include "ref/mvs-dynalloc.typ")
#part("mvs-osio.html", include "ref/mvs-osio.typ")
#part("mvs-vsam.html", include "ref/mvs-vsam.typ")
#part("mvs-smf.html", include "ref/mvs-smf.typ")
#part("mvs-program.html", include "ref/mvs-program.typ")
#part("mvs-sync.html", include "ref/mvs-sync.typ")
#part("mvs-console.html", include "ref/mvs-console.typ")
#part("mvs-security.html", include "ref/mvs-security.typ")
#part("mvs-subsys.html", include "ref/mvs-subsys.typ")
#part("ext.html", include "ref/ext.typ")
#part("s370.html", include "ref/s370.typ")

#show: appendices
#part("apx-cblocks.html", include "ref/apx-cblocks.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
