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
    [#cmd("FLT_DIG")], [7], [decimal digits a #cmd("float") preserves; see
      the note below],
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
#note[#cmd("FLT_DIG") is 7 in the header, but a #cmd("float") does not
preserve 7 decimal digits. With a fraction of 6 hexadecimal digits the
guaranteed number is floor((6 − 1) × log#sub[10] 16) = 6, and the compiler's
own #cmd("__FLT_DIG__") is 6. Use 6.]

#tab(caption: [Floating characteristics in \<float.h\>])[
  #table(columns: (1.15in, 2.2in, 0.6in, 1fr),
    [Macro], [Value], [Type], [Meaning],
    [#cmd("FLT_MAX")], [7.23700515E+75], [#cmd("float")], [largest
      #cmd("float")],
    [#cmd("DBL_MAX")], [7.23700557733226211E+75], [#cmd("double")],
      [largest #cmd("double")],
    [#cmd("LDBL_MAX")], [2147483647 (wrong)], [#cmd("long")], [see the
      note below],
    [#cmd("FLT_MIN")], [5.39760535E−79], [#cmd("float")], [smallest
      normalized #cmd("float"), 16#super[−65]],
    [#cmd("DBL_MIN")], [5.39760535E−79], [#cmd("float")], [smallest
      normalized #cmd("double"), 16#super[−65]],
    [#cmd("LDBL_MIN")], [5.39760535E−79], [#cmd("float")], [the same for
      #cmd("long double")],
    [#cmd("FLT_EPSILON")], [9.53674316E−07], [#cmd("float")], [difference
      between 1 and the next #cmd("float"), 16#super[−5]],
    [#cmd("DBL_EPSILON")], [2.22044604925031308E−16], [#cmd("float")],
      [difference between 1 and the next #cmd("double"), 16#super[−13]],
    [#cmd("LDBL_EPSILON")], [2.22044604925031308E−16], [#cmd("float")],
      [the same for #cmd("long double")],
  )
] <std-float-flt>

#idx("FLT_MAX", "not a constant")
The macros in @std-float-flt are not constants. Each expands to a member of
a union object in the library, which holds the bit pattern of the value:
#cmd("FLT_MAX"), for example, is #cmd("_FltMax._Fval"). This has three
consequences, all of which differ from the C standard:

- *They are not constant expressions.* They cannot initialize an object of
  static storage duration: #cmd("static double big = DBL_MAX;") is
  rejected with #cmd("initializer element is not constant"). Nor can they be
  used where the compiler needs a constant, such as a #cmd("case") label
  or an array size.
- *Several have the wrong type.* #cmd("DBL_MIN"), #cmd("DBL_EPSILON") and
  the #cmd("LDBL_") minimum and epsilon are #cmd("float") objects. Their
  values are exact in a #cmd("float") and become the same #cmd("double")
  when promoted, so arithmetic with them is correct, but
  #cmd("sizeof(DBL_EPSILON)") is 4.
- *#cmd("LDBL_MAX") is wrong.* It reads the first four bytes of the largest
  #cmd("double") as a #cmd("long"), and yields the integer 2147483647
  instead of 7.2E+75.

#idx("__DBL_MAX__")
Where a constant is needed, or for #cmd("LDBL_MAX"), use the macros that
cc370 itself predefines. They have the same values, as constants of the
right type: #cmd("__FLT_MAX__"), #cmd("__DBL_MAX__"), #cmd("__LDBL_MAX__"),
#cmd("__FLT_MIN__"), #cmd("__DBL_MIN__"), #cmd("__LDBL_MIN__"),
#cmd("__FLT_EPSILON__"), #cmd("__DBL_EPSILON__") and
#cmd("__LDBL_EPSILON__").

```
#include <float.h>

static const double big = __DBL_MAX__;   /* DBL_MAX is not a constant */
```

#note[#cmd("FLT_ROUNDS") is the constant 0, which in C means rounding
toward zero. The comment beside it in the header calls the rounding
direction unpredictable, for which C has the value −1. The library has no
function that changes the rounding.]
