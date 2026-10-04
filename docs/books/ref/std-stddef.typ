#import "../bookmaster/bookmaster.typ": *

= \<stddef.h\> — Common Definitions <std-stddef>

#idx("stddef.h")
The header #cmd("<stddef.h>") defines three types and two macros that the
other headers and most programs need. It declares no function.

#tab(caption: [Types defined in \<stddef.h\>])[
  #table(columns: (1.1in, 1.5in, 0.6in, 1fr),
    [Type], [Defined as], [Size], [Use],
    [#cmd("size_t")], [#cmd("unsigned long")], [4], [the type of
      #cmd("sizeof") and of object sizes],
    [#cmd("ptrdiff_t")], [#cmd("long")], [4], [the type of the difference
      of two pointers],
    [#cmd("wchar_t")], [#cmd("int")], [4], [a wide character, the element
      type of a wide string literal #cmd("L\"...\"")],
  )
] <std-stddef-types>

#idx("size_t")
#idx("ptrdiff_t")
#idx("wchar_t")
Each type is the one the compiler itself uses: #cmd("size_t") is the type
of a #cmd("sizeof") expression, #cmd("ptrdiff_t") the type of
#cmd("p - q"), and #cmd("wchar_t") the type of the elements of a wide
string literal. The format modifiers #cmd("%zu") and #cmd("%td") of the
#cmd("printf()") functions therefore agree with them, and #cmd("-Wformat")
checks them without a cast.

Because #cmd("size_t") is #cmd("unsigned long"), not #cmd("unsigned int"),
a program that prints a size with #cmd("%u") gets a warning from
#cmd("-Wformat"). Write #cmd("%zu"), or #cmd("%lu").

#tab(caption: [Macros defined in \<stddef.h\>])[
  #table(columns: (1.6in, 1fr),
    [Macro], [Expansion],
    [#cmd("NULL")], [#cmd("((void *)0)")],
    [#cmd("offsetof(")#var("type")#cmd(",")
      #var("member")#cmd(")")], [the offset in bytes of #var("member")
      from the start of the structure #var("type"), as a #cmd("size_t")
      constant],
  )
] <std-stddef-macros>

#idx("offsetof")
#cmd("offsetof") yields a constant and may be used in the initializer of a
static object. The offsets follow the alignment rules of the compiler: a
#cmd("double") or #cmd("long long") member is aligned on a doubleword, an
#cmd("int"), #cmd("long") or pointer on a fullword. For example, in

```
struct rec { char flag; double amount; int count; };
```

#cmd("offsetof(struct rec, amount)") is 8 and
#cmd("offsetof(struct rec, count)") is 16.

#note[A pointer is 4 bytes, but only the low 24 bits address storage on
MVS 3.8j. #cmd("size_t") and #cmd("ptrdiff_t") are 32-bit types all the
same, so arithmetic on sizes is not restricted to 24 bits.]

#note[The macro #cmd("__PDPCLIB_API__") is also defined by
#cmd("<stddef.h>"). It expands to nothing and is used in the library's own
sources. Programs should not use it.]
