#import "../bookmaster/bookmaster.typ": *

= \<ctype.h\> — Character Handling <std-ctype>

#idx("ctype.h")
#idx("character classification")
#idx("EBCDIC", "character classes")
The header #cmd("<ctype.h>") declares functions that classify a character
-- letter, digit, white space and so on -- and two that convert between
uppercase and lowercase.

== Characters on MVS <std-ctype-ebcdic>

#idx("code page 037")
Programs built with cc370 run in EBCDIC. The compiler translates character
constants and string literals to code page 037 (CP037), with one exception
for the newline: #cmd("'\\n'") is X'15', the EBCDIC new-line character,
not the line feed X'25'. The classification functions use a table indexed
by these code points, and they classify the characters as the C locale
does in CP037. There is only the C locale (see @std-locale); the table
does not change at run time.

Three properties of EBCDIC matter to a C program:

- *The letters are not contiguous.* The lowercase letters are at X'81'–X'89'
  (#cmd("a")–#cmd("i")), X'91'–X'99' (#cmd("j")–#cmd("r")) and X'A2'–X'A9'
  (#cmd("s")–#cmd("z")); the uppercase letters at X'C1'–X'C9', X'D1'–X'D9'
  and X'E2'–X'E9'. The gaps hold other characters. A test such as
  #cmd("c >= 'a' && c <= 'z'") is therefore also true for 15 characters
  that are not letters, and #cmd("c - 'a'") is not the position of a
  letter in the alphabet. Use #cmd("isalpha()"), #cmd("islower()") and
  #cmd("isupper()") instead.
- *Lowercase sorts before uppercase, and letters before digits.* The digits
  #cmd("0")–#cmd("9") are contiguous, at X'F0'–X'F9', so #cmd("c - '0'") is
  the value of a digit, as C requires.
- *A #cmd("char") is unsigned.* Every character, read from a string as a
  plain #cmd("char"), has a value from 0 to 255 and is a valid argument.

#idx("national characters")
Only the 26 letters of the Latin alphabet are letters to #cmd("isalpha()").
The accented letters of CP037, such as #cmd("â") at X'42', belong to no
class at all, and #cmd("toupper()") and #cmd("tolower()") leave them
unchanged. The national characters #cmd("$"), #cmd("#") and #cmd("@"),
which MVS allows in names, are punctuation.

@std-ctype-classes lists the code points that belong to each class.

#tab(caption: [Character classes in code page 037])[
  #table(columns: (0.95in, 0.5in, 1fr),
    [Function], [Count], [Code points],
    [#cmd("isupper()")], [26], [X'C1'–X'C9', X'D1'–X'D9', X'E2'–X'E9'],
    [#cmd("islower()")], [26], [X'81'–X'89', X'91'–X'99', X'A2'–X'A9'],
    [#cmd("isalpha()")], [52], [#cmd("isupper()") and #cmd("islower()")],
    [#cmd("isdigit()")], [10], [X'F0'–X'F9'],
    [#cmd("isxdigit()")], [22], [X'F0'–X'F9', X'81'–X'86' (#cmd("a")–#cmd("f")),
      X'C1'–X'C6' (#cmd("A")–#cmd("F"))],
    [#cmd("isalnum()")], [62], [#cmd("isalpha()") and #cmd("isdigit()")],
    [#cmd("isspace()")], [6], [X'05' horizontal tab, X'0B' vertical tab,
      X'0C' form feed, X'0D' carriage return, X'15' new line, X'40' space],
    [#cmd("isblank()")], [2], [X'05' horizontal tab, X'40' space],
    [#cmd("ispunct()")], [34], [X'4A'–X'50', X'5A'–X'61', X'6A'–X'6F',
      X'79'–X'7F', X'A1', X'AD', X'BD', X'C0', X'D0', X'E0'],
    [#cmd("isgraph()")], [96], [#cmd("isalnum()") and #cmd("ispunct()")],
    [#cmd("isprint()")], [97], [#cmd("isgraph()") and X'40' space],
    [#cmd("iscntrl()")], [60], [X'00'–X'3F', except X'29', X'30', X'31' and
      X'3E'],
  )
] <std-ctype-classes>

#idx("brackets", "classification")
#note[The square brackets and the circumflex are not classified correctly.
cc370 translates #cmd("'['"), #cmd("']'") and #cmd("'^'") to X'BA', X'BB'
and X'B0', their code points in CP037, but the table marks X'AD' and X'BD'
as punctuation instead -- the code points of the brackets in code page
1047 -- and gives X'BA', X'BB' and X'B0' no class. On this target
#cmd("ispunct('[')"), #cmd("isgraph('[')") and #cmd("isprint('[')") are
therefore 0, and so are they for #cmd("']'") and #cmd("'^'"), while X'AD'
(#cmd("Ý") in CP037) and X'BD' (#cmd("¨")) count as punctuation. A program
that must accept these three characters as printable has to test for them
itself.]

The line feed X'25' is a control character but not white space, because a
C program writes the new line as X'15'. Data that comes from a system
writing X'25' as its line end must be treated accordingly.

== Using the Functions <std-ctype-use>

#idx("EOF", "in ctype functions")
Each function is declared as a function and also defined as a macro of the
same name. The macro looks the argument up in the table directly; the
function, in #cmd("libc.a"), does the same. A program gets the function
instead of the macro with #cmd("#undef"), by writing the name in
parentheses, #cmd("(isalpha)(c)"), or by taking its address.

The argument must be #cmd("EOF") or a value from 0 to 255. #cmd("EOF") is
classified as nothing, and #cmd("toupper(EOF)") and #cmd("tolower(EOF)")
return #cmd("EOF"). Any other value outside that range reads storage
outside the table; the result is undefined, and no check is made.

#note[The macros index the table with their argument. When the argument has
type #cmd("char"), the compiler option #cmd("-Wall") reports
#cmd("subscript has type `char'"), and with #cmd("-Werror") the
compilation fails. Pass an #cmd("int"), or cast the character to
#cmd("unsigned char"): #cmd("isalpha((unsigned char)*p)"). The cast changes
no value, since #cmd("char") is unsigned.]

== isalnum, isalpha, isblank, iscntrl, isdigit, isgraph, islower, isprint, ispunct, isspace, isupper, isxdigit <std-ctype-is>

#idx("isalnum")
#idx("isalpha")
#idx("isblank")
#idx("iscntrl")
#idx("isdigit")
#idx("isgraph")
#idx("islower")
#idx("isprint")
#idx("ispunct")
#idx("isspace")
#idx("isupper")
#idx("isxdigit")

=== Format

```
#include <ctype.h>

int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
```

=== Description

Each function tests whether #var("c") belongs to one class of characters:

#deflist(width: 0.95in,
  [#cmd("isalnum()")], [a letter or a decimal digit],
  [#cmd("isalpha()")], [a letter],
  [#cmd("isblank()")], [a blank: space or horizontal tab],
  [#cmd("iscntrl()")], [a control character],
  [#cmd("isdigit()")], [a decimal digit],
  [#cmd("isgraph()")], [a printing character other than space],
  [#cmd("islower()")], [a lowercase letter],
  [#cmd("isprint()")], [a printing character, including space],
  [#cmd("ispunct()")], [a printing character that is neither a letter, a
    digit nor space],
  [#cmd("isspace()")], [white space: space, horizontal and vertical tab, form
    feed, carriage return, new line],
  [#cmd("isupper()")], [an uppercase letter],
  [#cmd("isxdigit()")], [a hexadecimal digit: #cmd("0")–#cmd("9"),
    #cmd("a")–#cmd("f"), #cmd("A")–#cmd("F")],
)

@std-ctype-classes gives the code points of each class.

=== Returns

Nonzero if #var("c") belongs to the class, 0 if it does not. The nonzero
value is not always 1: it is the bit that marks the class in the table,
for example 256 from #cmd("isspace()"). Test the result for zero or nonzero,
never compare it with 1.

=== Notes

#cmd("'['"), #cmd("']'") and #cmd("'^'") belong to no class; see
@std-ctype-ebcdic.

=== Example

The function in @std-ctype-ex checks one qualifier of a data set name and
changes it to uppercase. It tests letters with #cmd("isalpha()"), not with a
range of code points, and passes each character as an #cmd("unsigned char").

#fig(caption: [Checking a qualifier of a data set name])[
  #code(read("../ex/std-ctype/dsname.c"), numbers: true)
] <std-ctype-ex>

=== Related

@std-ctype-toupper.

== tolower, toupper <std-ctype-toupper>

#idx("tolower")
#idx("toupper")

=== Format

```
#include <ctype.h>

int tolower(int c);
int toupper(int c);
```

=== Description

#cmd("toupper()") converts a lowercase letter to the corresponding uppercase
letter, #cmd("tolower()") an uppercase letter to lowercase. Any other
argument is returned unchanged.

=== Returns

The converted character, or #var("c") when there is nothing to convert.
#cmd("EOF") is returned as #cmd("EOF").

=== Notes

The 26 letter pairs of the Latin alphabet are converted, and nothing else:
#cmd("toupper('a')") is #cmd("'A'") (X'81' to X'C1'), but #cmd("toupper()")
leaves #cmd("â") (X'42') as it is. In EBCDIC the cases differ by X'40', so
the conversion can also be written as an #cmd("OR") or #cmd("AND") of that
bit; such code is correct only for letters, and the functions are the safe
form.

=== Related

@std-ctype-is.
