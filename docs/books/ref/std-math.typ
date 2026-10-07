#import "../bookmaster/bookmaster.typ": *

= \<math.h\> — Mathematics <std-math>

#idx("math.h")
The header #cmd("<math.h>") declares the mathematical functions of C90, all
for #cmd("double"), and defines the macro #cmd("HUGE_VAL"). This chapter
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
  #cmd("-HUGE_VAL")), and #cmd("strtod()") on overflow.
#idx("HUGE_VAL")#idx("S0CC")#idx("S0CF")

== Error Reporting <std-math-errors>

#idx("errno", "mathematical functions")#idx("EDOM")
A function whose argument lies outside its domain stores #cmd("EDOM") in
#cmd("errno") and returns the value given in its entry. No function stores
#cmd("ERANGE"): a result that is too large ends the program as described
above.
To detect an error, set #cmd("errno") to 0 before the call and test it
afterwards. The C99 macro #cmd("math_errhandling") is not defined.

== What Is Not Provided <std-math-missing>

libc370 provides only the 22 functions described in this chapter. None of
the functions that C99 added to #cmd("<math.h>") exists: there are no
#cmd("float") and #cmd("long double") variants (#cmd("sinf()"),
#cmd("sqrtl()") ...), and no #cmd("round()"), #cmd("trunc()"),
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

#cmd("atan2(0.0, 0.0)") divides zero by zero and ends the program with abend
#cmd("S0CF"). C99 allows a domain error here\; test for both arguments being
zero before the call.

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

They inherit the limits of #cmd("exp()") (@std-math-exp). #cmd("cosh()")
and #cmd("sinh()") end the program with abend #cmd("S0CC") when the
magnitude of #var("x") exceeds about 173, a little before their result
would leave the range of a #cmd("double").
Where #cmd("exp()") is called with a negative argument -- #cmd("cosh()")
and #cmd("sinh()") of a negative #var("x"), #cmd("tanh()") of a positive
#var("x") -- the result loses accuracy as the magnitude grows:
#cmd("tanh(10)") has about eight correct digits, #cmd("tanh(20)") is
negative, and #cmd("cosh(-20)") is wrong in the first digit. Use the
symmetries instead: #cmd("cosh(-x)") = #cmd("cosh(x)"), #cmd("sinh(-x)") =
#cmd("-sinh(x)"), and for #cmd("tanh()") of a large argument the value 1.

#cmd("tanh()") also ends the program with abend #cmd("S0CC") when the
magnitude of #var("x") exceeds about 86.5, although its result, +1 or -1, is
in range: #cmd("exp(-2 * x)") overflows, or its series does.

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
_e_#super[#var("x")], by summing its power series.

=== Returns

The exponential of #var("x").

=== Notes

- *Negative arguments lose accuracy.* The series is summed without reducing
  the argument, and for negative #var("x") its terms alternate in sign and
  cancel. At #var("x") = -10 only about eight digits of the result are
  correct, at -20 none, and at -30 the result is negative. For a negative argument compute #cmd("1.0 / exp(-x)"), which
  is accurate.
- For #var("x") greater than about 173 or less than about -173 the program
  ends with abend #cmd("S0CC"). The terms of the series grow before they
  shrink, and each is formed by multiplying the previous one by #var("x")
  before dividing, so that intermediate product overflows first:
  #cmd("exp(174)") abends although its result, about 3.7E75, is within
  the range of a #cmd("double").
- No #cmd("errno") value is ever set.

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

- When #var("value") is 0, #cmd("frexp()") does not store into
  #cmd("*")#var("exp"): the variable keeps its previous contents, where C99
  requires 0. Set it to 0 before the call.
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

The logarithm. If #var("x") is 0 or negative, #cmd("log()") returns
#cmd("HUGE_VAL") -- a positive value, where C99 prescribes
#cmd("-HUGE_VAL") for 0 -- and #cmd("log10()") returns
#cmd("HUGE_VAL") divided by the natural logarithm of 10, about 4.3E72.

=== Errors

#deflist(width: 1.1in,
  [#cmd("EDOM")], [#var("x") is 0 or negative.],
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
)

=== Notes

- An integral #var("y") is converted to #cmd("int"), so it must lie within
  the range of #cmd("int"). The number of multiplications is
  #cmd("|y|") - 1, so a large integral exponent takes correspondingly long.
- #cmd("pow(0.0, y)") with a negative integral #var("y") divides by zero and
  ends the program with abend #cmd("S0CF"). With a non-integral #var("y") it
  takes the logarithm of 0, which is #cmd("HUGE_VAL"), and #cmd("exp()")
  of its product with #var("y") ends with abend #cmd("S0CC").
- A result beyond the range of a #cmd("double") ends the program with abend
  #cmd("S0CC"). With a non-integral #var("y"), a result less than 1 inherits
  the loss of accuracy of #cmd("exp()") for negative arguments:
  #cmd("pow(10.0, -7.5)") has only two correct digits.

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
