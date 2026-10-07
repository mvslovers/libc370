#import "../bookmaster/bookmaster.typ": *

= \<locale.h\> — Localization <std-locale>

#idx("locale.h")
#idx("locale")
#idx("C locale")
The header #cmd("<locale.h>") declares the functions that select and
describe a locale: the conventions for the character set, numbers, money
and dates.

libc370 provides one locale, the C locale, in which a program starts. The
characters are those of EBCDIC code page 037 as described in @std-ctype,
the decimal point is #cmd("."), and nothing is grouped. No other locale can
be selected.

#tab(caption: [Macros defined in \<locale.h\>])[
  #table(columns: (1.4in, 0.6in, 1fr),
    [Macro], [Value], [Category],
    [#cmd("LC_ALL")], [1], [the whole locale],
    [#cmd("LC_COLLATE")], [2], [collation, #cmd("strcoll()") and
      #cmd("strxfrm()")],
    [#cmd("LC_CTYPE")], [3], [character handling],
    [#cmd("LC_MONETARY")], [4], [monetary formatting],
    [#cmd("LC_NUMERIC")], [5], [the decimal point],
    [#cmd("LC_TIME")], [6], [#cmd("strftime()")],
    [#cmd("NULL")], [], [#cmd("((void *)0)")],
  )
] <std-locale-macros>

== setlocale <std-locale-setlocale>

#idx("setlocale")

=== Format

```
#include <locale.h>

char *setlocale(int category, const char *locale);
```

=== Description

#cmd("setlocale()") selects the locale #var("locale") for the part of the
program's behavior named by #var("category"), or queries it when
#var("locale") is #cmd("NULL"). Since the C locale is the only one, the
function changes nothing; it reports whether the request names that
locale:

- #var("locale") #cmd("NULL") queries the current locale, which is always
  #cmd("\"C\"").
- #cmd("\"C\"") and #cmd("\"\"") (the native locale, which is the C
  locale) are accepted.
- Any other name is refused.

=== Returns

The string #cmd("\"C\"") when #var("locale") is #cmd("NULL"), #cmd("\"C\"")
or #cmd("\"\""), and a null pointer for any other name. The returned string
must not be modified.

=== Notes

#var("category") is not checked: any value, including one that is none of
the #cmd("LC_") macros, is treated as a valid category.

=== Related

@std-locale-localeconv.

== localeconv <std-locale-localeconv>

#idx("localeconv")
#idx("lconv")

=== Format

```
#include <locale.h>

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
};

struct lconv *localeconv(void);
```

=== Description

#cmd("localeconv()") returns a structure that describes how numbers and
amounts of money are formatted in the current locale.

=== Returns

A pointer to a structure of the library, filled in for the C locale:
#cmd("decimal_point") is #cmd("\".\""); every other string member is the
empty string #cmd("\"\""), meaning the information is not available; every
#cmd("char") member is #cmd("CHAR_MAX") (255), meaning the same. The
structure must not be modified.

=== Notes

The C99 standard adds six #cmd("char") members for international currency
formatting -- #cmd("int_p_cs_precedes"), #cmd("int_n_cs_precedes"),
#cmd("int_p_sep_by_space"), #cmd("int_n_sep_by_space"),
#cmd("int_p_sign_posn") and #cmd("int_n_sign_posn"). The structure of
libc370 does not have them, and a program that refers to them does not
compile.

=== Related

@std-locale-setlocale.
