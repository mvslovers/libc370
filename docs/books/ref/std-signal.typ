#import "../bookmaster/bookmaster.typ": *

= \<signal.h\> — Signal Handling <std-signal>

#idx("signal.h")
#idx("signals")
The header #cmd("<signal.h>") declares #cmd("signal()"), which installs a
function to handle a signal, and #cmd("raise()"), which sends a signal to
the program.

== Signals on MVS <std-signal-mvs>

#idx("signals", "on MVS")
#idx("abend", "and signals")
MVS has no signals. LIBC/370 implements the interface of the C standard
with a table of handlers and nothing more: *a signal occurs only when the
program calls #cmd("raise()")*, or #cmd("abort()"), which raises
#cmd("SIGABRT"). No event of the system is turned into a signal:

- A program check -- a protection exception (S0C4), a data exception
  (S0C7), a divide exception (S0C9) and so on -- is an abend. It does not
  raise #cmd("SIGSEGV"), #cmd("SIGFPE") or #cmd("SIGILL"). A handler for
  those signals is called only by #cmd("raise()").
- The attention key of a TSO terminal does not raise #cmd("SIGINT"), and
  an operator's #cmd("STOP") or #cmd("CANCEL") command does not raise
  #cmd("SIGTERM").

To recover from an abend, a program uses the ESTAE support of the library
instead: the function #cmd("try()") and the macros of
#cmd("<mvs/recovery.h>"), described in @mvs-program-recovery.

#idx("SIGABRT")
The six signals of the C standard are defined. The handlers are kept in a
table in storage of the C run time that is shared by all tasks of the
program in the address space: a handler installed by one task is in force
for all of them.

#tab(caption: [Macros defined in \<signal.h\>])[
  #table(columns: (1.1in, 0.6in, 1fr),
    [Macro], [Value], [Meaning],
    [#cmd("SIGABRT")], [1], [abnormal termination, raised by
      #cmd("abort()")],
    [#cmd("SIGFPE")], [2], [arithmetic error],
    [#cmd("SIGILL")], [3], [invalid instruction],
    [#cmd("SIGINT")], [4], [interactive attention],
    [#cmd("SIGSEGV")], [5], [invalid storage access],
    [#cmd("SIGTERM")], [6], [request to end],
    [#cmd("SIG_DFL")], [], [the default action],
    [#cmd("SIG_IGN")], [], [ignore the signal],
    [#cmd("SIG_ERR")], [], [the error return of #cmd("signal()")],
  )
] <std-signal-macros>

#idx("sig_atomic_t")
#cmd("SIG_DFL"), #cmd("SIG_IGN") and #cmd("SIG_ERR") are the addresses of
three functions of the library, #cmd("__sigdfl"), #cmd("__sigign") and
#cmd("__sigerr"), each of type #cmd("void (*)(int)"). The header also
defines the type #cmd("sig_atomic_t") as #cmd("int").

#idx("SIG_DFL", "action")
The default action, #cmd("SIG_DFL"), depends on the signal:

#deflist(width: 1.1in,
  [#cmd("SIGABRT")], [ends the program with #cmd("exit(EXIT_FAILURE)"),
    return code 12. Functions registered with #cmd("atexit()") run and open
    files are closed. The program does not abend.],
  [all others], [nothing: #cmd("raise()") returns and the program
    continues.],
)

#cmd("SIG_IGN") does nothing for every signal.

== signal <std-signal-signal>

#idx("signal")

=== Format

```
#include <signal.h>

void (*signal(int sig, void (*func)(int)))(int);
```

=== Description

#cmd("signal()") installs #var("func") as the handler of the signal
#var("sig"). #var("func") is #cmd("SIG_DFL"), #cmd("SIG_IGN"), or the
address of a function that takes the signal number as its argument. The
handler stays installed until #cmd("signal()") is called again for the same
signal; it is not reset to #cmd("SIG_DFL") when the signal occurs.

=== Returns

#var("func"), the handler just installed.

=== Notes

- *The previous handler is not returned.* The C standard requires
  #cmd("signal()") to return the handler that was in force before the call,
  so that a program can restore it later. LIBC/370 returns #var("func")
  instead. A program that wants to restore an earlier handler must keep
  it itself.
- *An invalid signal number is not reported.* For a #var("sig") outside 1
  to 6, #cmd("signal()") installs nothing and returns #var("func"), not
  #cmd("SIG_ERR"), and does not set #cmd("errno"). #cmd("signal()") never
  returns #cmd("SIG_ERR").

=== Example

The program in @std-signal-ex writes a closing line to its report when it
ends through #cmd("abort()"). After #cmd("on_abort") returns,
#cmd("abort()") ends the program with return code 12.

#fig(caption: [Handling SIGABRT])[
  #code(read("../ex/std-signal/cleanup.c"), numbers: true)
] <std-signal-ex>

=== Related

@std-signal-raise, #cmd("abort()") (see @std-stdlib).

== raise <std-signal-raise>

#idx("raise")

=== Format

```
#include <signal.h>

int raise(int sig);
```

=== Description

#cmd("raise()") sends the signal #var("sig") to the program: it calls the
handler installed for #var("sig"), or performs the default action, and
returns when the handler returns. The handler runs on the task that called
#cmd("raise()").

=== Returns

0. #cmd("raise()") also returns 0 for a #var("sig") outside 1 to 6, for which
it does nothing; the standard asks for a nonzero value in that case.

=== Notes

#cmd("raise(SIGABRT)") with the default action does not return: it ends
the program as described in @std-signal-mvs.

=== Related

@std-signal-signal.
