#import "../bookmaster/bookmaster.typ": *

= \<float.h\> — Floating-Point Characteristics <std-float>

#idx("float.h")
#idx("hexadecimal floating point")
#idx("floating point", "characteristics")
The header #cmd("<float.h>") describes the floating-point types: their
base, precision and range. It declares no function.

== Floating Point on System/370 <std-float-hfp>

#idx("HFP")
System/370 has hexadecimal floating point (HFP), not the binary IEEE
format of most other machines. A number is a fraction of hexadecimal
digits multiplied by a power of 16:

- #cmd("float") is the short format, 4 bytes: a sign bit, a 7-bit
  exponent of 16, and a fraction of 6 hexadecimal digits (24 bits).
- #cmd("double") is the long format, 8 bytes: the same sign and exponent,
  and a fraction of 14 hexadecimal digits (56 bits).
- #cmd("long double") is the same as #cmd("double"), 8 bytes. The extended
  format of the machine is not used by the compiler.

Both formats have the same exponent range, so a #cmd("float") reaches as
far as a #cmd("double"): about #cmd("5.4E-79") to #cmd("7.2E+75"). There is
no infinity and no NaN; the compiler's #cmd("__FLT_HAS_INFINITY__") and
#cmd("__FLT_HAS_QUIET_NAN__") are 0, and so are those for the other types.

#idx("wobbling precision")
Because the fraction is normalized to a hexadecimal digit, not to a bit,
the leading digit may begin with up to three zero bits. The precision
therefore varies with the value: a #cmd("float") holds 21 to 24
significant bits, a #cmd("double") 53 to 56. #cmd("FLT_DIG") and
#cmd("DBL_DIG") give the number of decimal digits that survive in the worst
case.

== Macros <std-float-macros>

#tab(caption: [Integer characteristics in \<float.h\>])[
  #table(columns: (1.45in, 0.6in, 1fr),
    [Macro], [Value], [Meaning],
    [#cmd("FLT_RADIX")], [16], [base of the exponent],
    [#cmd("FLT_ROUNDS")], [0], [rounding of addition: toward zero],
    [#cmd("FLT_EVAL_METHOD")], [0], [each operation is evaluated in the
      type of its operands],
    [#cmd("DECIMAL_DIG")], [18], [decimal digits needed to write any
      #cmd("long double") and read it back unchanged],
    [#cmd("FLT_MANT_DIG")], [6], [digits of base 16 in the fraction of a
      #cmd("float")],
    [#cmd("DBL_MANT_DIG")], [14], [the same for #cmd("double")],
    [#cmd("LDBL_MANT_DIG")], [14], [the same for #cmd("long double")],
    [#cmd("FLT_DIG")], [6], [decimal digits a #cmd("float") preserves],
    [#cmd("DBL_DIG")], [15], [decimal digits a #cmd("double") preserves],
    [#cmd("LDBL_DIG")], [15], [the same for #cmd("long double")],
    [#cmd("FLT_MIN_EXP")], [−64], [smallest #var("e") for which
      16#super[#var("e")−1] is a normalized #cmd("float")],
    [#cmd("DBL_MIN_EXP")], [−64], [the same for #cmd("double")],
    [#cmd("LDBL_MIN_EXP")], [−64], [the same for #cmd("long double")],
    [#cmd("FLT_MAX_EXP")], [63], [largest #var("e") for which
      16#super[#var("e")−1] is a #cmd("float")],
    [#cmd("DBL_MAX_EXP")], [63], [the same for #cmd("double")],
    [#cmd("LDBL_MAX_EXP")], [63], [the same for #cmd("long double")],
    [#cmd("FLT_MIN_10_EXP")], [−78], [smallest power of 10 that is a
      normalized #cmd("float")],
    [#cmd("DBL_MIN_10_EXP")], [−78], [the same for #cmd("double")],
    [#cmd("LDBL_MIN_10_EXP")], [−78], [the same for #cmd("long double")],
    [#cmd("FLT_MAX_10_EXP")], [75], [largest power of 10 that is a
      #cmd("float")],
    [#cmd("DBL_MAX_10_EXP")], [75], [the same for #cmd("double")],
    [#cmd("LDBL_MAX_10_EXP")], [75], [the same for #cmd("long double")],
  )
] <std-float-int>

All macros in @std-float-int are integer constants and can be used in
#cmd("#if") directives.

#idx("FLT_DIG")
With a fraction of 6 hexadecimal digits a #cmd("float") preserves
floor((6 − 1) × log#sub[10] 16) = 6 decimal digits in the worst case, a
#cmd("double") with 14 digits floor(13 × log#sub[10] 16) = 15.

#tab(caption: [Floating characteristics in \<float.h\>])[
  #table(columns: (1.15in, 2.2in, 0.6in, 1fr),
    [Macro], [Value], [Type], [Meaning],
    [#cmd("FLT_MAX")], [7.23700515E+75], [#cmd("float")], [largest
      #cmd("float")],
    [#cmd("DBL_MAX")], [7.23700557733226211E+75], [#cmd("double")],
      [largest #cmd("double")],
    [#cmd("LDBL_MAX")], [7.23700557733226211E+75], [#cmd("long double")],
      [the same for #cmd("long double")],
    [#cmd("FLT_MIN")], [5.39760535E−79], [#cmd("float")], [smallest
      normalized #cmd("float"), 16#super[−65]],
    [#cmd("DBL_MIN")], [5.39760534693402789E−79], [#cmd("double")],
      [smallest normalized #cmd("double"), 16#super[−65]],
    [#cmd("LDBL_MIN")], [5.39760534693402789E−79], [#cmd("long double")],
      [the same for #cmd("long double")],
    [#cmd("FLT_EPSILON")], [9.53674316E−07], [#cmd("float")], [difference
      between 1 and the next #cmd("float"), 16#super[−5]],
    [#cmd("DBL_EPSILON")], [2.22044604925031308E−16], [#cmd("double")],
      [difference between 1 and the next #cmd("double"), 16#super[−13]],
    [#cmd("LDBL_EPSILON")], [2.22044604925031308E−16], [#cmd("long double")],
      [the same for #cmd("long double")],
  )
] <std-float-flt>

#idx("FLT_MAX", "constant")#idx("__DBL_MAX__")
The macros in @std-float-flt are constant expressions of the type shown, as
C99 requires: they expand to the values cc370 predefines,
#cmd("__FLT_MAX__"), #cmd("__DBL_MAX__") and so on, and can initialize an
object of static storage duration. The decimal values in the table are
those literals; each converts to exactly the bit pattern of the
hexadecimal value named in the last column.

```
#include <float.h>

static const double big = DBL_MAX;
```

#note[Up to release 2.20.0 these macros were members
 of union objects in
the library (#cmd("FLT_MAX") was #cmd("_FltMax._Fval")): not constant
expressions, #cmd("DBL_MIN") and #cmd("DBL_EPSILON") of type
#cmd("float"), and #cmd("LDBL_MAX") the #cmd("long") 2147483647. The
objects are still in #cmd("libc.a") for programs compiled with those
headers.]

#cmd("FLT_ROUNDS") is 0: hexadecimal floating point truncates, so addition
rounds toward zero. The library has no function that changes the rounding.

