#import "../bookmaster/bookmaster.typ": *

= \<setjmp.h\> — Nonlocal Jumps <std-setjmp>

#idx("setjmp.h")
#idx("nonlocal jump")
The header #cmd("<setjmp.h>") defines the type #cmd("jmp_buf") and
declares #cmd("setjmp()") and #cmd("longjmp()"), which let a program return
from a deeply nested call directly to a point it marked earlier, typically
to recover from an error.

#idx("jmp_buf")
A #cmd("jmp_buf") is an array of one structure of 64 bytes:

```
typedef struct {
    int regs[15];   /* general registers 0 to 14 */
    int retval;     /* the value setjmp returns: register 15 */
} jmp_buf[1];
```

The structure holds the general registers of the program at the point of
#cmd("setjmp()"). Register 13 addresses the save area of the calling
function, which is also its frame on the C stack, and register 14 the
instruction after the call, so these two words are what make the jump
possible. The members are described here to show what is saved; a program
should not use them.

== setjmp <std-setjmp-setjmp>

#idx("setjmp")

=== Format

```
#include <setjmp.h>

int __setj(jmp_buf env);
#define setjmp(x) (__setj(x))
```

=== Description

#cmd("setjmp()") saves general registers 0 to 14 in #var("env"), for a later
#cmd("longjmp()") to return to this point.

#cmd("setjmp()") is a macro that calls the library routine #cmd("__setj").
As the standard requires, use it only as the whole controlling expression
of an #cmd("if"), #cmd("switch") or loop statement, compared with a
constant, negated, or as a statement of its own.

=== Returns

0 when called directly. When control returns here through
#cmd("longjmp()"), the value given to #cmd("longjmp()"), which is never 0.

=== Notes

- The floating-point registers are not saved.
- Since #cmd("setjmp()") is a macro, it cannot be called through a pointer,
  and #cmd("&setjmp") does not compile.

=== Related

@std-setjmp-longjmp.

== longjmp <std-setjmp-longjmp>

#idx("longjmp")

=== Format

```
#include <setjmp.h>

void longjmp(jmp_buf env, int val);
```

=== Description

#cmd("longjmp()") restores the registers that #cmd("setjmp()") saved in
#var("env") and continues the program as if that #cmd("setjmp()") had just
returned, with the value #var("val"). If #var("val") is 0, #cmd("setjmp()")
returns 1.

The function that called #cmd("setjmp()") must not have returned in the
meantime: its frame on the stack, which register 13 points to, would no
longer be valid, and the program would continue on storage that is in use
for something else. The library does not check this.

=== Returns

#cmd("longjmp()") does not return.

=== Notes

#idx("longjmp", "local variables after")
- The registers are set back to their values at the time of
  #cmd("setjmp()"). A local variable of the function that called
  #cmd("setjmp()") that the compiler kept in a register therefore has its old
  value, while one kept in storage has its new value. Which is which
  depends on the optimization level. Declare such a variable
  #cmd("volatile") if it is changed between #cmd("setjmp()") and
  #cmd("longjmp()") and used afterwards; the standard says the same.
- #cmd("longjmp()") only restores registers. It does not close files, free
  storage, or cancel MVS resources obtained by the functions it leaves,
  such as an #cmd("ENQ") or an ESTAE environment.
- A #cmd("jmp_buf") belongs to the task that filled it. Do not use it from
  another task.

=== Example

@std-setjmp-ex is a small parser for sums such as #cmd("12+30"). The
function #cmd("number"), two calls deep, returns to #cmd("parse") with
#cmd("longjmp()") when it meets a character that is not a digit.

#fig(caption: [Leaving nested calls with longjmp])[
  #code(read("../ex/std-setjmp/parse.c"), numbers: true)
] <std-setjmp-ex>

=== Related

@std-setjmp-setjmp.
