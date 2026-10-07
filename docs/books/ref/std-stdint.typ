#import "../bookmaster/bookmaster.typ": *

= \<stdint.h\> — Integer Types <std-stdint>

#idx("stdint.h")
#idx("integer types", "exact-width")
The header #cmd("<stdint.h>") defines integer types of a stated width,
macros for their ranges, and macros that write constants of those types.
It declares no function. The macros that #cmd("printf()") and #cmd("scanf()")
need to print and read these types are in #cmd("<inttypes.h>") (see
@std-inttypes).

Every width the standard names exists on MVS: 8, 16, 32 and 64 bits, signed
and unsigned, in two's complement. Note which basic type is behind each:
the 32-bit types are #cmd("long"), not #cmd("int"), while
#cmd("intptr_t") is #cmd("int"). Both are 32 bits wide, but they are
different types to the compiler, and #cmd("-Wformat") holds a program to
the difference: an #cmd("int32_t") is printed with #cmd("%ld"), or with
#cmd("PRId32").

== Integer Types <std-stdint-types>

#tab(caption: [Types defined in \<stdint.h\>])[
  #table(columns: (2.2in, 1.6in, 0.5in),
    [Type], [Defined as], [Bytes],
    [#cmd("int8_t"), #cmd("int_least8_t"), #cmd("int_fast8_t")],
      [#cmd("signed char")], [1],
    [#cmd("uint8_t"), #cmd("uint_least8_t"), #cmd("uint_fast8_t")],
      [#cmd("unsigned char")], [1],
    [#cmd("int16_t"), #cmd("int_least16_t"), #cmd("int_fast16_t")],
      [#cmd("short")], [2],
    [#cmd("uint16_t"), #cmd("uint_least16_t"), #cmd("uint_fast16_t")],
      [#cmd("unsigned short")], [2],
    [#cmd("int32_t"), #cmd("int_least32_t"), #cmd("int_fast32_t")],
      [#cmd("long")], [4],
    [#cmd("uint32_t"), #cmd("uint_least32_t"), #cmd("uint_fast32_t")],
      [#cmd("unsigned long")], [4],
    [#cmd("int64_t"), #cmd("int_least64_t"), #cmd("int_fast64_t")],
      [#cmd("long long")], [8],
    [#cmd("uint64_t"), #cmd("uint_least64_t"), #cmd("uint_fast64_t")],
      [#cmd("unsigned long long")], [8],
    [#cmd("intmax_t")], [#cmd("long long")], [8],
    [#cmd("uintmax_t")], [#cmd("unsigned long long")], [8],
    [#cmd("intptr_t")], [#cmd("int")], [4],
    [#cmd("uintptr_t")], [#cmd("unsigned int")], [4],
  )
] <std-stdint-types-tab>

The least-width and fast types are the exact-width types of the same width.
A #cmd("int_fast8_t") is therefore one byte, although the machine handles a
fullword faster; choose #cmd("int") where speed matters more than size.

#idx("intptr_t")
#cmd("intptr_t") and #cmd("uintptr_t") hold any object pointer converted to
them, and convert back to the same pointer. Only the low 24 bits of the
value address storage; the high byte may carry flags, as in the last
entry of an MVS parameter list.

== Limits <std-stdint-limits>

All limit macros are constant expressions that can be used in #cmd("#if")
directives, and each has the type that an object of its integer type has
after the integer promotions: the limits of the 8-bit and 16-bit types are
#cmd("int") constants, those of the 32-bit types #cmd("long"), those of the
64-bit types #cmd("long long").

#tab(caption: [Limits of the exact-width types])[
  #table(columns: (1.1in, 2.4in, 1fr),
    [Macro], [Value], [Written as],
    [#cmd("INT8_MIN")], [−128], [#cmd("(-127 - 1)")],
    [#cmd("INT8_MAX")], [127], [#cmd("0x7f")],
    [#cmd("UINT8_MAX")], [255], [#cmd("0xff")],
    [#cmd("INT16_MIN")], [−32768], [#cmd("(-32767 - 1)")],
    [#cmd("INT16_MAX")], [32767], [#cmd("0x7fff")],
    [#cmd("UINT16_MAX")], [65535], [#cmd("(65535)")],
    [#cmd("INT32_MIN")], [−2147483648], [#cmd("(-2147483647L - 1)")],
    [#cmd("INT32_MAX")], [2147483647], [#cmd("2147483647L")],
    [#cmd("UINT32_MAX")], [4294967295], [#cmd("(0xffffffffUL)")],
    [#cmd("INT64_MIN")], [−9223372036854775808],
      [#cmd("(-9223372036854775807LL - 1)")],
    [#cmd("INT64_MAX")], [9223372036854775807],
      [#cmd("9223372036854775807LL")],
    [#cmd("UINT64_MAX")], [18446744073709551615],
      [#cmd("18446744073709551615ULL")],
  )
] <std-stdint-exact-tab>

#cmd("INT_LEAST")#var("N")#cmd("_MIN"), #cmd("INT_LEAST")#var("N")#cmd("_MAX"),
#cmd("UINT_LEAST")#var("N")#cmd("_MAX") and the corresponding
#cmd("INT_FAST")... and #cmd("UINT_FAST")... macros have the values of the
exact-width macros of the same #var("N").

#tab(caption: [Limits of the other integer types])[
  #table(columns: (1.4in, 2.4in, 1fr),
    [Macro], [Value], [Defined as],
    [#cmd("INTMAX_MIN")], [−9223372036854775808], [#cmd("INT64_MIN")],
    [#cmd("INTMAX_MAX")], [9223372036854775807], [#cmd("INT64_MAX")],
    [#cmd("UINTMAX_MAX")], [18446744073709551615], [#cmd("UINT64_MAX")],
    [#cmd("INTPTR_MIN")], [−2147483648], [#cmd("INT_MIN")],
    [#cmd("INTPTR_MAX")], [2147483647], [#cmd("INT_MAX")],
    [#cmd("UINTPTR_MAX")], [4294967295], [#cmd("UINT_MAX")],
    [#cmd("PTRDIFF_MIN")], [−2147483648], [#cmd("LONG_MIN")],
    [#cmd("PTRDIFF_MAX")], [2147483647], [#cmd("LONG_MAX")],
    [#cmd("SIZE_MAX")], [4294967295], [#cmd("4294967295UL")],
    [#cmd("SIG_ATOMIC_MIN")], [−2147483648], [#cmd("INT_MIN")],
    [#cmd("SIG_ATOMIC_MAX")], [2147483647], [#cmd("INT_MAX")],
    [#cmd("WCHAR_MIN")], [−2147483648], [#cmd("INT_MIN")],
    [#cmd("WCHAR_MAX")], [2147483647], [#cmd("INT_MAX")],
    [#cmd("WINT_MIN")], [0], [#cmd("0U")],
    [#cmd("WINT_MAX")], [4294967295], [#cmd("UINT_MAX")],
  )
] <std-stdint-other-tab>

The limits of #cmd("ptrdiff_t"), #cmd("size_t") and #cmd("wchar_t")
describe the types of #cmd("<stddef.h>") (see @std-stddef),
#cmd("sig_atomic_t") is the type of #cmd("<signal.h>") (see @std-signal),
and #cmd("wint_t") the type of #cmd("<wchar.h>"), an #cmd("unsigned int").

== Macros for Integer Constants <std-stdint-const>

#idx("INT64_C")
#idx("UINT64_C")
The function-like macros below write an integer constant with the type of
the corresponding least-width type, after promotion. The argument must be a
decimal, octal or hexadecimal constant without a suffix.

#tab(caption: [Macros for integer constants])[
  #table(columns: (1.6in, 1.5in, 1fr),
    [Macro], [Expands to], [Type],
    [#cmd("INT8_C(")#var("v")#cmd(")")], [#var("v")], [#cmd("int")],
    [#cmd("UINT8_C(")#var("v")#cmd(")")], [#var("v")], [#cmd("int")],
    [#cmd("INT16_C(")#var("v")#cmd(")")], [#var("v")], [#cmd("int")],
    [#cmd("UINT16_C(")#var("v")#cmd(")")], [#var("v")], [#cmd("int")],
    [#cmd("INT32_C(")#var("v")#cmd(")")], [#var("v")#cmd("L")], [#cmd("long")],
    [#cmd("UINT32_C(")#var("v")#cmd(")")], [#var("v")#cmd("UL")],
      [#cmd("unsigned long")],
    [#cmd("INT64_C(")#var("v")#cmd(")")], [#var("v")#cmd("LL")],
      [#cmd("long long")],
    [#cmd("UINT64_C(")#var("v")#cmd(")")], [#var("v")#cmd("ULL")],
      [#cmd("unsigned long long")],
    [#cmd("INTMAX_C(")#var("v")#cmd(")")], [#var("v")#cmd("LL")],
      [#cmd("long long")],
    [#cmd("UINTMAX_C(")#var("v")#cmd(")")], [#var("v")#cmd("ULL")],
      [#cmd("unsigned long long")],
  )
] <std-stdint-const-tab>

For example, #cmd("INT64_C(1) << 40") is a #cmd("long long") shift and
yields 1099511627776, where #cmd("1 << 40") would shift a 32-bit
#cmd("int").

== Other Names <std-stdint-other>

#cmd("<stdint.h>") includes #cmd("<stddef.h>"), #cmd("<limits.h>") and
#cmd("<signal.h>"), so every name those headers define is also defined
after #cmd("#include <stdint.h>") -- among them the signal numbers such as
#cmd("SIGINT"). It further defines #cmd("LONG_LONG_MAX"),
#cmd("ULONG_LONG_MAX") and a set of macros beginning with #cmd("PRINTF_")
that hold #cmd("printf()") length modifiers and field widths. These are not
part of the C standard; a portable program does not use them and uses
#cmd("<inttypes.h>") instead.
