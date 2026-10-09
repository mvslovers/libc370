#import "../bookmaster/bookmaster.typ": *

// Narrow cells: ragged right.
#show table: set par(justify: false)

= Errors and Recovery <pg-errors>

#idx("error handling")
A C program on MVS meets failure in two forms. A library function that
cannot do its work says so: it returns a value that marks the failure and,
usually, stores a reason in #cmd("errno"). The program decides what to do,
and goes on. An _abend_ is different: the system or the program itself ends
the task abnormally, with a completion code, and unless a recovery routine
intervenes the program is gone. This chapter describes both, the
#cmd("try()") function with which a program survives an abend, and what the
signals of the C standard do -- and do not do -- on MVS.

== Errors Reported by Functions <pg-errors-errno>

#idx("errno")#idx("errno", "per task")
The return value of a function says whether it failed: #cmd("NULL") from
#cmd("fopen()") and #cmd("malloc()"), #cmd("EOF") from #cmd("fclose()"), a
nonzero value from most others. Each function entry of the _LIBC/370 Library
Reference_ gives the value. #cmd("errno") says why, and only after a
failure: a function that succeeds may leave an earlier value in it.

#cmd("errno") is kept for each task. It is a macro that finds the error
number in the task anchor of the task that runs the code, so a subtask
has an #cmd("errno") of its own and never sees a value stored by another
task. It is 0 when #cmd("main()") is entered.

To use #cmd("errno") correctly:

+ Test the return value first. Look at #cmd("errno") only when the return
  value reports a failure.
+ Where a function can fail without a distinct return value, as
  #cmd("strtol()") can, set #cmd("errno") to 0 before the call and test it
  after.
+ Copy #cmd("errno") into a variable of your own before calling anything
  else, even #cmd("printf()"): any call may change it.
+ Check that the function you called sets #cmd("errno") at all. After a
  failed #cmd("fopen()"), for example, it is meaningful only for a few
  reasons (@pg-io-errors). Functions that call an MVS service often return
  the service's own code instead, as #cmd("remove()") returns the condition
  code of IDCAMS.

#idx("strerror")#idx("perror")
The values of #cmd("errno") are the numbers that UNIX systems use, so a
ported program finds the names it expects; most of them describe
conditions that cannot occur on MVS. #cmd("strerror()") returns the text of
a value and #cmd("perror()") writes it to #cmd("stderr"). #cmd("strerror()")
has no text for every value: for the VSAM values 150 to 154 and for any
number above 131 it returns a null pointer. Test its result before passing
it to #cmd("printf()"), as the function #cmd("errtext") in
@pg-errors-errtext-fig does.

#fig(caption: [Reporting errno safely])[
  #code(read("../ex/pg-errors/errtext.c"), numbers: true)
] <pg-errors-errtext-fig>

@pg-errors-errtext-jcl gives the program a temporary data set of one
track, which 1000 records do not fit. The write that runs out of space
fails with #cmd("ENOSPC"), the program reports it on #cmd("SYSTERM") with
the text #cmd("No space left on device") and the number 28, and does not
abend.

#fig(caption: [JCL with an output data set that is too small])[
  #code(read("../ex/pg-errors/errtext.jcl"))
] <pg-errors-errtext-jcl>

#fig(caption: [Output of ERRTEXT on SYSTERM])[_Output to be captured on
MVS._] <pg-errors-errtext-out>

== What an Abend Does <pg-errors-abend>

#idx("abend")#idx("completion code")
An abend ends the task with a completion code: a _system_ completion code
#cmd("S")#var("xxx"), three hexadecimal digits, when the system ends it,
or a _user_ completion code #cmd("U")#var("nnnn"), four decimal digits,
when a program ends itself with the #cmd("ABEND") macro. The most common
causes in a C program are:

#tab(caption: [Abends a C program commonly causes])[
  #table(columns: (0.7in, 1fr),
    [Code], [Usual cause],
    [#cmd("S0C4")], [A protection exception: a store through a null or
      uninitialized pointer, a store past the end of an array, or a stack
      that has overflowed (@pg-startup-stack).],
    [#cmd("S0C1")], [An operation exception: a call through a pointer that
      does not point to code.],
    [#cmd("S0C9")], [Integer division by zero.],
    [#cmd("S0CF")], [Floating-point division by zero.],
    [#cmd("S806")], [A program named in #cmd("LINK") or #cmd("ATTACH") was
      not found.],
    [#cmd("S878"), #cmd("S80A")], [Not enough storage in the region for a
      request that cannot fail softly.],
    [#cmd("U0801")], [The C start-up could not obtain its stack or anchors
      (@pg-startup-sequence).],
  )
] <pg-errors-codes-tab>

When a program abends, *none of the C termination runs*. The
#cmd("atexit()") functions are not called, no stream is closed, and the
records still in the buffers of the output streams are lost -- among them
the last block of every output data set, even after #cmd("fflush()"). The
step ends with the completion code instead of a return code, a dump is
written if the step has a #cmd("SYSUDUMP") or #cmd("SYSABEND") DD, and the
later steps of the job are skipped unless their #cmd("COND") parameter says
#cmd("EVEN") or #cmd("ONLY").

#idx("wtof")#idx("console", "progress messages")
Because the output of the program is lost exactly when it would explain
the abend, a program that may abend reports its progress on the console
rather than through #cmd("stdout"). #cmd("wtof()") from #cmd("<mvs/wto.h>")
formats a message as #cmd("printf()") does and writes it at once to the
job log:

```
wtof("MYPROG01I %ld RECORDS READ", count);
```

Keep such messages few. On MVS 3.8j the console log is shared by the whole
system, and a message per record fills it.

== Recovering from an Abend with try() <pg-errors-try>

#idx("try")#idx("ESTAE")#idx("recovery")
MVS lets a program establish a _recovery routine_ with the #cmd("ESTAE")
macro: when the task abends, the system calls the routine, which may let
the abend go on or resume the program at a point of its choosing. The
function #cmd("try()") of #cmd("<mvs/recovery.h>") packages this for a C
program. It calls a function under a recovery routine; when the function
returns, #cmd("try()") returns 0, and when the function abends,
#cmd("try()") returns the completion code, suppresses the dump and lets
the program continue after the call.

The value #cmd("try()") returns has the form #cmd("X'00sssuuu'"):
#cmd("sss") is the system completion code and #cmd("uuu") the user
completion code. An #cmd("S0C4") gives #cmd("X'000C4000'"), a #cmd("U0100")
gives #cmd("X'00000064'"). A negative value means that the recovery routine
could not be established and the function was not called.

To call a function under #cmd("try()"):

+ Write the function so that it passes its results through its arguments.
  #cmd("try()") does not return the value of the function.
+ Call it as #cmd("try(")#var("function")#cmd(", ")#var("arguments")#cmd(")").
  #cmd("try()") is a macro that needs at least one argument after the
  function; pass #cmd("0") to a function that takes none.
+ Test the result: negative, zero, or a completion code. The system code
  is #cmd("(rc >> 12) & 0xFFF"), the user code #cmd("rc & 0xFFF").

@pg-errors-guard-fig calls the same function twice, once with a valid
pointer and once with a null pointer, through which the store abends with
#cmd("S0C4"). Run with @pg-errors-guard-jcl, it writes #cmd("GOOD: completed"),
#cmd("BAD: abend S0C4") and #cmd("value=42") and ends with return code 8 --
not with an abend.

#fig(caption: [Surviving an abend with try()])[
  #code(read("../ex/pg-errors/guard.c"), numbers: true)
] <pg-errors-guard-fig>

#fig(caption: [JCL for GUARD])[
  #code(read("../ex/pg-errors/guard.jcl"))
] <pg-errors-guard-jcl>

#fig(caption: [Output of GUARD])[_Output to be captured on MVS._]
<pg-errors-guard-out>

#idx("try", "what is left behind")
#cmd("try()") stops the abend, but it does not undo what the function did
before it. Storage the function obtained stays allocated, a stream it
opened stays open, and an #cmd("ENQ") it holds stays held. The data a
function was changing may be half changed. Use #cmd("try()") around work
whose effects the caller can clean up or discard, and keep that work
small: the recovery routine is no substitute for checking a pointer before
it is used.

Some related services cover particular cases:

#deflist(width: 1.5in,
  [#cmd("__linkt()")], [calls another program with #cmd("LINK") under
    #cmd("try()"). When that program is a C program and abends, the library
    also closes its files and frees its stack and anchors, which its own
    termination code could not.],
  [#cmd("abendrpt()")], [establishes a recovery routine that writes an
    abend report to the console -- completion code, failing module and
    offset, registers, a traceback -- and then lets the abend go on, with or
    without a dump.],
  [#cmd("estae()")], [establishes a C function as a recovery routine of the
    program's own, for a program that needs more control than
    #cmd("try()") gives.],
  [#cmd("__fabandon()")], [closes a stream whose write abended with an x37
    code, after the program has recovered (@pg-io-errors).],
)

All of them are described in the _LIBC/370 Library Reference_, Chapter
“Program Management and Storage”. Each task can have at most ten recovery
routines established at a time, and #cmd("try()") uses one for the
duration of the call.

== Signals <pg-errors-signals>

#idx("signals")#idx("SIGABRT")#idx("abort")
MVS has no signals. LIBC/370 provides #cmd("signal()") and #cmd("raise()")
so that programs written to the C standard compile and run, but a signal
occurs only when the program calls #cmd("raise()"), or #cmd("abort()"),
which raises #cmd("SIGABRT"). Nothing else raises one:

- A program check -- #cmd("S0C4"), #cmd("S0C7"), #cmd("S0C9") and the others
  -- is an abend. It does not raise #cmd("SIGSEGV"), #cmd("SIGFPE") or
  #cmd("SIGILL").
- The attention key of a TSO terminal does not raise #cmd("SIGINT"), and an
  operator's #cmd("STOP") or #cmd("CANCEL") does not raise #cmd("SIGTERM").

A program that must survive an abend uses #cmd("try()"), not a handler for
#cmd("SIGSEGV").

The default action of every signal except #cmd("SIGABRT") is to do
nothing. For #cmd("SIGABRT") it is #cmd("exit(EXIT_FAILURE)"): the
#cmd("atexit()") functions run, the files are closed, and the program ends
with return code 12, without an abend. When #cmd("abort()") raises
#cmd("SIGABRT"), a handler installed for it runs first, and if it returns,
#cmd("abort()") still ends the program that way.

Three properties of #cmd("signal()") differ from the C standard and matter
when a program installs handlers:

- *It returns the new handler, not the previous one.* The usual idiom of
  saving the result of #cmd("signal()") and restoring it later does not
  work. A program that needs the earlier handler again keeps it in a
  variable of its own.
- *An invalid signal number is not reported.* #cmd("signal()") never
  returns #cmd("SIG_ERR"), and #cmd("raise()") returns 0 for a number it
  does nothing with.
- *A handler is shared by all tasks.* The table of handlers belongs to the
  program, not to the task, so a handler installed in one task is in force
  in all of them.

#idx("setjmp")#idx("longjmp")
#cmd("setjmp()") and #cmd("longjmp()") work as the standard describes and
are the way to leave nested calls when an error is detected. They are not
a way out of an abend, and #cmd("longjmp()") restores registers only: it
closes no file, frees no storage and removes no recovery routine
established by the functions it leaves.
