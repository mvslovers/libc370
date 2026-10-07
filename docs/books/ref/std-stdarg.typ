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
    to it), #cmd("long"), the unsigned types of these sizes, pointers, and
    a #cmd("float") passed to a parameter declared #cmd("float") in a
    prototype],
  [8 bytes], [#cmd("double") (and a #cmd("float") among the variable
    arguments, which is promoted to it), #cmd("long double"),
    #cmd("long long"), #cmd("unsigned long long")],
)

A call #cmd("f(1, 5LL, 2, 1.5f)"), for example, builds a list of 24 bytes:
the #cmd("int") 1 at offset 0, the #cmd("long long") at 4, the #cmd("int")
2 at 12 and the #cmd("double") 1.5 at 16.

The macros of #cmd("<stdarg.h>") are built into cc370
(#cmd("__builtin_va_start") and its companions), so the compiler, which
knows this layout, computes the position of every argument. They are
correct whatever the type of the last named parameter, #cmd("double") and
#cmd("long long") included.

== va_list, va_start, va_arg, va_end, va_copy <std-stdarg-va>

#idx("va_list")
#idx("va_start")
#idx("va_arg")
#idx("va_end")
#idx("va_copy")

=== Format

```
#include <stdarg.h>

typedef __builtin_va_list va_list;

#define va_start(ap, parmN)  __builtin_va_start(ap, parmN)
#define va_arg(ap, type)     __builtin_va_arg(ap, type)
#define va_end(ap)           __builtin_va_end(ap)
#define va_copy(dest, src)   __builtin_va_copy(dest, src)
```

=== Description

#deflist(width: 1.1in,
  [#cmd("va_list")], [the type of an object that holds the position in the
    argument list. It is a #cmd("char") pointer, 4 bytes long.],
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

#idx("va_arg", "promoted types")
- *#var("type") must be a promoted type.* Use #cmd("int") for an argument
  passed as #cmd("char") or #cmd("short"), and #cmd("double") for one
  passed as #cmd("float"). For #cmd("va_arg(ap, char)") cc370 warns that
  #cmd("char") is promoted to #cmd("int") and compiles a call of
  #cmd("abort()") in place of the read: the program ends when it reaches
  it.
- The last named parameter may have any type. C99 leaves the behavior
  undefined for a #cmd("char"), #cmd("short") or #cmd("float") last
  parameter\; cc370 handles these correctly too, but a portable program
  avoids them.
- #cmd("va_copy()") is defined unless the program is compiled with
  #cmd("-ansi") or #cmd("-std=c89")\; #cmd("__va_copy()") is always
  defined.
- The macros check nothing at run time. A #var("type") that does not match
  the argument passed, or a #cmd("va_arg()") past the last argument, reads
  the wrong bytes, and the program continues with wrong values.

=== Example

@std-stdarg-ex shows a function that passes its arguments on to
#cmd("vprintf()"), and one that reads them itself.

#fig(caption: [Functions with variable arguments])[
  #code(read("../ex/std-stdarg/msg.c"), numbers: true)
] <std-stdarg-ex>

=== Related

#cmd("vprintf()"), #cmd("vfprintf()"), #cmd("vsprintf()"), #cmd("vsnprintf()")
(see @std-stdio).
