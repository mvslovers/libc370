#import "../bookmaster/bookmaster.typ": *

= \<assert.h\> — Diagnostics <std-assert>

#idx("assert.h")
The header #cmd("<assert.h>") defines the macro #cmd("assert()"), which
checks a condition at run time and ends the program when it does not hold.
Whether the checks are compiled at all is decided by the macro
#cmd("NDEBUG").

== assert <std-assert-assert>

#idx("assert")
#idx("NDEBUG")

=== Format

```
#include <assert.h>

void assert(scalar expression);   /* a macro */
```

=== Description

#cmd("assert()") evaluates #var("expression"). If it is nonzero, nothing
happens. If it is zero, #cmd("assert()") writes a message to #cmd("stderr")
and calls #cmd("abort()"). The message names the expression as it is
written in the source, the source file and the line:

```
assertion failed for statement count > 0 in file payroll.c on line 112
```

If the macro #cmd("NDEBUG") is defined at the point where #cmd("<assert.h>")
is included, #cmd("assert()") expands to #cmd("((void)0)"): the expression is
not evaluated, so it must not contain an effect the program depends on,
such as an assignment or a function call that does work. Define
#cmd("NDEBUG") with the compiler option #cmd("-DNDEBUG") to remove the
checks from a production build.

=== Returns

#cmd("assert()") returns no value.

=== Notes

#idx("assert", "failure on MVS")
- A failed assertion does not abend the program. #cmd("abort()") raises
  #cmd("SIGABRT"), whose default action ends the program through
  #cmd("exit(EXIT_FAILURE)"): functions registered with #cmd("atexit()") run,
  open files are closed, and the program ends with return code 12, the
  value of #cmd("EXIT_FAILURE") on MVS. No dump is taken. In batch the
  return code is the condition code of the job step. See @std-signal for
  how a program can catch #cmd("SIGABRT").
- The message goes to #cmd("stderr"); the _libc370 Programmer's Guide_
  describes where #cmd("stderr") is written in batch and under TSO.
- The message does not contain the name of the function in which the
  assertion failed, which the C99 standard asks for.
- The header is protected against being included twice. Including
  #cmd("<assert.h>") again after defining or undefining #cmd("NDEBUG")
  therefore does not change #cmd("assert()"), contrary to the standard,
  which redefines it at each inclusion. Decide on #cmd("NDEBUG") before the
  first inclusion, preferably on the command line.
- The expansion is a conditional expression with #cmd("void") on one side
  only, which ISO C does not allow; compiled with #cmd("-pedantic"), every
  use of #cmd("assert()") draws the warning #cmd("ISO C forbids conditional
  expr with only one void side"). It is harmless, but it fails a build with
  #cmd("-pedantic-errors").
- The function #cmd("__assert"), which the macro calls, is declared in the
  header. It belongs to the library; programs should not call it directly.

=== Example

```
#include <assert.h>
#include <string.h>

/* copy a member name into an 8-byte field, padded with blanks */
void set_member(char out[8], const char *name)
{
    size_t len = strlen(name);

    assert(len >= 1 && len <= 8);
    memset(out, ' ', 8);
    memcpy(out, name, len);
}
```

=== Related

#cmd("abort()") (see @std-stdlib), #cmd("raise()"), #cmd("signal()") (see
@std-signal).
