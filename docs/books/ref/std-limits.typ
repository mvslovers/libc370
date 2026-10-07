#import "../bookmaster/bookmaster.typ": *

= \<limits.h\> — Sizes of Integer Types <std-limits>

#idx("limits.h")
#idx("integer types", "limits")
The header #cmd("<limits.h>") defines the ranges of the integer types. Every
macro expands to a constant that can be used in #cmd("#if") directives.

On MVS with cc370, a #cmd("char") is 8 bits and *unsigned*: #cmd("CHAR_MIN")
is 0 and #cmd("CHAR_MAX") is 255. This differs from most C compilers on
workstations. It also means that every character of the EBCDIC code page,
taken from a plain #cmd("char"), is a nonnegative value and can be passed
directly to the functions of #cmd("<ctype.h>") (see @std-ctype). #cmd("int")
and #cmd("long") are both 32 bits, and #cmd("long long") is 64 bits.
#idx("char", "unsigned")

#tab(caption: [Macros defined in \<limits.h\>])[
  #table(columns: (1.25in, 2.3in, 1fr),
    [Macro], [Value on MVS], [Meaning],
    [#cmd("CHAR_BIT")], [8], [bits in a #cmd("char")],
    [#cmd("SCHAR_MIN")], [−128], [minimum of #cmd("signed char")],
    [#cmd("SCHAR_MAX")], [127], [maximum of #cmd("signed char")],
    [#cmd("UCHAR_MAX")], [255], [maximum of #cmd("unsigned char")],
    [#cmd("CHAR_MIN")], [0], [minimum of #cmd("char")],
    [#cmd("CHAR_MAX")], [255], [maximum of #cmd("char")],
    [#cmd("MB_LEN_MAX")], [1], [bytes in a multibyte character, in any
      locale],
    [#cmd("SHRT_MIN")], [−32768], [minimum of #cmd("short")],
    [#cmd("SHRT_MAX")], [32767], [maximum of #cmd("short")],
    [#cmd("USHRT_MAX")], [65535], [maximum of #cmd("unsigned short")],
    [#cmd("INT_MIN")], [−2147483648], [minimum of #cmd("int")],
    [#cmd("INT_MAX")], [2147483647], [maximum of #cmd("int")],
    [#cmd("UINT_MAX")], [4294967295 (#cmd("U"))], [maximum of
      #cmd("unsigned int")],
    [#cmd("LONG_MIN")], [−2147483648 (#cmd("L"))], [minimum of #cmd("long")],
    [#cmd("LONG_MAX")], [2147483647 (#cmd("L"))], [maximum of #cmd("long")],
    [#cmd("ULONG_MAX")], [4294967295 (#cmd("UL"))], [maximum of
      #cmd("unsigned long")],
    [#cmd("LLONG_MIN")], [−9223372036854775808 (#cmd("LL"))], [minimum of
      #cmd("long long")],
    [#cmd("LLONG_MAX")], [9223372036854775807 (#cmd("LL"))], [maximum of
      #cmd("long long")],
    [#cmd("ULLONG_MAX")], [18446744073709551615 (#cmd("ULL"))], [maximum of
      #cmd("unsigned long long")],
  )
] <std-limits-tab>

A suffix in parentheses shows the type of the constant: #cmd("U") is
#cmd("unsigned int"), #cmd("L") is #cmd("long"), and so on. The minimum
values are written as expressions, for example #cmd("(-INT_MAX-1)"), so that
they have the type of the limit they describe\; #cmd("SCHAR_MIN") is the
literal #cmd("-128").

#note[#cmd("<limits.h>") also defines #cmd("UINT16_MAX") and
#cmd("INT32_MAX"), which belong to #cmd("<stdint.h>"). Their values are the
same as there (see @std-stdint). A program that defines these names itself
must not include #cmd("<limits.h>").]
