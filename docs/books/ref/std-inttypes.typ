#import "../bookmaster/bookmaster.typ": *

= \<inttypes.h\> — Format Conversion of Integer Types <std-inttypes>

#idx("inttypes.h")
The header #cmd("<inttypes.h>") includes #cmd("<stdint.h>") (see
@std-stdint) and adds two things: macros that give the conversion
specifiers of #cmd("printf()") and #cmd("scanf()") for each integer type of
#cmd("<stdint.h>"), and four functions for the greatest-width integer type
#cmd("intmax_t").

== Format Specifier Macros <std-inttypes-macros>

#idx("PRId64")
#idx("format specifier macros")
Each macro expands to a string literal holding a length modifier and a
conversion letter. The literal is joined to the rest of the format by
string concatenation, with the #cmd("%") written outside:

```
int64_t total;
printf("total %" PRId64 " bytes\n", total);
```

The names follow one pattern. #cmd("PRI") macros are for the
#cmd("printf()") family, #cmd("SCN") macros for the #cmd("scanf()") family.
Then comes the conversion letter -- #cmd("d"), #cmd("i"), #cmd("o"),
#cmd("u"), #cmd("x"), and for #cmd("printf()") also #cmd("X") -- and then the
type: #var("N") for #cmd("int")#var("N")#cmd("_t") and
#cmd("uint")#var("N")#cmd("_t"), #cmd("LEAST")#var("N"), #cmd("FAST")#var("N"),
#cmd("MAX") for #cmd("intmax_t") and #cmd("PTR") for #cmd("intptr_t").
#cmd("PRIdFAST16"), for example, prints an #cmd("int_fast16_t") in decimal.

Since the least-width and fast types are the exact-width types, all the
macros for one width expand to the same length modifier.
@std-inttypes-mod gives it for every width.

#tab(caption: [Length modifiers of the PRI and SCN macros])[
  #table(columns: (1.9in, 1.1in, 1.1in, 1fr),
    [Types], [#cmd("PRI") modifier], [#cmd("SCN") modifier], [Example],
    [8 bits: #var("N") = 8], [none], [#cmd("hh")],
      [#cmd("PRIu8") = #cmd("\"u\""), #cmd("SCNu8") = #cmd("\"hhu\"")],
    [16 bits: #var("N") = 16], [none], [#cmd("h")],
      [#cmd("PRId16") = #cmd("\"d\""), #cmd("SCNd16") = #cmd("\"hd\"")],
    [32 bits: #var("N") = 32], [#cmd("l")], [#cmd("l")],
      [#cmd("PRIx32") = #cmd("\"lx\"")],
    [64 bits: #var("N") = 64], [#cmd("ll")], [#cmd("ll")],
      [#cmd("PRId64") = #cmd("\"lld\"")],
    [#cmd("intmax_t"), #cmd("uintmax_t"): #cmd("MAX")], [#cmd("ll")],
      [#cmd("ll")], [#cmd("PRIuMAX") = #cmd("\"llu\"")],
    [#cmd("intptr_t"), #cmd("uintptr_t"): #cmd("PTR")], [none], [none],
      [#cmd("PRIxPTR") = #cmd("\"x\"")],
  )
] <std-inttypes-mod>

The #cmd("PRI") macros for 8 and 16 bits need no modifier, because a
#cmd("char") or #cmd("short") argument is promoted to #cmd("int") when it is
passed. The #cmd("SCN") macros do need one, because #cmd("scanf()") stores
through a pointer to an object of exactly that size. The #cmd("SCN")
macros exist for #cmd("d"), #cmd("i"), #cmd("o"), #cmd("u") and #cmd("x")
only; as the standard prescribes, there is no #cmd("SCNX").

Every macro agrees with the type it names, so #cmd("-Wformat") accepts it
without a warning; a #cmd("-Wall -Werror") build is the check.

== imaxabs <std-inttypes-imaxabs>

#idx("imaxabs")

=== Format

```
#include <inttypes.h>

intmax_t imaxabs(intmax_t j);
```

=== Description

#cmd("imaxabs()") computes the absolute value of #var("j").

=== Returns

The absolute value of #var("j").

=== Notes

The absolute value of #cmd("INTMAX_MIN") cannot be represented in an
#cmd("intmax_t"). As in standard C, the result for that argument is
undefined.

=== Related

#cmd("abs()"), #cmd("labs()"), #cmd("llabs()") (see @std-stdlib).

== imaxdiv <std-inttypes-imaxdiv>

#idx("imaxdiv")
#idx("imaxdiv_t")

=== Format

```
#include <inttypes.h>

typedef struct { intmax_t quot; intmax_t rem; } imaxdiv_t;

imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);
```

=== Description

#cmd("imaxdiv()") divides #var("numer") by #var("denom") and computes the
quotient and the remainder in one call. The quotient is truncated toward
zero, and the remainder has the sign of #var("numer").

=== Returns

A structure of type #cmd("imaxdiv_t") whose member #cmd("quot") holds the
quotient and #cmd("rem") the remainder. For example,
#cmd("imaxdiv(-10000000000, 3)") returns a quotient of −3333333333 and a
remainder of −1.

=== Notes

#var("denom") must not be 0. The function does not check it.

=== Related

#cmd("div()"), #cmd("ldiv()"), #cmd("lldiv()") (see @std-stdlib).

== strtoimax, strtoumax <std-inttypes-strtoimax>

#idx("strtoimax")
#idx("strtoumax")

=== Format

```
#include <inttypes.h>

intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);
```

=== Description

#cmd("strtoimax()") and #cmd("strtoumax()") convert the beginning of the string
#var("nptr") to an #cmd("intmax_t") or a #cmd("uintmax_t"). They are
#cmd("strtoll()") and #cmd("strtoull()") under another name, since
#cmd("intmax_t") is #cmd("long long") on MVS, and they behave exactly as
those functions do:

- White space, as #cmd("isspace()") tests it, is skipped, then an optional
  sign.
- #var("base") is 0 or 2 to 36. With 0 the base is taken from the number:
  a leading #cmd("0x") or #cmd("0X") gives 16, a leading #cmd("0") gives 8,
  anything else 10. With 16 a leading #cmd("0x") is allowed. The prefix
  counts only when a hexadecimal digit follows it: from #cmd("\"0xg\"")
  only the #cmd("0") is converted.
- The letters #cmd("a") to #cmd("z") and #cmd("A") to #cmd("Z") stand for
  the digit values 10 to 35. They are recognized by their values in the
  EBCDIC code page, in which the letters are not contiguous.

If #var("endptr") is not #cmd("NULL"), #cmd("*")#var("endptr") is set to
the first character after the number, or to #var("nptr") when no digit was
converted.

=== Returns

The converted value, or 0 when no digit was converted. When the value is
out of range, #cmd("strtoimax()") returns #cmd("INTMAX_MAX") or
#cmd("INTMAX_MIN") and #cmd("strtoumax()") returns #cmd("UINTMAX_MAX").

For #cmd("strtoumax()"), a number preceded by #cmd("-") is converted and then
negated in the unsigned type, as the standard prescribes: #cmd("\"-1\"")
yields #cmd("UINTMAX_MAX").

=== Errors

#deflist(width: 1in,
  [#cmd("ERANGE")], [The value is out of the range of the return type.],
  [#cmd("EINVAL")], [#var("base") is negative, 1, or greater than 36. The
    function returns 0 and sets #cmd("*")#var("endptr") to #var("nptr").],
)

=== Notes

The wide-character forms #cmd("wcstoimax") and #cmd("wcstoumax") of the C
standard are not provided.

=== Example

@std-inttypes-ex reads a track count from the parameter and prints the
capacity of that many 3350 tracks with the format macros of a 32-bit and
a 64-bit type.

#fig(caption: [Using strtoimax and the format macros])[
  #code(read("../ex/std-inttypes/tracks.c"), numbers: true)
] <std-inttypes-ex>

=== Related

#cmd("strtol()"), #cmd("strtoll()"), #cmd("strtoul()"), #cmd("strtoull()") (see
@std-stdlib).
