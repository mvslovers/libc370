#import "../bookmaster/bookmaster.typ": *

= \<string.h\> — String Handling <std-string>

#idx("string.h")
The header #cmd("<string.h>") declares the functions that copy, compare,
search and measure strings and blocks of storage, together with
#cmd("strerror()") and #cmd("strdup()"). It defines #cmd("size_t")
(#cmd("unsigned long"), 4 bytes) and #cmd("NULL").

Three things are particular to MVS:

- *Collating order.* The comparison functions compare bytes as
  #cmd("unsigned char"), and on MVS the bytes are EBCDIC. In EBCDIC the space
  and most punctuation sort first, then the lowercase letters, then the
  uppercase letters, then the digits. A program that sorts with
  #cmd("strcmp()") therefore puts #cmd("\"abc\"") before #cmd("\"ABC\"") and
  both before #cmd("\"123\"") -- the reverse of the ASCII order.
  #idx("collating sequence")#idx("EBCDIC", "collating sequence")
- *Machine instructions.* #cmd("memcpy()") and #cmd("memset()") use the
  System/370 #cmd("MOVE LONG") instruction (#cmd("MVCL")). The header defines
  #cmd("memset()") as a #cmd("static inline") function, so every call is
  expanded in place.
- *Only the C locale.* #cmd("strcoll()") and #cmd("strxfrm()") behave as
  #cmd("strcmp()") and a plain copy.

The functions do not check their pointer arguments. A null pointer is not
caught: reading through it reads the low storage of the system, and storing
through it ends in a protection exception, abend #cmd("S0C4"). A string
without a terminating null character is read until a null byte happens to
follow.

== memchr <std-string-memchr>

#idx("memchr")

=== Format

```
#include <string.h>

void *memchr(const void *s, int c, size_t n);
```

=== Description

#cmd("memchr()") searches the first #var("n") bytes of the object
#var("s") for the byte #var("c"), converted to #cmd("unsigned char").

=== Returns

A pointer to the first byte that matches, or #cmd("NULL") if none of the
#var("n") bytes does.

=== Related

#cmd("strchr()") (@std-string-strchr).

== memcmp <std-string-memcmp>

#idx("memcmp")

=== Format

```
#include <string.h>

int memcmp(const void *s1, const void *s2, size_t n);
```

=== Description

#cmd("memcmp()") compares the first #var("n") bytes of the objects #var("s1")
and #var("s2"), byte by byte, as #cmd("unsigned char").

=== Returns

-1 if #var("s1") is less than #var("s2"), 0 if the #var("n") bytes are equal,
and 1 if #var("s1") is greater. The values are always exactly -1, 0 or 1.

=== Notes

The order is the order of the EBCDIC code points.

=== Related

#cmd("strcmp()") (@std-string-strcmp).

== memcpy <std-string-memcpy>

#idx("memcpy")

=== Format

```
#include <string.h>

void *memcpy(void *s1, const void *s2, size_t n);
```

=== Description

#cmd("memcpy()") copies #var("n") bytes from the object #var("s2") into the
object #var("s1"). The copy is done by one #cmd("MVCL") instruction.

=== Returns

#var("s1").

=== Notes

- The objects must not overlap. If they overlap so that the copy would
  overwrite source bytes before it has moved them, #cmd("MVCL") moves
  nothing at all, and #var("s1") is left unchanged. Use #cmd("memmove()") for
  overlapping objects.
- #cmd("MVCL") takes its length from the low-order 24 bits of the register,
  so #var("n") must be less than 16 megabytes. On MVS 3.8j no object is
  larger.

=== Related

#cmd("memmove()") (@std-string-memmove), #cmd("strcpy()")
(@std-string-strcpy).

== memmove <std-string-memmove>

#idx("memmove")

=== Format

```
#include <string.h>

void *memmove(void *s1, const void *s2, size_t n);
```

=== Description

#cmd("memmove()") copies #var("n") bytes from the object #var("s2") into the
object #var("s1"). The objects may overlap: the bytes are copied as if
through a temporary area. When #var("s1") lies above #var("s2") the copy runs
from the last byte to the first.

=== Returns

#var("s1").

=== Notes

#cmd("memmove()") copies one byte at a time. For large objects that do not
overlap, #cmd("memcpy()") is faster.

=== Related

#cmd("memcpy()") (@std-string-memcpy).

== memset <std-string-memset>

#idx("memset")

=== Format

```
#include <string.h>

static __inline void *memset(void *s, int c, size_t n);
```

=== Description

#cmd("memset()") stores the byte #var("c"), converted to #cmd("unsigned char"), into each of the first #var("n") bytes of the object #var("s").

=== Returns

#var("s").

=== Notes

- The header defines #cmd("memset()") as a #cmd("static inline") function,
  which the compiler expands at each call into a #cmd("MVCL") instruction
  that pads the target with #var("c"). The library also contains an
  external #cmd("memset()") with the same effect, which is used only by a
  program that declares #cmd("memset()") itself instead of including
  #cmd("<string.h>").
- As with #cmd("memcpy()"), #var("n") must be less than 16 megabytes.
- Because the function is #cmd("static"), its address differs from one
  source file to the next.

=== Related

#cmd("calloc()") in @std-stdlib.

== strcat, strncat <std-string-strcat>

#idx("strcat")#idx("strncat")

=== Format

```
#include <string.h>

char *strcat(char *s1, const char *s2);
char *strncat(char *s1, const char *s2, size_t n);
```

=== Description

#cmd("strcat()") appends a copy of the string #var("s2"), including its
terminating null character, to the end of the string #var("s1").

#cmd("strncat()") appends at most #var("n") characters of #var("s2"), and then
a null character. #var("s1") must have room for up to #var("n") + 1 more
bytes.

=== Returns

#var("s1").

=== Related

#cmd("strcpy()") (@std-string-strcpy).

== strchr, strrchr <std-string-strchr>

#idx("strchr")#idx("strrchr")

=== Format

```
#include <string.h>

char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
```

=== Description

#cmd("strchr()") locates the first occurrence of #var("c"), converted to
#cmd("char"), in the string #var("s")\; #cmd("strrchr()") locates the last.
The terminating null character is part of the string, so both functions
find it when #var("c") is #cmd("'\\0'").

=== Returns

A pointer to the character found, or #cmd("NULL") if #var("c") does not
occur in the string.

=== Related

#cmd("memchr()") (@std-string-memchr), #cmd("strpbrk()")
(@std-string-strcspn), #cmd("strstr()") (@std-string-strstr).

== strcmp, strncmp <std-string-strcmp>

#idx("strcmp")#idx("strncmp")

=== Format

```
#include <string.h>

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
```

=== Description

#cmd("strcmp()") compares the strings #var("s1") and #var("s2") character by
character, as #cmd("unsigned char"). #cmd("strncmp()") compares at most
#var("n") characters and stops earlier at the end of the strings.

=== Returns

-1 if #var("s1") is less than #var("s2"), 0 if they are equal, and 1 if
#var("s1") is greater. The values are always exactly -1, 0 or 1.
#cmd("strncmp()") returns 0 when #var("n") is 0.

=== Notes

The order is the EBCDIC order described at the beginning of this chapter.
To compare without regard to case, use #cmd("strcasecmp()") (@std-strings).

=== Related

#cmd("memcmp()") (@std-string-memcmp), #cmd("strcoll()")
(@std-string-strcoll).

== strcoll, strxfrm <std-string-strcoll>

#idx("strcoll")#idx("strxfrm")

=== Format

```
#include <string.h>

int strcoll(const char *s1, const char *s2);
size_t strxfrm(char *s1, const char *s2, size_t n);
```

=== Description

#cmd("strcoll()") compares two strings according to the collating sequence
of the current locale. #cmd("strxfrm()") transforms the string #var("s2")
into #var("s1") so that #cmd("strcmp()") of two transformed strings gives the
same result as #cmd("strcoll()") of the originals. At most #var("n") bytes,
including the null character, are written.

libc370 supports only the #cmd("\"C\"") locale, whose collating sequence is
the EBCDIC code order. #cmd("strcoll()") is therefore #cmd("strcmp()"), and
#cmd("strxfrm()") copies the string unchanged.

=== Returns

#cmd("strcoll()") returns what #cmd("strcmp()") returns.

#cmd("strxfrm()") returns the length of #var("s2"), not counting the null
character. If that length is #var("n") or more, nothing is written to
#var("s1"): the transformed string does not fit.

=== Related

#cmd("strcmp()") (@std-string-strcmp)\; #cmd("setlocale()") in @std-locale.

== strcpy, strncpy <std-string-strcpy>

#idx("strcpy")#idx("strncpy")

=== Format

```
#include <string.h>

char *strcpy(char *s1, const char *s2);
char *strncpy(char *s1, const char *s2, size_t n);
```

=== Description

#cmd("strcpy()") copies the string #var("s2"), including its terminating null
character, into #var("s1").

#cmd("strncpy()") copies at most #var("n") characters of #var("s2") into
#var("s1"). If #var("s2") is shorter than #var("n"), the rest of the
#var("n") bytes is filled with null characters\; if it is #var("n") or longer,
#var("s1") receives no terminating null character.

=== Returns

#var("s1").

=== Related

#cmd("memcpy()") (@std-string-memcpy), #cmd("strdup()")
(@std-string-strdup).

== strcspn, strpbrk, strspn <std-string-strcspn>

#idx("strcspn")#idx("strpbrk")#idx("strspn")

=== Format

```
#include <string.h>

size_t strcspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
size_t strspn(const char *s1, const char *s2);
```

=== Description

#cmd("strspn()") returns the length of the initial part of #var("s1") that
consists only of characters from #var("s2"). #cmd("strcspn()") returns the
length of the initial part of #var("s1") that consists only of characters
#emph[not] in #var("s2"). #cmd("strpbrk()") locates the first character of
#var("s1") that occurs in #var("s2").

=== Returns

#cmd("strspn()") and #cmd("strcspn()") return a length.
#cmd("strpbrk()") returns a pointer to the character found, or #cmd("NULL")
if no character of #var("s2") occurs in #var("s1").

=== Related

#cmd("strtok()") (@std-string-strtok).

== strdup <std-string-strdup>

#idx("strdup")

=== Format

```
#include <string.h>

char *strdup(const char *s);
```

=== Description

#cmd("strdup()") allocates storage with #cmd("calloc()") and copies the
string #var("s") into it, with its terminating null character.

=== Returns

A pointer to the copy, or #cmd("NULL") if #var("s") is #cmd("NULL") or the
storage could not be obtained.

=== Errors

#deflist(width: 1.1in,
  [#cmd("ENOMEM")], [The storage could not be obtained (set by
    #cmd("malloc()")).],
)

=== Notes

Release the copy with #cmd("free()"). #cmd("strdup()") is a POSIX function,
not part of C99. A null #var("s") is accepted, which POSIX does not require.

=== Related

#cmd("malloc()") and #cmd("free()") in @std-stdlib.

== strerror <std-string-strerror>

#idx("strerror")#idx("error messages")

=== Format

```
#include <string.h>

char *strerror(int errnum);
```

=== Description

#cmd("strerror()") returns the text of the message for the error number
#var("errnum"), usually a value of #cmd("errno"). The messages are in
English, for example #cmd("\"Out of memory\"") for #cmd("ENOMEM").

=== Returns

A pointer to the message, or #cmd("NULL") if #var("errnum") is negative or
greater than 131. #cmd("strerror(0)") returns #cmd("\"No Error\"").

=== Notes

- The messages are constant strings. The header declares the return type
  #cmd("char *"), as C99 does, but the program must not modify them.
- The error numbers of the VSAM functions, #cmd("EVSTYPE") (150) to
  #cmd("EVSERROR") (154), have no message: #cmd("strerror()") returns
  #cmd("NULL") for them. C99 requires a string for every value, so test for
  #cmd("NULL") before printing the result, as in the example.
- The list of error numbers is in @std-errno.

=== Example

#code(read("../ex/std-string/strerror.c"))

=== Related

#cmd("perror()") in @std-stdio.

== strlen <std-string-strlen>

#idx("strlen")

=== Format

```
#include <string.h>

size_t strlen(const char *s);
```

=== Description

#cmd("strlen()") counts the characters of the string #var("s") up to, but not
including, the terminating null character.

=== Returns

The length of the string.

== strstr <std-string-strstr>

#idx("strstr")

=== Format

```
#include <string.h>

char *strstr(const char *s1, const char *s2);
```

=== Description

#cmd("strstr()") locates the first occurrence of the string #var("s2") in the
string #var("s1"). The terminating null character of #var("s2") is not
compared.

=== Returns

A pointer to the start of the occurrence, or #cmd("NULL") if #var("s2") does
not occur in #var("s1").

=== Notes

If #var("s2") is the empty string, C99 requires the result #var("s1").
libc370 returns #cmd("NULL") instead.

=== Related

#cmd("strchr()") (@std-string-strchr).

== strtok <std-string-strtok>

#idx("strtok")#idx("tokens")

=== Format

```
#include <string.h>

char *strtok(char *s1, const char *s2);
```

=== Description

A sequence of calls to #cmd("strtok()") splits the string #var("s1") into
tokens, each delimited by one or more characters from the string #var("s2").
The first call passes #var("s1")\; each later call passes #cmd("NULL") and
continues where the previous one stopped. The set of delimiters #var("s2")
may differ from call to call.

Each call skips the delimiters at the current position, then overwrites the
first delimiter after the token with a null character and returns the
token.

=== Returns

A pointer to the next token, or #cmd("NULL") when no token is left.

=== Notes

- #cmd("strtok()") modifies #var("s1"). Do not pass a string literal.
- The position is kept for each MVS task separately, so two tasks of one
  program can tokenize different strings at the same time. Within one task
  there is one position: a routine of the program that calls
  #cmd("strtok()") itself disturbs a caller that is in the middle of a
  sequence. The functions of libc370 itself do not disturb it.
- Several adjacent delimiters count as one, so an empty field between two
  delimiters produces no token.

=== Example

#code(read("../ex/std-string/strtok.c"))

=== Related

#cmd("strspn()") and #cmd("strcspn()") (@std-string-strcspn).
