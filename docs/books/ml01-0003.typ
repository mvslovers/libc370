#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "libc370 for MVS 3.8j",
  subtitle: "Programmer's Guide",
  short-title: "libc370 Programmer's Guide",
  number: "ML01-0003-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[First Edition (October 2026)]

    This edition applies to the release of libc370 that follows libc370
    2.2.0, and to all subsequent releases and modifications until otherwise
    indicated in new editions. The release number is entered here when that
    release is made.

    *Draft.* This book is in preparation and describes the library as it is
    being developed. It is published together with that release.

    Comments on this book may be addressed to the issue tracker of
    the mvslovers/libc370 repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book explains how to write C programs for MVS 3.8j with libc370: how a
program starts and ends, how it reads and writes data sets, what the
EBCDIC character set and the 24-bit address space mean for C code, and how
a program uses MVS services such as dynamic allocation, the console,
multitasking and TCP/IP.

Each function is described in full in the companion volume, the _libc370
Library Reference_\; this book shows how the functions are used together.

libc370 is built with, and only with, the cc370 toolchain. Every header
checks the compiler: compiled with a cc370 older than 1.1.0 it stops with
#cmd("#error \"libc370 needs cc370 1.1.0 or later\"").

== Who Should Use This Book

This book is for programmers who write C programs that run on MVS 3.8j. It
assumes that you know the C language and the basic concepts of MVS:
data sets, DD statements, job steps and load modules.

== How This Book Is Organized

#deflist(width: 1.35in,
  [Chapter 1], [“Introducing libc370”.],
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
  [Appendix A], [“Migrating from crent370 and libc370 1.x”.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML01-0001], [_cc370 User's Guide_],
  [ML01-0002], [_cc370 Command Reference_],
  [ML01-0004], [_libc370 Library Reference_],
)

#mainmatter()
#set page(numbering: "1")

#include "guide/pg-intro.typ"
#include "guide/pg-startup.typ"
#include "guide/pg-io.typ"
#include "guide/pg-charset.typ"
#include "guide/pg-storage.typ"
#include "guide/pg-errors.typ"
#include "guide/pg-rent.typ"
#include "guide/pg-tasks.typ"
#include "guide/pg-services.typ"
#include "guide/pg-sockets.typ"
#include "guide/pg-tso.typ"
#include "guide/pg-asm.typ"
#include "guide/pg-porting.typ"

#show: appendices
#include "guide/pg-migrate.typ"

#heading(numbering: none)[Index]
#make-index()
