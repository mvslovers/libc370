#import "../bookmaster/bookmaster.typ": *

= \<strings.h\> — String Operations <std-strings>

#idx("strings.h")
The POSIX header #cmd("<strings.h>") declares the two case-insensitive string
comparisons. It includes #cmd("<stddef.h>") for #cmd("size_t").

Both functions fold each character through #cmd("tolower()"), which knows the
EBCDIC alphabet: the lowercase letters lie in three runs, #cmd("X'81'")–#cmd("X'89'"),
#cmd("X'91'")–#cmd("X'99'") and #cmd("X'A2'")–#cmd("X'A9'"), and each
uppercase letter is #cmd("X'40'") above its lowercase partner. A fold written
for ASCII, such as #cmd("c | 0x20"), does not work on MVS\; these functions do.
#idx("EBCDIC", "case folding")

The order the functions report is the order of the folded EBCDIC codes, not
the ASCII order: the space and most punctuation come before the letters, and
the digits after them.

== strcasecmp, strncasecmp <std-strings-strcasecmp>

#idx("strcasecmp")#idx("strncasecmp")#idx("case-insensitive comparison")

=== Format

```
#include <strings.h>

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
```

=== Description

#cmd("strcasecmp()") compares the strings #var("s1") and #var("s2"), ignoring
the difference between uppercase and lowercase letters. Each character is
converted with #cmd("tolower()"), and the converted values are compared as
#cmd("unsigned char").

#cmd("strncasecmp()") does the same for at most #var("n") characters. It stops
earlier at the end of the strings.

=== Returns

-1 if #var("s1") is less than #var("s2"), 0 if they are equal, and 1 if
#var("s1") is greater. The values are always exactly -1, 0 or 1.
#cmd("strncasecmp()") returns 0 when #var("n") is 0.

=== Notes

- The comparison follows the EBCDIC code points, so
  #cmd("strcasecmp(\"a\", \"1\")") is negative. On an ASCII system it is
  positive.
- The library has no #cmd("stricmp()") or #cmd("strncmpi()")\; use these two
  functions instead.

=== Example

#code(read("../ex/std-strings/casecmp.c"))

=== Related

#cmd("strcmp()") and #cmd("strncmp()") in @std-string\; #cmd("tolower()") in
@std-ctype.
