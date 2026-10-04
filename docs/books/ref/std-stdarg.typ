#import "../bookmaster/bookmaster.typ": *

= \<stdarg.h\> — Variable Arguments <std-stdarg>

#idx("stdarg.h")
#idx("variable arguments")
The header #cmd("<stdarg.h>") defines a type and four macros with which a
function that takes a variable number of arguments, declared with
#cmd("..."), reads the arguments that follow its last named parameter.

== The Parameter List <std-stdarg-plist>

#idx("parameter list")
A C function compiled by cc370 receives its arguments in a parameter list
addressed by register 1. The arguments lie in the list one after the other,
in the order of the call, each taking the size of its type after the
default argument promotions, with no padding between them:

#deflist(width: 1.6in,
  [4 bytes], [#cmd("int") (and the #cmd("char") and #cmd("short") promoted
    to it), #cmd("long"), the unsigned types of these sizes, pointers],
  [8 bytes], [#cmd("double") (and the #cmd("float") promoted to it),
    #cmd("long double"), #cmd("long long"), #cmd("unsigned long long")],
)

A call #cmd("f(1, 5LL, 2, 1.5f)"), for example, builds a list of 24 bytes:
the #cmd("int") 1 at offset 0, the #cmd("long long") at 4, the #cmd("int")
2 at 12 and the #cmd("double") 1.5 at 16.

The macros of #cmd("<stdarg.h>") walk this list with a character pointer.
They are plain macros, not built into the compiler.

== va_list, va_start, va_arg, va_end, va_copy <std-stdarg-va>

#idx("va_list")
#idx("va_start")
#idx("va_arg")
#idx("va_end")
#idx("va_copy")

=== Format

```
#include <stdarg.h>

typedef char *va_list;

#define va_start(ap, parmN) ap = (char *)&parmN + 4
#define va_arg(ap, type) \
        *(type *)(ap += sizeof(type), ap - sizeof(type))
#define va_end(ap) ap = 0
#define va_copy(dest, src) ((dest) = (src))
```

=== Description

#deflist(width: 1.1in,
  [#cmd("va_list")], [the type of an object that holds the position in the
    argument list. It is a #cmd("char") pointer.],
  [#cmd("va_start()")], [sets #var("ap") to the first argument after the
    named parameter #var("parmN"), which must be the last named parameter
    of the function. It is called once before the first #cmd("va_arg()").],
  [#cmd("va_arg()")], [returns the next argument as a value of type
    #var("type") and advances #var("ap") past it.],
  [#cmd("va_end()")], [ends the use of #var("ap"). Call it before the
    function returns, for every #cmd("va_start()") and #cmd("va_copy()").],
  [#cmd("va_copy()")], [makes #var("dest") a copy of #var("src"), at the same
    position, so that the remaining arguments can be read twice.],
)

A #cmd("va_list") can be passed to another function, such as
#cmd("vprintf()"), which then reads the arguments. The caller must not use
the #cmd("va_list") again afterwards except to call #cmd("va_end()").

=== Notes

#idx("va_start", "restrictions")
The macros compute positions from the sizes of the types and do not check
anything. A mistake does not fail; it reads the wrong bytes and the program
continues with wrong values. Observe these rules:

- *The last named parameter must be a 4-byte type*: #cmd("int"),
  #cmd("long"), an unsigned type of that size, or a pointer.
  #cmd("va_start()") assumes the next argument starts 4 bytes after it.
  - If it is a #cmd("double") or a #cmd("long long"), #cmd("va_start()")
    points into the middle of it, and every #cmd("va_arg()") reads 4 bytes
    early. This is a restriction of libc370; the C standard allows these
    types.
  - If it is a #cmd("char"), #cmd("short") or #cmd("float"), the standard
    leaves the behavior undefined. For #cmd("char") and #cmd("short") the
    function works on a copy of the parameter in its own frame, and
    #cmd("va_start()") points into that frame, not into the argument list. A
    #cmd("float") is passed as a #cmd("double") and fails as a
    #cmd("double") does.

  When a function must take a #cmd("double") as its last named parameter,
  add a parameter after it, such as a count, or pass the #cmd("double")
  among the variable arguments.
- *#var("type") must be a promoted type.* Use #cmd("int") for an argument
  passed as #cmd("char") or #cmd("short"), and #cmd("double") for one
  passed as #cmd("float"). #cmd("va_arg(ap, char)") advances by one byte
  and reads the first byte of the #cmd("int"), which on this machine is its
  high-order byte. The standard leaves this undefined too.
- #var("type") must be a type name that becomes a pointer type when
  followed by #cmd("*"); for a pointer to a function, define a
  #cmd("typedef") first.
- #var("parmN") must not be declared #cmd("register"), since its address is
  taken.

=== Example

@std-stdarg-ex shows a function that passes its arguments on to
#cmd("vprintf()"), and one that reads them itself.

#fig(caption: [Functions with variable arguments])[
  #code(read("../ex/std-stdarg/msg.c"), numbers: true)
] <std-stdarg-ex>

=== Related

#cmd("vprintf()"), #cmd("vfprintf()"), #cmd("vsprintf()"), #cmd("vsnprintf()")
(see @std-stdio).
