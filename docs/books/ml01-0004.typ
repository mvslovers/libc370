#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "libc370 for MVS 3.8j",
  subtitle: "Library Reference",
  short-title: "libc370 Library Reference",
  number: "ML01-0004-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[First Edition (October 2026)]

    This edition applies to Version 2 Release 4 of libc370
    (libc370 2.4.1), as built with cc370 1.4.0, and to all subsequent
    releases and modifications until otherwise indicated in new editions.

    Comments on this book may be addressed to the issue tracker of
    the mvslovers/libc370 repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#contents(depth: 2)
#figures()

#heading(numbering: none)[About This Book] <about>

This book describes the functions, macros and types of libc370, the C
library for programs that run on MVS 3.8j and are built with the cc370
cross-toolchain. For each function it gives the header that declares it,
its prototype, what it does, what it returns and how it reports an error.

The library implements the C standard library, a set of interfaces from
POSIX, and a large group of extensions that give a C program access to MVS
services: data sets and DD statements, dynamic allocation, VSAM, the console,
tasks and synchronization, subsystems and JES2.

How to write, build and run a program with libc370 -- start-up, the
different kinds of files, the run-time environment -- is the subject of the
companion volume, the _libc370 Programmer's Guide_.

libc370 is built with, and only with, the cc370 toolchain. Every header
checks the compiler: compiled with a cc370 older than 1.4.0 it stops with
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
  [ML01-0001], [_cc370 User's Guide_],
  [ML01-0002], [_cc370 Command Reference_],
  [ML01-0003], [_libc370 Programmer's Guide_],
)

#mainmatter()
#set page(numbering: "1")

#include "ref/std-assert.typ"
#include "ref/std-ctype.typ"
#include "ref/std-errno.typ"
#include "ref/std-float.typ"
#include "ref/std-inttypes.typ"
#include "ref/std-iso646.typ"
#include "ref/std-limits.typ"
#include "ref/std-locale.typ"
#include "ref/std-math.typ"
#include "ref/std-setjmp.typ"
#include "ref/std-signal.typ"
#include "ref/std-stdarg.typ"
#include "ref/std-stdbool.typ"
#include "ref/std-stddef.typ"
#include "ref/std-stdint.typ"
#include "ref/std-stdio.typ"
#include "ref/std-stdlib.typ"
#include "ref/std-string.typ"
#include "ref/std-strings.typ"
#include "ref/std-time.typ"
#include "ref/std-unistd.typ"
#include "ref/std-wchar.typ"
#include "ref/sockets.typ"
#include "ref/mvs-datasets.typ"
#include "ref/mvs-dynalloc.typ"
#include "ref/mvs-osio.typ"
#include "ref/mvs-vsam.typ"
#include "ref/mvs-smf.typ"
#include "ref/mvs-program.typ"
#include "ref/mvs-sync.typ"
#include "ref/mvs-console.typ"
#include "ref/mvs-security.typ"
#include "ref/mvs-subsys.typ"
#include "ref/ext.typ"
#include "ref/s370.typ"

#show: appendices
#include "ref/apx-cblocks.typ"

#heading(numbering: none)[Index]
#make-index()
