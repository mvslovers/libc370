#import "../bookmaster/bookmaster.typ": *

// Narrow cells: ragged right.
#show table: set par(justify: false)

= Program Structure and Start-Up <pg-startup>

#idx("start-up", "of a C program")
A C program on MVS is a load module that MVS calls like any other program:
with #cmd("EXEC PGM=") in a job step, with #cmd("CALL") or as a command
under TSO, or with #cmd("LINK") or #cmd("ATTACH") from another program.
Before #cmd("main()") can run, the library has to build a C environment
around it: a stack, the anchors that hold #cmd("errno") and the open
files, the three standard streams, the arguments and the environment
variables. This chapter describes what the start-up does, how to choose
between the three start-up modules, how a program receives its parameter
and returns its return code, how to give it a larger stack, and how to
pass it settings through the #cmd("SYSENV") DD statement.

== What Happens Before main() <pg-startup-sequence>

#idx("@@CRT0")#idx("@@START")#idx("entry point")
The entry point of a C load module is #cmd("@@CRT0"), the start-up routine
of the library. A source file that defines #cmd("main()") refers to it, so
every C program is linked with it and enters there;
@pg-startup-sequence-fig shows the path from MVS to #cmd("main()").

#fig(caption: [From MVS to main()])[
  #code("MVS            EXEC PGM=, CALL, LINK, ATTACH
  |
  v
@@CRT0         start-up routine (libc.a, crt0.o; crtm.o for a module)
  |              - one GETMAIN for the program area and the stack
  |              - the anchors: program, task, process
  |              - program name, TSO or batch
  |              - IDENTIFY CTHREAD, if the program has threads
  v
@@START        C start-up routine in libc.a
  |              - opens stdout, stderr, stdin
  |              - reads the SYSENV (or ENVIRON) DD
  |              - splits the parameter into argc and argv
  v
main(argc, argv)
  |
  v
exit(status)   atexit() functions, files closed, storage freed,
               status returned to MVS in register 15")
] <pg-startup-sequence-fig>

In detail, #cmd("@@CRT0"):

+ obtains one area of storage, with a conditional #cmd("GETMAIN"), that
  holds the _program properties area_ and the C stack. The size of the
  stack is 256 KB unless the program asks for another (see
  @pg-startup-stack);
+ chains the program properties area to the first save area of the task,
  where every library function can find it without a global variable;
+ creates the _task anchor_, which holds #cmd("errno") and the other
  per-task state of the library, and the _process anchor_, which holds the
  open files, the #cmd("atexit()") functions and the environment variables;
+ records the program name and finds out whether the program runs under
  TSO, in the foreground or in the background;
+ makes the thread driver #cmd("CTHREAD") known to MVS with
  #cmd("IDENTIFY"), if the program contains it (@pg-startup-variants);
+ calls #cmd("@@START").

#cmd("@@START") then opens #cmd("stdout"), #cmd("stderr") and #cmd("stdin")
(@pg-io describes them), loads the environment variables, sets the time
zone, divides the parameter into #cmd("argv") (@pg-startup-parm) and calls
#cmd("main()"). When #cmd("main()") returns, it calls #cmd("exit()") with
the value #cmd("main()") returned.

#idx("U0801 abend")
Two things can stop a program before #cmd("main()"):

- *No storage for the stack or the anchors.* The start-up writes the console
  message #cmd("@@CRT0 - No storage for C stack") (or #cmd("CLIBGRT") or
  #cmd("CLIBCRT") in place of #cmd("C stack")) and ends with abend
  #cmd("U0801") and a dump. The region of the step is too small for the
  stack; raise #cmd("REGION") or ask for a smaller stack.
- *A standard stream cannot be opened.* The start-up writes a console
  message that begins #cmd("@@START:") and names the stream and the
  #cmd("errno") value, for example
  ```
  @@START: stdout (SYSPRINT) could not be opened: errno 12, out of storage - raise REGION
  ```
  and ends the program with return code 12 without calling #cmd("main()").
  A missing DD is not a reason: a missing #cmd("SYSPRINT") or
  #cmd("SYSTERM") is replaced by a SYSOUT data set, a missing #cmd("SYSIN")
  by an empty input.

== The Start-Up Routines <pg-startup-variants>

#idx("@@CRT0", "in libc.a")#idx("crt0")#idx("crt1")#idx("crtm")
#idx("start-up module")
There are two start-up routines, and both define the entry point
#cmd("@@CRT0"), so a program is linked with exactly one of them:

#tab(caption: [The start-up routines])[
  #table(columns: (1.1in, 0.9in, 1fr),
    [Routine], [Default stack], [Use it for],
    [#cmd("@@CRT0"), a member of #cmd("libc.a")], [256 KB], [Every C
      program, with or without threads. It builds the C environment
      described above and releases it when the program ends.],
    [#cmd("crtm.o")], [64 KB], [A C module that is called from a C program
      running on the same task and shares its C environment. It builds no
      environment of its own. Never use it for a program that MVS starts.],
  )
] <pg-startup-variants-tab>

#cmd("@@CRT0") is a member of #cmd("libc.a"), so a link that names no
start-up object takes it from the library, like any other function. It is
also installed as the files #cmd("crt0.o") and #cmd("crt1.o"), two identical
copies for the build tools that name a start-up object explicitly; the cc370
driver links #cmd("crt0.o"). Which of the three a program is linked with
makes no difference to what it does.

#idx("CTHREAD", "and the start-up")#idx("IDENTIFY", "CTHREAD")
*Threads.* A thread is attached under the name #cmd("CTHREAD"), the thread
driver of the library. The driver is a member of #cmd("libc.a") of its own,
which a call of #cmd("cthread_create()") links into the program. The
start-up looks for it, and only when it finds it does it make the name known
to MVS with #cmd("IDENTIFY"). A program without threads therefore carries no
driver and issues no #cmd("IDENTIFY"), and a program with threads needs
nothing of its own to make them work. A program that issues the
#cmd("IDENTIFY") itself as well gets return code 4 from it, which does no
harm. The start-up does not check the return code of its own
#cmd("IDENTIFY")\; @pg-tasks describes threads.

#note[Up to libc370 2.2.0 there were two start-up objects that differed in
this one step: #cmd("crt0.o") identified #cmd("CTHREAD") in every program,
and a program linked with #cmd("crt1.o") could not create threads. A load
module keeps the start-up it was linked with, so an old module behaves the
old way until it is linked again.]

#idx("entry point", "not at offset 0")
*Where the entry point is.* When #cmd("@@CRT0") comes from #cmd("crt0.o"),
named first on the link, it is at offset 0 of the load module. When it comes
from #cmd("libc.a"), it lies wherever the linker placed it. MVS does not
care: the entry point is recorded in the directory entry of the load module,
which ld370 writes in either case, and #cmd("LINK"), #cmd("ATTACH") and
#cmd("LOAD") take it from there. Only code that treats the first byte of a
module as its entry point would notice.

@pg-startup-crtlink-fig shows three modules as file370 lists them:
#cmd("plain"), a program without threads, and #cmd("thread"), one that
calls #cmd("cthread_create()"), both linked with #cmd("crt0.o") first as
the cc370 driver does it, and #cmd("plain2"), the first program linked
without a start-up object. The thread driver #cmd("CTHREAD") is an
unresolved weak reference (#cmd("WX")) in a program without threads, and
#cmd("@@CRT0") is at offset 0 only when #cmd("crt0.o") was named.

#fig(caption: [The start-up routine and the thread driver in three load
  modules])[
  #screen(raw(read("../ex/pg-startup/crtlink.txt")))
] <pg-startup-crtlink-fig>

#idx("crtm", "restrictions")
*crtm.* #cmd("crtm") is for a narrow case. It expects to find the anchors
of a C program that is already running on the same task, uses them instead
of building its own, and at the end releases only its own stack. It is
entered with the address of its parameter in register 0, not through a
parameter list in register 1, so it is called by code written for it, not
by MVS. Three consequences follow:

- Started as a job step, or on a task without a C program, a #cmd("crtm")
  module abends at once.
- Its #cmd("@@START") opens the standard streams again, in the shared
  environment, so the calling program finds its #cmd("stdout"),
  #cmd("stderr") and #cmd("stdin") replaced.
- It issues no #cmd("IDENTIFY"). A thread it creates can be attached only
  when the calling program has made #cmd("CTHREAD") known.

A program that calls another C program with #cmd("__link()") or
#cmd("system()") does not need #cmd("crtm"): the called program, with the
usual #cmd("@@CRT0"), builds its own environment and releases it before it
returns.

=== Linking with crtm <pg-startup-link>

#idx("-nostartfiles")
To link a module with #cmd("crtm"):

+ Leave out #cmd("crt0.o") with #cmd("-nostartfiles").
+ Name #cmd("crtm.o"). #cmd("cc370 -print-file-name=crtm.o") gives its full
  name in the installed library.

@pg-startup-crtm-fig shows the command, which is silent when it succeeds,
and the entry point, which #cmd("crtm.o") puts at offset 0.

#fig(caption: [Linking with crtm])[
  #screen(raw(read("../ex/pg-startup/crtm.txt")))
] <pg-startup-crtm-fig>

#idx("mbt", "start-up module")
With mbt, the key #cmd("startup") of a #cmd("[[module]]") in
#cmd("project.toml") selects the start-up object: #cmd("\"crt0\"") (the
default) and #cmd("\"crt1\"") now give the same module, #cmd("\"crtm\"")
links #cmd("crtm.o"). The _cc370 Command Reference_, Chapter 1, “The cc370
Command”, describes the link options.

== Receiving Arguments <pg-startup-parm>

#idx("PARM", "and argv")#idx("argc")#idx("argv")
#cmd("main()") is declared #cmd("int main(void)") or
#cmd("int main(int argc, char **argv)"). The start-up fills #cmd("argv") from
the parameter the program was given:

- In a job step, the #cmd("PARM") value of the #cmd("EXEC") statement.
- Under TSO, with #cmd("CALL"), the parameter given to #cmd("CALL"), in
  the same way.
- Under TSO as a command, the operands of the command.

#cmd("argv[0]") is the name of the program: in a job step and with
#cmd("CALL") the name under which MVS loaded the module, up to eight
characters; as a TSO command the command name. The rest of the parameter
is divided into arguments by these rules:

- Arguments are separated by one or more blanks. Blanks before the first
  and after the last argument are ignored.
- An argument that begins with a double quotation mark #cmd("\"") ends at
  the next #cmd("\""), and may contain blanks. The quotation marks are not
  part of the argument, and #cmd("\"\"") is an empty argument. A quotation
  mark inside an argument has no special meaning. A quoted argument that is
  not closed runs to the end of the parameter.
- Nothing else is interpreted: there is no escape character, and lowercase
  letters are not changed.
- At most 49 arguments are taken; the rest of the parameter is ignored. A
  parameter longer than 307 characters is cut to 307.

Without a parameter, #cmd("argc") is 1. #cmd("argv[argc]") is always a
null pointer. The strings are in storage of the start-up that lasts until
the program ends, and the program may change them.

@pg-startup-args-fig prints its arguments, and @pg-startup-args-jcl runs it.

#fig(caption: [A program that shows its arguments])[
  #code(read("../ex/pg-startup/args.c"), numbers: true)
] <pg-startup-args-fig>

#fig(caption: [Passing a parameter in a job step])[
  #code(read("../ex/pg-startup/args.jcl"))
] <pg-startup-args-jcl>

The apostrophes belong to JCL; the program receives
#cmd("COPY \"MY FILE\" 10"). It prints #cmd("argc=4"), then #cmd("ARGS")
as #cmd("argv[0]") and the three arguments #cmd("COPY"),
#cmd("MY FILE") and #cmd("10").

#fig(caption: [Output of ARGS])[_Output to be captured on MVS._]
<pg-startup-args-out>

#note[A JCL #cmd("PARM") value can be at most 100 characters long. A
program that needs more input than that reads it from a data set, such as
#cmd("SYSIN").]

== Ending the Program <pg-startup-exit>

#idx("return code")#idx("exit")#idx("atexit")#idx("EXIT_FAILURE")
A C program ends normally in one of three ways: #cmd("main()") returns,
the program calls #cmd("exit()"), or it calls #cmd("_Exit()"). In each
case the library closes every open stream -- which writes the last block of
every output data set -- releases its storage and returns to MVS with the
status in register 15. #cmd("exit()") and a return from #cmd("main()") first
call the functions registered with #cmd("atexit()"), the last registered
first. #cmd("_Exit()") does not.

The status becomes the return code of the program and, in a batch job,
the condition code of the step, which the job log shows and the
#cmd("COND") parameter of later steps tests. MVS programs report with the
conventional values, and a C program should use them too:

#tab(caption: [Conventional return codes])[
  #table(columns: (0.6in, 1fr),
    [Code], [Meaning],
    [0], [Success. #cmd("EXIT_SUCCESS") is 0.],
    [4], [Warning: the work was done, but something deserves a look.],
    [8], [Error: the work was not done, or only in part.],
    [12], [Severe error. #cmd("EXIT_FAILURE") is 12 on MVS, not 1.],
    [16], [Terminal error, for example a missing DD or no storage.],
  )
] <pg-startup-rc-tab>

A #cmd("COND") test can compare values from 0 to 4095; keep return codes in
that range.

#cmd("abort()") is not an abnormal end on MVS. It raises #cmd("SIGABRT") and
then calls #cmd("exit(EXIT_FAILURE)"): the #cmd("atexit()") functions run,
the files are closed and the program ends with return code 12, without an
abend and without a dump. @pg-errors describes abends and signals.

To end a program with a return code:

+ Choose a code from @pg-startup-rc-tab for each way the program can end.
+ Register with #cmd("atexit()") whatever must be done on every normal end,
  such as a closing line in a report. Do it early in #cmd("main()").
+ Return the code from #cmd("main()"), or call #cmd("exit()") where
  returning would be awkward.

@pg-startup-rcode-fig follows these steps. Run as in @pg-startup-rcode-jcl
it writes #cmd("REPORT BEGINS"), #cmd("limit 250") and #cmd("REPORT ENDS")
to #cmd("SYSPRINT") and ends with return code 4. Without a #cmd("PARM") it
writes #cmd("no limit given") to #cmd("SYSTERM") and ends with 8; the
trailer is written in both cases.

#fig(caption: [Return codes and an atexit() function])[
  #code(read("../ex/pg-startup/rcode.c"), numbers: true)
] <pg-startup-rcode-fig>

#fig(caption: [Testing the return code in the next step])[
  #code(read("../ex/pg-startup/rcode.jcl"))
] <pg-startup-rcode-jcl>

#note[Nothing of this happens when the program abends. The
#cmd("atexit()") functions are not called, the streams are not closed, and
the records still in their buffers are lost; see @pg-errors.]

== The Stack <pg-startup-stack>

#idx("stack", "size")#idx("__stklen")#idx("@@STKLEN")
The automatic variables of every C function, and the save area of every
call, are kept on the C stack, which the start-up obtains in one piece
before #cmd("main()") is called. Its size is fixed for the life of the
program: 256 KB with #cmd("@@CRT0"), 64 KB with #cmd("crtm").

*The stack is not checked.* Each function takes its frame from the top of
the stack as it is called, and nothing tests whether the frame still fits.
A program that needs more stack than it has writes past the end of it into
whatever storage follows. The result may be an abend at once, usually a
protection exception (#cmd("S0C4")), or damaged data that shows up much
later in a place that has nothing to do with the cause.

To find out how much stack a function needs, compile the source with
#cmd("-S") and look at the #cmd("FRAME=") operand of the #cmd("PDPPRLG")
macro at the start of each function: it is the size of the frame in bytes.
@pg-startup-frame-fig shows it for the program of @pg-startup-stack-fig.
The static function #cmd("fill") is #cmd("@@F2") in the assembler
source.

#fig(caption: [The stack frame of each function])[
  #screen(raw(read("../ex/pg-startup/frame.txt")))
] <pg-startup-frame-fig>

#cmd("fill") needs 32,856 bytes for each call and calls itself ten times,
so eleven frames, about 360 KB, are on the stack at the deepest point --
more than the default 256 KB.

To give a program a larger stack:

+ Define, in one source file of the program, an external variable
  #cmd("__stklen") of type #cmd("unsigned") with the size in bytes. The
  compiler gives it the external name #cmd("@@STKLEN"), which the start-up
  looks for.
+ Make the size at least 4096. A smaller value, or no #cmd("__stklen")
  at all, gives the default size.
+ Check that the region of the step can hold the stack in one piece,
  beside everything else the program allocates.

#fig(caption: [A program that asks for a 512 KB stack])[
  #code(read("../ex/pg-startup/stack.c"), numbers: true)
] <pg-startup-stack-fig>

#cmd("__stklen") is read by the start-up and never written, so it does not
stand in the way of a reentrant program (@pg-rent). A smaller stack is
worth asking for too: a program that is called many times in one address
space, such as a server module, holds its whole stack for every call.
@pg-storage describes how to keep the stack use of a program small.

== Settings from the SYSENV DD <pg-startup-env>

#idx("environment variables", "SYSENV")#idx("SYSENV DD statement")
#idx("ENVIRON DD statement")
MVS passes no environment to a program. The library keeps a list of
environment variables for each program, shared by all its tasks, and the
start-up fills it from a data set: the one allocated to the DD
#cmd("SYSENV"), or, if the step has none, to #cmd("ENVIRON"). Without
either, the list starts empty. #cmd("getenv()") reads a variable,
#cmd("setenv()") changes one.

Each record of the data set sets one variable:

- #var("name")#cmd("=")#var("value"). Blanks around the name and in front
  of the value are removed, and so are trailing blanks.
- A record that begins with #cmd("*") or #cmd("#") is a comment; a record
  without #cmd("=") is ignored.
- When the last eight characters of a record longer than eight characters
  are all digits, they are taken for a sequence number and removed. A value
  that ends in eight digits therefore loses them; set such a value with
  #cmd("setenv()") instead.

#fig(caption: [Reading a setting with getenv()])[
  #code(read("../ex/pg-startup/env.c"), numbers: true)
] <pg-startup-env-fig>

#fig(caption: [Settings in the SYSENV DD])[
  #code(read("../ex/pg-startup/env.jcl"))
] <pg-startup-env-jcl>

Run with @pg-startup-env-jcl, the program prints #cmd("limit is 250")\;
without the #cmd("SYSENV") DD, #cmd("limit is 100").

Some variables are read by the library itself: #cmd("TZ") sets the time
zone, and the variables that begin with #cmd("DATASET_"), #cmd("TEMP_"),
#cmd("SYSOUT_") and #cmd("TERMINAL_") give the attributes of data sets that
#cmd("fopen()") creates (@pg-io-new). The start-up opens the standard streams
_before_ it reads #cmd("SYSENV"), so these variables do not apply to
#cmd("stdout"), #cmd("stderr") and #cmd("stdin").

The variables are kept in the program's storage. A program that changes
one changes it for all its tasks, and for no other program.
