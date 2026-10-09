#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "LIBC/370 for MVS 3.8j",
  subtitle: "Programmer's Guide",
  short-title: "LIBC/370 Programmer's Guide",
  number: "ML01-0003-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[First Edition (October 2026)]

    This edition applies to Version 2 Release 6 of LIBC/370
    (libc370 2.6.3), as built with CC/370 1.4.0, and to all subsequent
    releases and modifications until otherwise indicated in new editions.

    *Draft.* The output of the examples that run on MVS has still to be
    captured; those figures are marked as such.

    Comments on this book may be addressed to the issue tracker of
    the mvslovers/libc370 repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#part("index.html", title: [LIBC/370 Programmer's Guide])[
#titlepage()
#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book explains how to write C programs for MVS 3.8j with LIBC/370 (libc370): how a
program starts and ends, how it reads and writes data sets, what the
EBCDIC character set and the 24-bit address space mean for C code, and how
a program uses MVS services such as dynamic allocation, the console,
multitasking and TCP/IP.

Each function is described in full in the companion volume, the _LIBC/370
Library Reference_\; this book shows how the functions are used together.

LIBC/370 is built with, and only with, the CC/370 (cc370) toolchain. Every header
checks the compiler: compiled with a CC/370 older than 1.4.0 it stops with
#cmd("#error \"libc370 needs cc370 1.4.0 or later\"").

== Who Should Use This Book

This book is for programmers who write C programs that run on MVS 3.8j. It
assumes that you know the C language and the basic concepts of MVS:
data sets, DD statements, job steps and load modules.

== How This Book Is Organized

#deflist(width: 1.35in,
  [Chapter 1], [“Introducing LIBC/370”.],
  [Chapter 2], [“Program Structure and Start-Up”.],
  [Chapter 3], [“Files and I/O”.],
  [Chapter 4], [“Character Sets”.],
  [Chapter 5], [“Storage”.],
  [Chapter 6], [“Errors and Recovery”.],
  [Chapter 7], [“Reentrant and Reusable Programs”.],
  [Chapter 8], [“Multitasking”.],
  [Chapter 9], [“Using MVS Services”.],
  [Chapter 10], [“TCP/IP Sockets”.],
  [Chapter 11], [“Programs under TSO and ISPF”.],
  [Chapter 12], [“Calling Between C and Assembler”.],
  [Chapter 13], [“Porting Programs to MVS”.],
  [Appendix A], [“Migrating from crent370 and LIBC/370 1.x”.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML01-0001], [_CC/370 User's Guide_],
  [ML01-0002], [_CC/370 Command Reference_],
  [ML01-0004], [_LIBC/370 Library Reference_],
)

#mainmatter()
]
#set page(numbering: "1")

#part("pg-intro.html", include "guide/pg-intro.typ")
#part("pg-startup.html", include "guide/pg-startup.typ")
#part("pg-io.html", include "guide/pg-io.typ")
#part("pg-charset.html", include "guide/pg-charset.typ")
#part("pg-storage.html", include "guide/pg-storage.typ")
#part("pg-errors.html", include "guide/pg-errors.typ")
#part("pg-rent.html", include "guide/pg-rent.typ")
#part("pg-tasks.html", include "guide/pg-tasks.typ")
#part("pg-services.html", include "guide/pg-services.typ")
#part("pg-sockets.html", include "guide/pg-sockets.typ")
#part("pg-tso.html", include "guide/pg-tso.typ")
#part("pg-asm.html", include "guide/pg-asm.typ")
#part("pg-porting.html", include "guide/pg-porting.typ")

#show: appendices
#part("pg-migrate.html", include "guide/pg-migrate.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
