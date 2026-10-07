#import "../bookmaster/bookmaster.typ": *

= Programs under TSO and ISPF <pg-tso>

#idx("TSO")
#idx("ISPF")
A C program built with libc370 runs under TSO without any change: the same
load module can be a batch job step, a program called from a terminal and a
TSO command. This chapter shows what differs between those ways of running
it -- how the program receives its arguments, where its standard streams
go, how it reads from the terminal -- and how a program issues TSO commands
and uses the dialog services of ISPF. The functions are described in the
_libc370 Library Reference_, Chapter 29, “Program Management and Storage”
(TSO commands), and Chapter 33, “Subsystems, JES2 and ISPF” (ISPF
services).

== Three Ways to Run a Program under TSO <pg-tso-ways>

#idx("TSO", "CALL command")
#idx("TSO", "command processor")
#idx("CPPL")
#idx("TSO", "background")
@pg-tso-ways-tab compares the three ways in which TSO starts a program.

#tab(caption: [How TSO starts a C program])[
  #table(columns: (1.15in, 1fr, 1fr, 1fr),
    [], [*CALL*], [*As a command*], [*TSO in batch*],
    [Typed as], [#cmd("CALL 'your.LOADLIB(WHERE)' 'A B'")],
      [#cmd("WHERE A B")], [either form, in #cmd("SYSTSIN") of a job that
      runs #cmd("IKJEFT01")],
    [Parameter], [a #cmd("PARM") string, as in batch], [a command processor
      parameter list (CPPL)], [as for the form used],
    [#cmd("argv[0]")], [the member name], [the command name], [as for the
      form used],
    [#cmd("stdout") without a #cmd("SYSPRINT") DD], [the terminal], [the
      terminal], [a SYSOUT data set],
    [#cmd("tsocmd()")], [not available], [available], [as for the form
      used],
  )
] <pg-tso-ways-tab>

*CALL* is the simplest way to try a program: it names the load library and
the member, and passes the parameter as #cmd("EXEC PGM=") passes
#cmd("PARM"). The C start-up splits it into #cmd("argv") at blanks\; a part
in double quotes stays one argument, as in batch (see @pg-startup).

*As a command*, the program is called by its member name, and receives a
CPPL: the command buffer, and the addresses of the control blocks of the
TSO session. The start-up takes the operands from the command buffer and
splits them as it splits a #cmd("PARM") string. For TSO to find the program
by name, its load library must be one that TSO searches for commands: the
#cmd("STEPLIB") of your logon procedure, or a library of the link list. A
program that is to call other TSO commands or ISPF services must be started
this way.

*TSO in batch* runs the terminal monitor program #cmd("IKJEFT01") as a job
step and reads the commands from #cmd("SYSTSIN"). The program then runs as
under TSO, either form, but without a terminal: what would go to the
terminal goes to SYSOUT.

== Finding Out How the Program Was Started <pg-tso-where>

#idx("CLIBPPA", "TSO flags")
#idx("PPAFLAG_TSOFG")
#idx("PPAFLAG_TSOBG")
#idx("GRTFLAG1_TSO")
The start-up records what it found in the run-time anchors (see
@pg-startup), and a program reads it there:

#deflist(width: 1.6in,
  [#cmd("PPAFLAG_TSOFG")], [in #cmd("ppaflag") of the #cmd("CLIBPPA"): the
    program runs in a TSO session at a terminal.],
  [#cmd("PPAFLAG_TSOBG")], [in #cmd("ppaflag"): the program runs under TSO,
    at a terminal or in a batch job (the TSO environment has a PSCB). In a
    batch job it is set without #cmd("PPAFLAG_TSOFG"), at a terminal with
    it, so test #cmd("PPAFLAG_TSOFG") first.],
  [#cmd("ppacppl")], [in the #cmd("CLIBPPA"): the address of the CPPL when
    the program was started as a command, otherwise #cmd("NULL").],
  [#cmd("GRTFLAG1_TSO")], [in #cmd("grtflag1") of the #cmd("CLIBGRT"): the
    parameter had the form of a TSO command buffer.],
)

@pg-tso-where-fig reports all of them, and its arguments.

#fig(caption: [WHERE, how was the program started?])[
  #code(read("../ex/pg-tso/where.c"), numbers: true)
] <pg-tso-where-fig>

To see the difference, run WHERE at a terminal in both forms, and in a
batch job:

```
CALL 'your.LOADLIB(WHERE)' 'one "two three"'
WHERE one "two three"
```

```
//WHERE    JOB  (ACCT),'TSO IN BATCH',CLASS=A,MSGCLASS=X
//TMP      EXEC PGM=IKJEFT01
//STEPLIB  DD   DSN=your.LOADLIB,DISP=SHR
//SYSTSPRT DD   SYSOUT=*
//SYSPRINT DD   SYSOUT=*
//SYSTSIN  DD   *
  WHERE one "two three"
  CALL 'your.LOADLIB(WHERE)' 'one "two three"'
/*
```

#fig(caption: [WHERE at a terminal and under IKJEFT01])[
  _Output to be captured on MVS._
] <pg-tso-where-out-fig>

#cmd("PPAFLAG_TSOFG") and #cmd("ppacppl") are the two tests a program
normally needs: the first decides whether there is a terminal, the second
whether TSO services such as #cmd("tsocmd()") can be used.
#cmd("GRTFLAG1_TSO") is a guess the start-up makes from the shape of the
parameter, before it knows more\; prefer #cmd("ppacppl").

#idx("TSO", "prefix of data set names")
One effect of the way a program is started is easy to miss: whether an
unquoted data set name given to #cmd("fopen()") gets the TSO prefix put in
front of it. The library adds the prefix only when #cmd("GRTFLAG1_TSO") is
set, that is when the program runs as a TSO command processor. Under
#cmd("CALL") the parameter has the batch shape, the flag is not set, and
the name is used as written, just as in batch. A program that may be started in more than one way should
write data set names fully qualified, in quotes, which mean the same in
every environment:

```
fp = fopen("'IBMUSER.TEST.DATA'", "r");
```

== Terminal Input and Output <pg-tso-term>

#idx("TSO", "terminal I/O")
#idx("stdout", "under TSO")
#idx("stdin", "under TSO")
The start-up opens the standard streams by DD name (see @pg-io), and under
TSO that leads to the terminal in some cases and not in others:

- #cmd("stdout") is the #cmd("SYSPRINT") DD and #cmd("stderr") the
  #cmd("SYSTERM") DD, when they are allocated. When they are not, the
  library allocates the terminal in a TSO session, and a SYSOUT data set
  otherwise.
- #cmd("stdin") is the #cmd("SYSIN") DD. When there is none, it is a dummy
  data set that is at end of file at once -- *not the terminal*.

So, before a program that reads from the terminal is run, allocate
#cmd("SYSIN") to the terminal, and check that no #cmd("SYSPRINT")
allocation from an earlier program sends the output to a data set instead:

```
ALLOCATE FILE(SYSIN) DATASET(*)
ALLOCATE FILE(SYSPRINT) DATASET(*)
CALL 'your.LOADLIB(ASK)'
FREE FILE(SYSIN SYSPRINT)
```

#idx("end of file", "at the TSO terminal")
A line from the terminal is read with the TGET service. Leading and trailing
blanks are removed from it. A terminal has no end-of-file key, so the
library takes a line that consists of #cmd("/*"), #cmd("//"),
#cmd("<eof>") or #cmd("<EOF>") as end of file. @pg-tso-ask-fig reads names
until then.

#fig(caption: [ASK, a terminal dialogue])[
  #code(read("../ex/pg-tso/ask.c"), numbers: true)
] <pg-tso-ask-fig>

#fig(caption: [ASK at the terminal])[
  _Output to be captured on MVS._
] <pg-tso-ask-out-fig>

Write each prompt as a complete line, ending in #cmd("\\n"). Output is
passed on a line at a time, when the line ends\; a prompt without a newline
appears only when the program calls #cmd("fflush()") or writes the next
newline.

The terminal stream is allocated with #cmd("RECFM=V") and
#cmd("LRECL=4000"). The environment variables #cmd("TERMINAL_RECFM"),
#cmd("TERMINAL_LRECL") and #cmd("TERMINAL_BLKSIZE") change that for a
terminal stream the program opens itself. The standard streams are opened
before the #cmd("SYSENV") DD is read, so a value from there does not reach
them\; one set with #cmd("setenv()") in #cmd("__premain()") does
(@pg-startup-premain).

#note[When a program abends, records that are still in the buffers of its
streams are lost. Under TSO a line has normally reached the terminal once it
is ended, but output to a data set or to SYSOUT is lost up to the last full
block (see @pg-errors).]

=== The TMP's Input and Output: \*PUTLINE and \*GETLINE <pg-tso-putline>

#idx("*PUTLINE")#idx("PUTLINE")#idx("SYSTSPRT")
Under #cmd("IKJEFT01") in batch, the terminal monitor program writes its
messages and the output of commands to #cmd("SYSTSPRT"), while
#cmd("stdout") goes to #cmd("SYSPRINT") or to a SYSOUT data set of its own.
To put a program's output into #cmd("SYSTSPRT"), in order with everything
else, open the file name #cmd("*PUTLINE") for writing. Each line goes out
as a PUTLINE: in batch into #cmd("SYSTSPRT"), in a TSO session to the
terminal, without a DD. Whether TSO/E #cmd("OUTTRAP") traps it at the
terminal has not been measured. A line of more than 252 characters is written as several lines.

Without the terminal monitor program -- a program run with
#cmd("EXEC PGM=") -- #cmd("fopen(\"*PUTLINE\", \"w\")") returns
#cmd("NULL") with #cmd("errno") #cmd("ENODEV"). The stream cannot be read.

#idx("*GETLINE")#idx("GETLINE")#idx("SYSTSIN")
The reading side is the file name #cmd("*GETLINE"), opened #cmd("\"r\"").
Each line comes from GETLINE: under #cmd("IKJEFT01") in batch, the next
line of #cmd("SYSTSIN"). A line the program reads is not run by the
terminal monitor program as a command\; the commands go on after the last
line read. At the end of #cmd("SYSTSIN") the stream reaches end of file,
and the terminal monitor program still ends normally. The end of file is
the #cmd("END") that the terminal monitor program supplies for itself
there: the read takes it, so the #cmd("END") line that otherwise closes
#cmd("SYSTSPRT") is missing. In a TSO session the line comes from the
terminal, without a DD. Leading
blanks are kept and trailing blanks removed. Without the terminal monitor
program the open fails with #cmd("ENODEV").

To send all of a program's standard output there and read its standard
input from there, set #cmd("stdout") and #cmd("stdin") in
#cmd("__premain()") (@pg-startup-premain). Where there is no terminal
monitor program both opens fail, and the usual streams are opened:

```
#include <stdio.h>
#include <mvs/crt.h>

int __premain(char *parm, char *pgmname, void **pgmr1)
{
    FILE *out = fopen("*PUTLINE", "w");
    FILE *in  = fopen("*GETLINE", "r");

    if (out)
        stdout = out;
    if (in)
        stdin = in;
    return 0;
}
```

== Calling TSO Commands from a Program <pg-tso-cmd>

#idx("tsocmd")
#idx("tsocmdf")
#idx("TSO", "command from a program")
#cmd("tsocmd()") calls a TSO command processor, such as #cmd("LISTCAT") or
#cmd("ALLOCATE"), the way TSO calls it when the command is typed: it builds
a command buffer from the command name and the operands and calls the
command processor with #cmd("LINK"), with a copy of the program's own CPPL.
#cmd("tsocmdf()") formats the operands as #cmd("printf()") does first.
@pg-tso-listc-fig lists the catalog entries of a level.

#fig(caption: [LISTC, calling the LISTCAT command])[
  #code(read("../ex/pg-tso/listc.c"), numbers: true)
] <pg-tso-listc-fig>

+ *Test for a CPPL first* (lines 18--21). #cmd("tsocmd()") needs the CPPL of
  the calling program, and a program has one only when it was started as a
  command. Without it #cmd("tsocmd()") returns 8 -- which is also a return
  code commands use -- and writes a message to the operator. Testing
  #cmd("ppacppl") gives the user a clear message and keeps the console
  quiet.
+ *Call the command* (line 23). The return code of the command processor is
  returned\; -1 means that it could not be called at all, for example
  because no command of that name was found.
+ *The command writes its own output.* LISTCAT writes to the terminal, or to
  #cmd("SYSTSPRT") under #cmd("IKJEFT01"), not to the program's
  #cmd("stdout").

Run LISTC as a command, not with CALL:

```
LISTC IBMUSER
```

#fig(caption: [LISTC at the terminal])[
  _Output to be captured on MVS._
] <pg-tso-listc-out-fig>

#cmd("tsocmdf()") formats into a buffer of 1024 bytes and cuts a longer
command. #cmd("tsocmd()") leaves the name of the command it called in the
environment control table of the session, as the primary command name.

=== Running Another Program: system()

#idx("system")
#cmd("system()") runs a load module as a subtask and waits for its end,
which is the way to run a _program_ rather than a command. Its argument is
the program name and, after a blank, the parameter. When the calling program
was started as a TSO command, the called program is started as a command
too\; otherwise it receives the parameter as a #cmd("PARM") string. The
result is a completion code, not a POSIX status: test the high-order bit for
an abend, as the example in the Library Reference shows. Do not call
#cmd("system(NULL)") to ask whether a command processor exists\; this
library does not support that call and takes the contents of low storage as
the program name.

== Using ISPF Services <pg-tso-ispf>

#idx("ISPF", "dialog services")
#idx("ISPEXEC")
#idx("ISPLINK")
ISPF offers its dialog services -- panels, tables, variables, file
tailoring -- through two interfaces: the #cmd("ISPEXEC") command, which takes
a service request as a string, and the #cmd("ISPLINK") routine, which takes
one parameter for each operand. A program that uses either must run as a TSO
command under ISPF, for example entered on the command line of the ISPF
option for TSO commands, or started with the ISPF #cmd("SELECT") service.

=== Requests as Strings: ISPEXEC

#idx("ispexec")
The simplest way to use a service that needs only names and values is to
pass the request, as a string, to the #cmd("ISPEXEC") command with
#cmd("tsocmd()"). @pg-tso-panel-fig displays a panel and reports which key
ended the display.

#fig(caption: [PANEL, displaying an ISPF panel])[
  #code(read("../ex/pg-tso/panel.c"), numbers: true)
] <pg-tso-panel-fig>

The library also offers #cmd("ispexec()"), which formats the request as
#cmd("printf()") does and calls the same command. Two properties of it make
the form of @pg-tso-panel-fig the safer one:

- #cmd("ispexec()") formats into a buffer of 256 bytes without checking the
  length: a longer request overwrites storage. #cmd("snprintf()") into a
  buffer of the program's own cannot overrun.
- #cmd("ispexec()") formats the request a second time on the way to the
  command, so a #cmd("%") in a substituted value -- a data set name, a
  user's input -- is taken as a conversion. #cmd("tsocmd()") passes the
  string as it is.

The panel must be in a library of the #cmd("ISPPLIB") concatenation of the
ISPF session. From the ISPF command line for TSO commands, enter

```
PANEL MYPANEL
```

#fig(caption: [PANEL under ISPF])[
  _Output to be captured on MVS._
] <pg-tso-panel-out-fig>

=== Parameters by Address: ISPLINK

#idx("isplink")
#idx("ISP_LAST")
Some services exist only in the #cmd("ISPLINK") interface, because they take
the address of a program variable: #cmd("VDEFINE"), #cmd("VCOPY"),
#cmd("VDELETE") and the others marked so in #cmd("<mvs/ispf.h>").
#cmd("isplink()") calls #cmd("ISPLINK") with one parameter for each argument.
Every argument is an address, and the last one must be marked with
#cmd("ISP_LAST()"), which sets its high-order bit\; a C call does not set it
by itself (see @pg-asm). Without the mark #cmd("isplink()") returns 20 and
writes a message to the operator. @pg-tso-zuser-fig ties the dialog variable
#cmd("ZUSER") to a C array and reads it.

#fig(caption: [ZUSER, reading an ISPF variable])[
  #code(read("../ex/pg-tso/zuser.c"), numbers: true)
] <pg-tso-zuser-fig>

The service names in #cmd("<mvs/ispf.h>") are padded with blanks to the
eight characters #cmd("ISPLINK") expects, such as #cmd("ISP_VDEFINE"). One of
them is wrong: #cmd("ISP_LMMDEL") is #cmd("\"LMDDEL  \""). To delete a
member, write #cmd("\"LMMDEL  \"") yourself.

#idx("ISPLINK", "resolving at link time")
*Linking.* #cmd("ISPLINK") is a routine of ISPF, not of libc370, and the
linker on the workstation cannot find it: @pg-tso-zuser-link-fig shows the
first link failing, as it should, because the module would end with an
abend at its first call. The same holds for #cmd("ispf_available()"), which
calls the ISPF routine #cmd("ISPQRY").

#fig(caption: [Linking ZUSER on the workstation])[
  #screen(raw(read("../ex/pg-tso/zuser-link.txt")))
] <pg-tso-zuser-link-fig>

The second link writes the module with the reference left open. It must be
resolved on MVS before the program runs, by link-editing the module once
more with the ISPF load library as the automatic call library:

+ Link with #cmd("-Wl,--allow-unresolved"), as above, and transfer the
  module to a load library on MVS (see the _cc370 User's Guide_).
+ Link-edit it again on MVS, with the library that contains #cmd("ISPLINK")
  as #cmd("SYSLIB"):
  ```
  //RELINK   JOB  (ACCT),'RESOLVE ISPLINK',CLASS=A,MSGCLASS=X
  //LKED     EXEC PGM=IEWL,PARM='LIST,MAP,XREF'
  //SYSLIB   DD   DSN=your.ISPF.LOADLIB,DISP=SHR
  //SYSLMOD  DD   DSN=your.LOADLIB,DISP=SHR
  //SYSUT1   DD   UNIT=SYSDA,SPACE=(CYL,(1,1))
  //SYSPRINT DD   SYSOUT=*
  //SYSLIN   DD   *
    INCLUDE SYSLMOD(ZUSER)
    ENTRY @@CRT0
    NAME ZUSER(R)
  /*
  ```
+ Check in the map that #cmd("ISPLINK") is now resolved, then run ZUSER as
  a command under ISPF.

#fig(caption: [IEWL map of ZUSER and ZUSER under ISPF])[
  _Output to be captured on MVS._
] <pg-tso-zuser-out-fig>

A program that needs only services of the #cmd("ISPEXEC") interface avoids
this step altogether: @pg-tso-panel-fig contains no reference to an ISPF
routine and links like any other program.
