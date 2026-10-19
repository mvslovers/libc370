#import "../bookmaster/bookmaster.typ": *

= \<math.h\> — Mathematics <std-math>

#idx("math.h")
The header #cmd("<math.h>") declares the mathematical functions of C90, all
for #cmd("double"), their #cmd("long double") variants from C99, and defines
the macro #cmd("HUGE_VAL"). This chapter
first describes the floating-point arithmetic of System/370, which differs
from the IEEE arithmetic that most C programs are written for, and then
the functions.

== Floating Point on System/370 <std-math-hfp>

#idx("hexadecimal floating point")#idx("floating point", "System/370")
System/370 has no IEEE floating point. Its floating-point numbers are
_hexadecimal_ (HFP): a sign bit, a 7-bit exponent of 16 with a bias of 64,
and a fraction of 6 hexadecimal digits for #cmd("float") (4 bytes) or 14
for #cmd("double") (8 bytes). #cmd("long double") is the same as
#cmd("double"). The details and the values of the #cmd("<float.h>") macros
are in @std-float. The consequences for a program are these:

- *Range.* A #cmd("double") holds magnitudes from about 5.4E-79 to about
  7.2E75\; a #cmd("float") has the same range. IEEE doubles reach 1.8E308.
- *Precision.* The fraction is normalized to a nonzero leading
  #emph[hexadecimal] digit, so the precision "wobbles" between 53 and 56
  bits for a #cmd("double") and between 21 and 24 bits for a #cmd("float"),
  depending on the value. A #cmd("double") gives about 15 decimal digits.
- *No infinity and no NaN.* Every bit pattern is a number. There is no
  #cmd("INFINITY"), no #cmd("NAN") and no #cmd("isnan()") or
  #cmd("isinf()").
- *Overflow ends the program.* A result too large for the format causes an
  exponent-overflow program interruption, abend #cmd("S0CC"), which no
  program mask suppresses. A floating-point division by zero causes a
  floating-point-divide interruption, abend #cmd("S0CF"). Nothing returns an
  infinity instead.
- *#cmd("HUGE_VAL") is a finite number*: 9.999999999999999999999E72, which is
  less than #cmd("DBL_MAX"). #cmd("acos()"), #cmd("asin()") and #cmd("log()")
  return it on a domain error (#cmd("asin()") of a value below -1 returns
  #cmd("-HUGE_VAL")), #cmd("exp()") and #cmd("pow()") on overflow, and
  #cmd("strtod()") on overflow.
#idx("HUGE_VAL")#idx("S0CC")#idx("S0CF")

== Error Reporting <std-math-errors>

#idx("errno", "mathematical functions")#idx("EDOM")
A function whose argument lies outside its domain stores #cmd("EDOM") in
#cmd("errno") and returns the value given in its entry. #cmd("exp()"),
#cmd("pow()") and #cmd("log(0)") store #cmd("ERANGE") for a result that is
too large or too small, or a pole, and return #cmd("HUGE_VAL") (or
#cmd("-HUGE_VAL")) or 0. The other functions do not check the range: a
result that is too large ends the program as described above.
To detect an error, set #cmd("errno") to 0 before the call and test it
afterwards. The C99 macro #cmd("math_errhandling") is not defined.

== The long double Functions <std-math-long>

#idx("long double", "mathematical functions")#idx("sqrtl")
C99 gives each of the 22 functions a #cmd("long double") variant, named with
a trailing #cmd("l"): #cmd("acosl()"), #cmd("asinl()"), #cmd("atanl()"),
#cmd("atan2l()"), #cmd("ceill()"), #cmd("cosl()"), #cmd("coshl()"),
#cmd("expl()"), #cmd("fabsl()"), #cmd("floorl()"), #cmd("fmodl()"),
#cmd("frexpl()"), #cmd("ldexpl()"), #cmd("logl()"), #cmd("log10l()"),
#cmd("modfl()"), #cmd("powl()"), #cmd("sinl()"), #cmd("sinhl()"),
#cmd("sqrtl()"), #cmd("tanl()") and #cmd("tanhl()"). Each has the
prototype of its #cmd("double") function with #cmd("long double") in place
of #cmd("double"), for example

```
long double sqrtl(long double x);
long double frexpl(long double value, int *exp);
long double modfl(long double value, long double *iptr);
```

#cmd("long double") is #cmd("double") on this target (@std-float), so
#cmd("<math.h>") declares each variant as another name of its
#cmd("double") function: #cmd("sqrtl(x)") calls #cmd("sqrt()") and returns
exactly what #cmd("sqrt(x)") returns, with the same errors. The entries of
this chapter apply to both. (Up to LIBC/370 2.23.0 the variants did not
exist.)

The two share one external name on MVS (#cmd("SQRT")), which is why
#cmd("<math.h>") declares the #cmd("double") function with that name as an
#cmd("asm") label as well. A program that declares #cmd("sqrt()") itself
and calls it before it includes #cmd("<math.h>") gets the warning that two
external symbols map to one MVS name. Include #cmd("<math.h>") first, or do
not declare the function yourself.

== What Is Not Provided <std-math-missing>

LIBC/370 provides only the 22 functions described in this chapter and
their #cmd("long double") variants. None of the other functions that C99
added to #cmd("<math.h>") exists: there are no #cmd("float") variants
(#cmd("sinf()"), #cmd("sqrtf()") ...), and no #cmd("round()"), #cmd("trunc()"),
#cmd("hypot()"), #cmd("cbrt()"), #cmd("log2()"), #cmd("exp2()"),
#cmd("expm1()"), #cmd("log1p()"), #cmd("copysign()"), #cmd("fmin()"),
#cmd("fmax()"), #cmd("nan()") or the classification macros
#cmd("fpclassify()"), #cmd("isfinite()") and #cmd("signbit()"). The types
#cmd("float_t") and #cmd("double_t") and the macros #cmd("HUGE_VALF"),
#cmd("HUGE_VALL"), #cmd("INFINITY") and #cmd("NAN") are not defined, and the
headers #cmd("<fenv.h>"), #cmd("<complex.h>") and #cmd("<tgmath.h>") do not
exist. A call of a missing function is an implicit declaration, which the
compiler reports.

== Accuracy and the Compiler <std-math-accuracy>

#idx("mathematical functions", "accuracy")
The functions are written in C and compute their results from power series.
For arguments of moderate size they are accurate to nearly the full
precision of a #cmd("double"). Each entry states where they are not.

The compiler evaluates some calls itself. #cmd("fabs()") always becomes the
#cmd("LOAD POSITIVE") instruction (#cmd("LPDR")) and never calls the
library, and calls of #cmd("floor()"), #cmd("ceil()"), #cmd("sqrt()") and
#cmd("pow()") with constant arguments are replaced by their value at
compile time. To make sure that the library function is called -- in a test,
for example -- pass a variable, or compile with #cmd("-fno-builtin").

== acos, asin <std-math-acos>

#idx("acos")#idx("asin")

=== Format

```
#include <math.h>

double acos(double x);
double asin(double x);
```

=== Description

#cmd("acos()") computes the principal value of the arc cosine of #var("x"),
#cmd("asin()") that of the arc sine.

=== Returns

#cmd("acos()") returns a value in the range 0 to #sym.pi radians,
#cmd("asin()") a value in the range -#sym.pi/2 to +#sym.pi/2. If #var("x") is
outside the range -1 to +1, #cmd("acos()") returns #cmd("HUGE_VAL"), and
#cmd("asin()") returns #cmd("HUGE_VAL") above +1 and #cmd("-HUGE_VAL")
below -1.

=== Errors

#deflist(width: 1.1in,
  [#cmd("EDOM")], [#var("x") is less than -1 or greater than +1.],
)

=== Related

#cmd("atan()") (@std-math-atan), #cmd("cos()") (@std-math-cos).

== atan, atan2 <std-math-atan>

#idx("atan")#idx("atan2")

=== Format

```
#include <math.h>

double atan(double x);
double atan2(double y, double x);
```

=== Description

#cmd("atan()") computes the principal value of the arc tangent of #var("x").
#cmd("atan2()") computes the arc tangent of #var("y")/#var("x"), using the
signs of both arguments to determine the quadrant.

=== Returns

#cmd("atan()") returns a value in the range -#sym.pi/2 to +#sym.pi/2,
#cmd("atan2()") a value in the range -#sym.pi to +#sym.pi radians.

=== Notes

#cmd("atan2(0.0, 0.0)") returns 0. Up to release 2.22.0 it divided zero by
zero and ended the program with abend #cmd("S0CF").

=== Related

#cmd("acos()") and #cmd("asin()") (@std-math-acos), #cmd("tan()")
(@std-math-cos).

== ceil, floor <std-math-ceil>

#idx("ceil")#idx("floor")

=== Format

```
#include <math.h>

double ceil(double x);
double floor(double x);
```

=== Description

#cmd("ceil()") computes the smallest integral value not less than #var("x"),
#cmd("floor()") the largest integral value not greater than #var("x").

=== Returns

The integral value, as a #cmd("double").

=== Notes

The result is exact for every #cmd("double"), including values beyond the
range of #cmd("int") and #cmd("long long"). From 2#super[52] upward every
#cmd("double") is integral and is returned unchanged.

=== Example

#code(read("../ex/std-math/split.c"))

=== Related

#cmd("modf()") (@std-math-modf), #cmd("fmod()") (@std-math-fmod).

== cos, sin, tan <std-math-cos>

#idx("cos")#idx("sin")#idx("tan")

=== Format

```
#include <math.h>

double cos(double x);
double sin(double x);
double tan(double x);
```

=== Description

#cmd("cos()"), #cmd("sin()") and #cmd("tan()") compute the cosine, sine and
tangent of #var("x"), an angle in radians. #cmd("tan()") is computed as
#cmd("sin(x) / cos(x)").

=== Returns

The cosine, sine or tangent. If #cmd("cos(x)") is exactly 0,
#cmd("tan()") returns #cmd("HUGE_VAL") without setting #cmd("errno").

=== Notes

- The argument is first reduced by whole multiples of 2#sym.pi, with the
  number of multiples converted to an #cmd("int"). #var("x") must therefore
  be smaller in magnitude than about 1.3E10\; the result for a larger
  argument is meaningless.
- The reduction is done in #cmd("double") arithmetic, so the error grows with
  the magnitude of #var("x"): at about 10#super[4] the result has some 12
  correct digits, at about 10#super[9] some 7.

=== Related

#cmd("acos()") (@std-math-acos), #cmd("atan()") (@std-math-atan).

== cosh, sinh, tanh <std-math-cosh>

#idx("cosh")#idx("sinh")#idx("tanh")#idx("hyperbolic functions")

=== Format

```
#include <math.h>

double cosh(double x);
double sinh(double x);
double tanh(double x);
```

=== Description

#cmd("cosh()"), #cmd("sinh()") and #cmd("tanh()") compute the hyperbolic
cosine, sine and tangent of #var("x"). All three are computed from
#cmd("exp()"): #cmd("cosh()") and #cmd("sinh()") from #cmd("exp(x)") and its
reciprocal, #cmd("tanh()") from #cmd("exp(-2 * x)").

=== Returns

The hyperbolic cosine, sine or tangent.

=== Notes

They are computed with #cmd("exp()") (@std-math-exp). For a magnitude of
#var("x") above about 174.7, #cmd("exp()") returns #cmd("HUGE_VAL") and
stores #cmd("ERANGE"), so #cmd("cosh()") and #cmd("sinh()") return half of
it, about 5E72, with the sign of the result. #cmd("tanh()") returns +1 or -1
for a large magnitude.

Up to release 2.22.0 #cmd("exp()") lost all accuracy for negative
arguments, so #cmd("tanh(20)") was negative and #cmd("cosh(-20)") wrong in
the first digit, and all three ended with abend #cmd("S0CC") above about 173
(#cmd("tanh()") above about 86.5).

=== Related

#cmd("exp()") (@std-math-exp).

== exp <std-math-exp>

#idx("exp")

=== Format

```
#include <math.h>

double exp(double x);
```

=== Description

#cmd("exp()") computes the exponential function of #var("x"),
_e_#super[#var("x")]. It splits #var("x") into #var("n") ln 2 + #var("r") with
|#var("r")| at most ln 2 / 2, sums the power series of _e_#super[#var("r")],
and scales by 2#super[#var("n")] in the exponent of the result.

=== Returns

The exponential of #var("x"). For #var("x") above about 174.67 it returns
#cmd("HUGE_VAL"), below about -180.2 it returns 0, and in both cases stores
#cmd("ERANGE") in #cmd("errno").

=== Notes

- Up to release 2.22.0 the series of _e_#super[#var("x")] was summed without
  reducing the argument: negative arguments lost their accuracy
  (#cmd("exp(-30)") was negative), and above about 173 or below about -173
  an intermediate term ended the program with abend #cmd("S0CC"), although
  #cmd("exp(174)"), about 3.7E75, is within range.

=== Related

#cmd("log()") (@std-math-log), #cmd("pow()") (@std-math-pow).

== fabs <std-math-fabs>

#idx("fabs")

=== Format

```
#include <math.h>

double fabs(double x);
```

=== Description

#cmd("fabs()") computes the absolute value of #var("x").

=== Returns

The absolute value of #var("x").

=== Notes

The compiler translates each call into the #cmd("LPDR") instruction\; the
library function is used only when its address is taken or the program is
compiled with #cmd("-fno-builtin").

=== Related

#cmd("abs()") in @std-stdlib.

== fmod <std-math-fmod>

#idx("fmod")#idx("remainder", "floating-point")

=== Format

```
#include <math.h>

double fmod(double x, double y);
```

=== Description

#cmd("fmod()") computes the floating-point remainder of #var("x") /
#var("y"): the value #var("x") - #var("i") #sym.times #var("y") for the
integer #var("i") that makes the result have the sign of #var("x") and a
magnitude less than that of #var("y").

=== Returns

The remainder. If #var("y") is 0, #cmd("fmod()") returns 0 and does not set
#cmd("errno")\; C99 allows either this or a domain error.

=== Notes

The quotient #var("x") / #var("y") is formed and rounded, so for operands of
very different magnitudes the remainder is not exact as C99 requires. It is
always in the range 0 to #var("y") in magnitude, with the sign of #var("x").
When the quotient exceeds 2#super[52] it has no fractional digits left, and
the result is no longer the true remainder, only a value in that range.

=== Related

#cmd("modf()") (@std-math-modf), #cmd("div()") in @std-stdlib.

== frexp, ldexp <std-math-frexp>

#idx("frexp")#idx("ldexp")

=== Format

```
#include <math.h>

double frexp(double value, int *exp);
double ldexp(double x, int exp);
```

=== Description

#cmd("frexp()") breaks #var("value") into a fraction and a power of 2: it
stores the power in #cmd("*")#var("exp") and returns the fraction.

#cmd("ldexp()") multiplies #var("x") by 2 raised to the power #var("exp").

Both functions work on the binary exponent, as C99 defines them, although
the hardware exponent is a power of 16: the binary exponent is the hardware
exponent times 4, adjusted by the leading zero bits of the fraction.

=== Returns

#cmd("frexp()") returns a value whose magnitude is in the range 0.5 to less
than 1, such that #var("value") equals the result times
2#super[#cmd("*")#var("exp")]. For a #var("value") of 0 it returns 0.

#cmd("ldexp()") returns #var("x") #sym.times 2#super[#var("exp")].

=== Notes

- When #var("value") is 0, #cmd("frexp()") stores 0 into
  #cmd("*")#var("exp"). Up to release 2.22.0 it stored nothing.
- #cmd("ldexp()") does not check the range. A result beyond the range of a
  #cmd("double") is not reported and is wrong.

=== Related

#cmd("modf()") (@std-math-modf), #cmd("log()") (@std-math-log).

== log, log10 <std-math-log>

#idx("log")#idx("log10")#idx("logarithm")

=== Format

```
#include <math.h>

double log(double x);
double log10(double x);
```

=== Description

#cmd("log()") computes the natural logarithm of #var("x"), #cmd("log10()")
the base-10 logarithm. #cmd("log10(x)") is #cmd("log(x)") divided by the
natural logarithm of 10.

=== Returns

The logarithm. If #var("x") is 0, #cmd("log()") returns #cmd("-HUGE_VAL"),
if it is negative #cmd("HUGE_VAL")\; #cmd("log10()") returns these divided by
the natural logarithm of 10, about 4.3E72.

=== Errors

#deflist(width: 1.1in,
  [#cmd("ERANGE")], [#var("x") is 0. Up to release 2.22.0 this was
    #cmd("EDOM"), and the result #cmd("+HUGE_VAL").],
  [#cmd("EDOM")], [#var("x") is negative.],
)

=== Related

#cmd("exp()") (@std-math-exp), #cmd("frexp()") (@std-math-frexp).

== modf <std-math-modf>

#idx("modf")

=== Format

```
#include <math.h>

double modf(double value, double *iptr);
```

=== Description

#cmd("modf()") breaks #var("value") into an integral part and a fractional
part, both with the sign of #var("value"). It stores the integral part in
#cmd("*")#var("iptr").

=== Returns

The fractional part.

=== Notes

The result is exact for every #cmd("double"), including values beyond the
range of #cmd("int").

=== Example

See the example under #cmd("ceil()") (@std-math-ceil).

=== Related

#cmd("floor()") (@std-math-ceil), #cmd("fmod()") (@std-math-fmod).

== pow <std-math-pow>

#idx("pow")#idx("power function")

=== Format

```
#include <math.h>

double pow(double x, double y);
```

=== Description

#cmd("pow()") computes #var("x") raised to the power #var("y"). When
#var("y") is an integral value, the result is formed by repeated
multiplication, and for a negative #var("y") by a final division. Otherwise
it is computed as #cmd("exp(y * log(x))").

=== Returns

#var("x")#super[#var("y")]. #cmd("pow(x, 0.0)") is 1 for every #var("x").
If #var("x") is negative and #var("y") is not integral, #cmd("pow()")
returns 0.

=== Errors

#deflist(width: 1.1in,
  [#cmd("EDOM")], [#var("x") is negative and #var("y") is not an integral
    value.],
  [#cmd("ERANGE")], [#var("x") is 0 and #var("y") negative (the result is
    #cmd("HUGE_VAL")), or the result is beyond the range of a
    #cmd("double") (#cmd("HUGE_VAL"), negative for a negative #var("x") and
    an odd #var("y")).],
)

=== Notes

- An integral #var("y") below 2#super[31] in magnitude is applied by
  repeated squaring, about log#sub[2] |#var("y")| multiplications\; a larger
  one, and a non-integral one, through #cmd("exp(y * log(x))").
- Up to release 2.22.0 an integral #var("y") took |#var("y")| - 1
  multiplications, #cmd("pow(0.0, y)") with a negative #var("y") ended the
  program with abend #cmd("S0CF") or #cmd("S0CC"), a result beyond the
  range ended it with #cmd("S0CC"), and a result less than 1 with a
  non-integral #var("y") lost its accuracy with #cmd("exp()").

=== Related

#cmd("exp()") (@std-math-exp), #cmd("sqrt()") (@std-math-sqrt).

== sqrt <std-math-sqrt>

#idx("sqrt")#idx("square root")

=== Format

```
#include <math.h>

double sqrt(double x);
```

=== Description

#cmd("sqrt()") computes the nonnegative square root of #var("x") by
Newton's method.

=== Returns

The square root. If #var("x") is negative, #cmd("sqrt()") returns 0.

=== Errors

#deflist(width: 1.1in,
  [#cmd("EDOM")], [#var("x") is negative.],
)

=== Example

#code(read("../ex/std-math/errno.c"))

=== Related

#cmd("pow()") (@std-math-pow).
