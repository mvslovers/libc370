#import "../bookmaster/bookmaster.typ": *

= \<stdlib.h\> — General Utilities <std-stdlib>

#idx("stdlib.h")
The header #cmd("<stdlib.h>") declares the functions for storage allocation,
program termination, the environment, numeric conversion, sorting and
searching, pseudo-random numbers, integer arithmetic and multibyte
characters. @std-stdlib-tab lists its types and macros.

#tab(caption: [Types and macros of \<stdlib.h\>])[
  #table(columns: (1.3in, 1fr),
    [Name], [Definition on this target],
    [#cmd("size_t")], [#cmd("unsigned long"), 4 bytes.],
    [#cmd("wchar_t")], [#cmd("int"), 4 bytes (see @std-wchar).],
    [#cmd("div_t")], [#cmd("struct { int quot; int rem; }"), 8 bytes.],
    [#cmd("ldiv_t")], [#cmd("struct { long int quot; long int rem; }"),
      8 bytes.],
    [#cmd("lldiv_t")], [#cmd("struct { long long int quot; long long int rem; }"), 16 bytes.],
    [#cmd("NULL")], [#cmd("((void *)0)")],
    [#cmd("EXIT_SUCCESS")], [0],
    [#cmd("EXIT_FAILURE")], [*12*, the conventional MVS return code for a
      severe error. On other systems it is usually 1.],
    [#cmd("RAND_MAX")], [32767],
    [#cmd("MB_CUR_MAX")], [1],
  )
] <std-stdlib-tab>
#idx("EXIT_FAILURE")#idx("EXIT_SUCCESS")

== Storage <std-stdlib-storage>

#idx("storage", "allocation")#idx("GETMAIN")#idx("heap")
#cmd("malloc()"), #cmd("calloc()") and #cmd("realloc()") obtain each block
with its own conditional #cmd("GETMAIN"), and #cmd("free()") returns it with
#cmd("FREEMAIN"). There is no heap that the library manages itself: the
system's storage management is the heap. This has some consequences:

- Each request is rounded up, together with an 8-byte prefix that the library
  keeps in front of the block, to a multiple of 64 bytes. A request for 1
  byte occupies 64 bytes, one for 60 bytes 128. Many small blocks are
  better allocated as one array.
- The storage lies below the 16-megabyte line, as all storage on MVS 3.8j,
  in the region of the job step. The region size, set by #cmd("REGION=")
  in the JCL, limits the total.
- The storage comes from subpool 0, unless the program has chosen another
  subpool for the task with the functions of #cmd("<mvs/storage.h>"). A
  block remembers its subpool, so #cmd("free()") needs no information
  about it.
- A block is aligned on a doubleword (8 bytes), which suffices for every
  type.

== Program Termination <std-stdlib-termination>

#idx("return code")#idx("condition code")#idx("program termination")
A program ends normally when #cmd("main()") returns or when it calls
#cmd("exit()"). The library then calls the functions registered with
#cmd("atexit()"), closes every open stream, releases its own storage and
returns to the caller of the program. The status -- the value returned by
#cmd("main()") or passed to #cmd("exit()") -- becomes the return code of
the program in register 15. For a batch job step it is the condition code
that the job log shows and that the #cmd("COND") parameter of later steps
tests. A JCL #cmd("COND") test can compare values from 0 to 4095.

#cmd("_Exit()") ends the program the same way, but without calling the
registered functions. #cmd("abort()") ends it with the return code 12.
None of these functions ends the program with an abend.

== Environment Variables <std-stdlib-env>

#idx("environment variables")#idx("SYSENV DD statement")#idx("ENVIRON DD statement")
MVS has no environment in the POSIX sense. libc370 keeps a list of
environment variables for each program, shared by all its tasks, and fills
it at start-up from a data set: the one allocated to the DD name
#cmd("SYSENV"), or, if there is no #cmd("SYSENV") DD statement, the one
allocated to #cmd("ENVIRON"). Without either, the list starts empty.

Each record of the data set that contains an equal sign sets one variable:

- The part before the first #cmd("=") is the name, the rest the value.
  Blanks at the start and end of the name, at the start of the value and at
  the end of the record are removed.
- A record that begins with #cmd("*") or #cmd("#") is a comment.
- If the last eight characters of a record longer than eight characters are
  all digits, they are taken for a sequence number and removed. A variable
  whose value ends in eight digits, such as #cmd("ACCT=12345678"), can
  therefore lose them.
- Records of up to 1023 characters are read.

The variable #cmd("TZ") from this data set sets the time zone (see
@std-time). @std-stdlib-env-fig shows the JCL that runs the program of the
example under #cmd("getenv()") with two variables.

#fig(caption: [Environment variables in the SYSENV data set])[
  #code(read("../ex/std-stdlib/env.jcl"))
] <std-stdlib-env-fig>

Besides the functions of this chapter, #cmd("<mvs/env.h>") declares three
extensions: #cmd("getenvi()"), which finds a name regardless of case,
#cmd("setenvi()"), which sets an integer value, and #cmd("loadenv()"),
which reads further #cmd("name=value") records from any data set.

== abort <std-stdlib-abort>

#idx("abort")

=== Format

```
#include <stdlib.h>

void abort(void);
```

=== Description

#cmd("abort()") raises the signal #cmd("SIGABRT"). If a handler for
#cmd("SIGABRT") is installed, it is called. If there is none, or the
handler returns, or the signal is ignored, #cmd("abort()") calls
#cmd("exit(EXIT_FAILURE)").

=== Returns

#cmd("abort()") does not return, unless a #cmd("SIGABRT") handler leaves it
with #cmd("longjmp()").

=== Notes

- The program ends normally, with return code 12. It does not abend and
  produces no dump.
- Because #cmd("abort()") goes through #cmd("exit()"), the functions
  registered with #cmd("atexit()") are called and the streams are flushed
  and closed. C99 describes #cmd("abort()") as abnormal termination, which
  does not call them.

=== Related

#cmd("exit()") (@std-stdlib-exit)\; #cmd("signal()") and #cmd("raise()") in
@std-signal.

== abs, labs, llabs <std-stdlib-abs>

#idx("abs")#idx("labs")#idx("llabs")

=== Format

```
#include <stdlib.h>

int abs(int j);
long int labs(long int j);
long long int llabs(long long int j);
```

=== Description

These functions compute the absolute value of #var("j").

=== Returns

The absolute value. The absolute value of the most negative number of the
type cannot be represented\; the behavior is then undefined, as in C99.

=== Related

#cmd("div()") (@std-stdlib-div)\; #cmd("fabs()") in @std-math.

== atexit <std-stdlib-atexit>

#idx("atexit")

=== Format

```
#include <stdlib.h>

int atexit(void (*func)(void));
```

=== Description

#cmd("atexit()") registers the function #var("func") to be called when the
program ends normally, by #cmd("exit()") or by a return from
#cmd("main()"). The functions are called in the reverse order of their
registration, before the streams are closed, so they may still write output.

=== Returns

0 if the function was registered\; -1 if #var("func") is #cmd("NULL") or the
storage for the registration could not be obtained.

=== Notes

- The number of registrations is limited only by storage.
- The registered functions are not called by #cmd("_Exit()"). They are
  called by #cmd("abort()"), which ends through #cmd("exit()").

=== Related

#cmd("exit()") (@std-stdlib-exit).

== atof <std-stdlib-atof>

#idx("atof")

=== Format

```
#include <stdlib.h>

double atof(const char *nptr);
```

=== Description

#cmd("atof(nptr)") is #cmd("strtod(nptr, NULL)").

=== Returns

The converted value, or 0 if no number could be converted.

=== Notes

#cmd("strtod()") may set #cmd("errno") to #cmd("ERANGE")\; so does
#cmd("atof()").

=== Related

#cmd("strtod()") (@std-stdlib-strtod).

== atoi, atol, atoll <std-stdlib-atoi>

#idx("atoi")#idx("atol")#idx("atoll")

=== Format

```
#include <stdlib.h>

int atoi(const char *nptr);
long int atol(const char *nptr);
long long int atoll(const char *nptr);
```

=== Description

#cmd("atoi(nptr)") is #cmd("(int)strtol(nptr, NULL, 10)"),
#cmd("atol(nptr)") is #cmd("strtol(nptr, NULL, 10)"), and
#cmd("atoll(nptr)") is #cmd("strtoll(nptr, NULL, 10)").

=== Returns

The converted value, or 0 if no number could be converted.

=== Notes

The base is always 10: #cmd("atoi(\"0x10\")") is 0. A value outside the
range of the type gives the limit of the type and sets #cmd("errno") to
#cmd("ERANGE"), as #cmd("strtol()") does. #cmd("int") and #cmd("long") are
both 32 bits.

=== Related

#cmd("strtol()") (@std-stdlib-strtol).

== bsearch <std-stdlib-bsearch>

#idx("bsearch")#idx("binary search")

=== Format

```
#include <stdlib.h>

void *bsearch(const void *key, const void *base,
              size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
```

=== Description

#cmd("bsearch()") searches the array #var("base") of #var("nmemb") elements
of #var("size") bytes for an element that matches the object #var("key").
The array must be sorted in ascending order according to #var("compar"),
which returns a negative value, 0 or a positive value when its first
argument is less than, equal to or greater than its second.

=== Returns

A pointer to a matching element, or #cmd("NULL") if there is none. If
several elements match, any of them may be returned.

=== Notes

*The arguments of #var("compar") are in the opposite order to C99.*
libc370 calls #cmd("compar(element, key)"), where C99 specifies
#cmd("compar(key, element)"). A comparison function whose two arguments have
the same type, as with #cmd("qsort()"), works either way. A function that
expects a key of another type as its first argument -- a string key, say,
searched in an array of structures -- does not work. Make the key an object
of the element type, as in the example.

=== Example

#code(read("../ex/std-stdlib/sort.c"))

=== Related

#cmd("qsort()") (@std-stdlib-qsort).

== calloc <std-stdlib-calloc>

#idx("calloc")

=== Format

```
#include <stdlib.h>

void *calloc(size_t nmemb, size_t size);
```

=== Description

#cmd("calloc()") allocates storage for an array of #var("nmemb") objects of
#var("size") bytes each and sets all of it to binary zeros. The size is
rounded up to a multiple of 8 and obtained with #cmd("malloc()").

=== Returns

A pointer to the storage, or #cmd("NULL") if it could not be obtained.

=== Errors

#deflist(width: 1.1in,
  [#cmd("ENOMEM")], [The product #var("nmemb") #sym.times #var("size") does
    not fit into 32 bits, or #cmd("malloc()") failed.],
)

=== Notes

- A product that does not fit into 32 bits is refused before
  #cmd("malloc()") is called, so no console message is written for it.
  Any other failure is a failure of #cmd("malloc()"), with its message.
- #cmd("calloc()") with a product of 0 returns #cmd("NULL"), as
  #cmd("malloc(0)") does.

=== Example

#code(read("../ex/std-stdlib/alloc.c"))

=== Related

#cmd("malloc()") (@std-stdlib-malloc), #cmd("free()") (@std-stdlib-free).

== div, ldiv, lldiv <std-stdlib-div>

#idx("div")#idx("ldiv")#idx("lldiv")

=== Format

```
#include <stdlib.h>

div_t div(int numer, int denom);
ldiv_t ldiv(long int numer, long int denom);
lldiv_t lldiv(long long int numer, long long int denom);
```

=== Description

These functions divide #var("numer") by #var("denom") and compute the
quotient and the remainder in one call.

=== Returns

A structure whose member #cmd("quot") holds the quotient, truncated toward
zero, and #cmd("rem") the remainder, which has the sign of #var("numer").

=== Notes

A #var("denom") of 0 is not checked. For #cmd("div()") and #cmd("ldiv()") it
causes a fixed-point-divide exception and ends the program with abend
#cmd("S0C9").

=== Related

#cmd("fmod()") in @std-math.

== exit, \_Exit <std-stdlib-exit>

#idx("exit")#idx("_Exit")

=== Format

```
#include <stdlib.h>

void exit(int status);
void _Exit(int status);
```

=== Description

#cmd("exit()") ends the program normally:

+ It calls the functions registered with #cmd("atexit()"), the last
  registered first.
+ It flushes and closes all open streams, including #cmd("stdout") and
  #cmd("stderr").
+ It releases the storage of the C run-time environment, including the
  environment variables.
+ It returns to the caller of the program with #var("status") in register
  15.

#cmd("_Exit()") does the same without step 1.

=== Returns

The functions do not return.

=== Notes

- #var("status") becomes the return code of the program and, for a batch
  job step, its condition code. #cmd("exit(7)") shows as
  #cmd("COND CODE 0007") in the job log. Use #cmd("EXIT_SUCCESS") (0) and
  #cmd("EXIT_FAILURE") (12), or the conventional MVS codes 4, 8 and 16.
- A return from #cmd("main()") with value #var("n") is equivalent to
  #cmd("exit(")#var("n")#cmd(")").
- #cmd("_Exit()") flushes and closes the streams too: C99 leaves that to the
  implementation.

=== Related

#cmd("atexit()") (@std-stdlib-atexit), #cmd("abort()")
(@std-stdlib-abort).

== free <std-stdlib-free>

#idx("free")

=== Format

```
#include <stdlib.h>

void free(void *ptr);
```

=== Description

#cmd("free()") releases the storage at #var("ptr"), which must have been
obtained from #cmd("malloc()"), #cmd("calloc()") or #cmd("realloc()") (or a
library function documented to use them, such as #cmd("strdup()")). The
storage is returned to MVS at once, to the subpool it came from. If
#var("ptr") is #cmd("NULL"), nothing happens.

=== Notes

A pointer that was not obtained from these functions, or that has already
been freed, is not detected. The #cmd("FREEMAIN") issued for it fails, and
the program abends.

=== Related

#cmd("malloc()") (@std-stdlib-malloc).

== getenv <std-stdlib-getenv>

#idx("getenv")

=== Format

```
#include <stdlib.h>

char *getenv(const char *name);
```

=== Description

#cmd("getenv()") searches the environment variables of the program (see
@std-stdlib-env) for #var("name"). The comparison is case-sensitive.

=== Returns

A pointer to the value, or #cmd("NULL") if no variable of that name exists.

=== Notes

- The value belongs to the library. Do not modify it, and do not use the
  pointer after the variable has been changed or removed with
  #cmd("setenv()"), #cmd("putenv()") or #cmd("unsetenv()"): the storage is
  then freed.
- To search regardless of case, use #cmd("getenvi()") from
  #cmd("<mvs/env.h>").

=== Example

#code(read("../ex/std-stdlib/env.c"))

The JCL in @std-stdlib-env-fig runs this program.

=== Related

#cmd("setenv()") (@std-stdlib-setenv).

== malloc <std-stdlib-malloc>

#idx("malloc")#idx("Out of memory message")

=== Format

```
#include <stdlib.h>

void *malloc(size_t size);
```

=== Description

#cmd("malloc()") allocates #var("size") bytes of storage, as described in
@std-stdlib-storage. The storage is not initialized.

=== Returns

A pointer to the storage, or #cmd("NULL") if it could not be obtained.

=== Errors

#deflist(width: 1.1in,
  [#cmd("ENOMEM")], [The storage could not be obtained, #var("size") is
    greater than 6 megabytes, or #var("size") is 0.],
)

=== Notes

- *Every failure writes a message to the console*: the text
  #cmd("Out of memory, bytes needed=")#var("n"), followed by a traceback of
  the calling routines. The message is meant for the operator\; the program
  still receives #cmd("NULL") and continues.
- #cmd("malloc(0)") returns #cmd("NULL"), sets #cmd("ENOMEM") and writes the
  message. C99 allows the null pointer, but a program that passes a length of
  0 harmlessly on other systems produces a console message here. Test for 0
  before the call.
- A single block can be at most 6 megabytes (6,291,456 bytes).
- A storage shortage is reported as #cmd("NULL"), not as an abend: the
  #cmd("GETMAIN") is conditional.

=== Example

See the example under #cmd("calloc()") (@std-stdlib-calloc).

=== Related

#cmd("calloc()") (@std-stdlib-calloc), #cmd("realloc()")
(@std-stdlib-realloc), #cmd("free()") (@std-stdlib-free).

== mblen, mbtowc, wctomb <std-stdlib-mblen>

#idx("mblen")#idx("mbtowc")#idx("wctomb")#idx("multibyte characters")

=== Format

```
#include <stdlib.h>

int mblen(const char *s, size_t n);
int mbtowc(wchar_t *pwc, const char *s, size_t n);
int wctomb(char *s, wchar_t wchar);
```

=== Description

The only multibyte encoding is the single-byte one: every character is one
byte, and there is no shift state.

#cmd("mblen()") determines the length of the multibyte character at
#var("s"). #cmd("mbtowc()") also converts it into a wide character, which it
stores in #cmd("*")#var("pwc") unless #var("pwc") is #cmd("NULL")\; the wide
value is the byte value, 0 to 255. #cmd("wctomb()") converts the wide
character #var("wchar") into one byte at #var("s").

=== Returns

When #var("s") is #cmd("NULL"), all three return 0: the encoding has no
shift state.

#cmd("mblen()") and #cmd("mbtowc()") return 0 if #var("s") points to the null
character, 1 for any other byte, and -1 if #var("n") is 0.

#cmd("wctomb()") returns 1, or -1 if #var("wchar") is negative or greater
than 255 and therefore has no single-byte form.

=== Notes

#cmd("errno") is not set: C99 requires #cmd("EILSEQ") for an invalid
character.

=== Related

#cmd("mbstowcs()") (@std-stdlib-mbstowcs)\; @std-wchar.

== mbstowcs, wcstombs <std-stdlib-mbstowcs>

#idx("mbstowcs")#idx("wcstombs")

=== Format

```
#include <stdlib.h>

size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n);
size_t wcstombs(char *s, const wchar_t *pwcs, size_t n);
```

=== Description

#cmd("mbstowcs()") converts the string #var("s") into wide characters, one
for each byte, and stores at most #var("n") of them, including the
terminating null wide character, in #var("pwcs").

#cmd("wcstombs()") converts the wide string #var("pwcs") into bytes and
stores at most #var("n") bytes, including the terminating null character,
in #var("s").

If the destination is #cmd("NULL"), nothing is stored and #var("n") is
ignored: the functions only compute the length the result would have.

=== Returns

The number of characters stored, not counting the terminating null
character, or #var("n") if the result was cut off at #var("n") (it then has
no terminator). #cmd("wcstombs()") returns #cmd("(size_t)-1") when it meets
a wide character that is negative or greater than 255\; the bytes before it
have been stored.

=== Example

See the example in @std-wchar.

=== Related

#cmd("mbtowc()") and #cmd("wctomb()") (@std-stdlib-mblen).

== qsort <std-stdlib-qsort>

#idx("qsort")#idx("sorting")

=== Format

```
#include <stdlib.h>

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
```

=== Description

#cmd("qsort()") sorts the array #var("base") of #var("nmemb") elements of
#var("size") bytes in ascending order, as determined by #var("compar"),
which returns a negative value, 0 or a positive value when its first
argument is less than, equal to or greater than its second.

=== Notes

- The sort is a quicksort. It is not stable: elements that compare equal
  may appear in any order.
- The function recurses only into the smaller part of each partition and
  loops over the larger, so the recursion depth is at most the base-2
  logarithm of #var("nmemb") -- 20 levels for a million elements.
- With #cmd("strcmp()") as the comparison, the order is the EBCDIC order
  (see @std-string).

=== Example

See the example under #cmd("bsearch()") (@std-stdlib-bsearch).

=== Related

#cmd("bsearch()") (@std-stdlib-bsearch).

== rand, srand <std-stdlib-rand>

#idx("rand")#idx("srand")#idx("random numbers")

=== Format

```
#include <stdlib.h>

int rand(void);
void srand(unsigned int seed);
```

=== Description

#cmd("rand()") computes the next number of a pseudo-random sequence.
#cmd("srand()") starts a new sequence with #var("seed"). Each MVS task has a
sequence of its own.

The generator is the linear congruential one of the C standard's example,
#cmd("next = next * 1103515245 + 12345"), with the result taken from bits
16 and up of #cmd("next").

=== Returns

#cmd("rand()") returns the next number of the sequence.

=== Notes

- *The results do not lie in the range 0 to #cmd("RAND_MAX").* The result is
  masked with #cmd("X'8FFF'") instead of #cmd("X'7FFF'"): bits 12 to 14 are
  always 0 and bit 15 is set about half the time. #cmd("rand()") returns
  only the values 0 to 4095 and 32768 to 36863, 8192 different values in
  all. A program that scales the result with #cmd("RAND_MAX") gets results
  outside its intended range. Until this is corrected, use
  #cmd("rand() & 0xFFF") for an evenly distributed value from 0 to 4095.
- Without #cmd("srand()") the sequence starts from the seed 0, not 1 as C99
  specifies: the first call returns 0.

=== Related

None.

== realloc <std-stdlib-realloc>

#idx("realloc")

=== Format

```
#include <stdlib.h>

void *realloc(void *ptr, size_t size);
```

=== Description

#cmd("realloc()") changes the size of the block #var("ptr") to #var("size")
bytes. It always allocates a new block with #cmd("malloc()"), copies the
contents of the old block -- as much as fits -- and frees the old block.
Bytes beyond the old size are not initialized.

If #var("ptr") is #cmd("NULL"), #cmd("realloc()") is #cmd("malloc(size)").
If #var("size") is 0, it frees #var("ptr") and returns #cmd("NULL").

=== Returns

A pointer to the new block, or #cmd("NULL") if it could not be obtained. In
that case the old block is unchanged and still allocated.

=== Errors

#deflist(width: 1.1in,
  [#cmd("ENOMEM")], [The new block could not be obtained.],
)

=== Notes

- The block is never extended in place, not even when it shrinks, so the
  result always differs from #var("ptr"), and enlarging a block needs room
  for both blocks at once.
- A failure writes the console message of #cmd("malloc()").

=== Example

See the example under #cmd("calloc()") (@std-stdlib-calloc).

=== Related

#cmd("malloc()") (@std-stdlib-malloc), #cmd("free()") (@std-stdlib-free).

== setenv, putenv, unsetenv <std-stdlib-setenv>

#idx("setenv")#idx("putenv")#idx("unsetenv")

=== Format

```
#include <stdlib.h>

int setenv(const char *name, const char *value, int rewrite);
int putenv(const char *str);
int unsetenv(const char *name);
```

=== Description

#cmd("setenv()") sets the environment variable #var("name") to
#var("value"). Blanks at the start and end of #var("name") and at the start
of #var("value") are removed. If a variable of that name exists already, it
is replaced when #var("rewrite") is not 0 and left alone otherwise. The
name and value are copied.

#cmd("putenv()") takes a string of the form #cmd("name=value") and calls
#cmd("setenv(name, value, 1)"). A string without #cmd("=") sets the variable
to an empty value.

#cmd("unsetenv()") removes the variable #var("name").

=== Returns

#cmd("setenv()") and #cmd("putenv()") return 0 if the variable was set or
left alone as requested, and 1 if #var("name") is empty, #var("value") is
#cmd("NULL"), or the storage could not be obtained. #cmd("unsetenv()")
always returns 0.

=== Notes

- *Different from POSIX:* #cmd("setenv()") and #cmd("putenv()") return 1,
  not -1, on failure, and set no #cmd("errno").
- #cmd("setenv()") looks for an existing variable first with the exact name
  and then regardless of case. #cmd("setenv(\"Path\", ...)") with
  #var("rewrite") therefore replaces a variable #cmd("PATH"), and the
  variable is afterwards called #cmd("Path"). #cmd("unsetenv()") and
  #cmd("getenv()") compare exactly.
- #cmd("putenv()") copies the string. POSIX makes the string itself part of
  the environment, so that a later change to it changes the variable\; in
  libc370 it does not.
- #cmd("putenv()") temporarily overwrites the #cmd("=") in #var("str") with a
  null character and restores it before it returns. #var("str") must
  therefore be in modifiable storage, not a string constant of a reentrant
  module.
- A #cmd("name") that contains #cmd("=") is not rejected.
- The functions do not change the time zone: call #cmd("tzset()") after
  setting #cmd("TZ") (see @std-time).

=== Related

#cmd("getenv()") (@std-stdlib-getenv).

== strtod, strtof, strtold <std-stdlib-strtod>

#idx("strtod")#idx("strtof")#idx("strtold")

=== Format

```
#include <stdlib.h>

double strtod(const char *nptr, char **endptr);
float strtof(const char *nptr, char **endptr);
long double strtold(const char *nptr, char **endptr);
```

=== Description

#cmd("strtod()") converts the initial part of the string #var("nptr") to a
#cmd("double"). It skips white space, then accepts an optional sign, a
sequence of decimal digits with an optional decimal point, and an optional
exponent: #cmd("e") or #cmd("E"), an optional sign and at least one digit.
An exponent without a digit is not part of the number.

If #var("endptr") is not #cmd("NULL"), a pointer to the first character
after the number is stored in #cmd("*")#var("endptr"), or #var("nptr") if
no number was found.

#cmd("strtof()") converts to #cmd("float") and #cmd("strtold()") to
#cmd("long double"), which is #cmd("double") on this target.

=== Returns

The converted value, or 0 if no number was found. On overflow
#cmd("strtod()") returns #cmd("HUGE_VAL") with the sign of the number, and
#cmd("strtof()") returns #cmd("FLT_MAX") with the sign of the number. On
underflow both return 0.

=== Errors

#deflist(width: 1.1in,
  [#cmd("ERANGE")], [The value is outside the range of the result type:
    above about 7.2E75 or below about 5.4E-79 in magnitude.],
)

=== Notes

- The range is that of hexadecimal floating point (see @std-math).
  Overflow and underflow are detected before any intermediate result is
  formed, so no exponent of any size causes a program check.
- The C99 forms #cmd("INF"), #cmd("INFINITY"), #cmd("NAN") and hexadecimal
  floating constants (#cmd("0x1.8p3")) are not recognized: the hardware has
  no infinity or NaN. #cmd("strtod(\"0x10\", ...)") converts the #cmd("0")
  and stops at the #cmd("x").
- At most 17 significant digits are used\; further digits only scale the
  value. The result is close to, but not always exactly, the nearest
  #cmd("double").
- #cmd("strtof()") reports overflow with #cmd("FLT_MAX") rather than an
  infinity, because #cmd("<math.h>") has no #cmd("HUGE_VALF").

=== Related

#cmd("atof()") (@std-stdlib-atof), #cmd("strtol()")
(@std-stdlib-strtol)\; #cmd("scanf()") in @std-stdio.

== strtol, strtoll, strtoul, strtoull <std-stdlib-strtol>

#idx("strtol")#idx("strtoll")#idx("strtoul")#idx("strtoull")

=== Format

```
#include <stdlib.h>

long int strtol(const char *nptr, char **endptr, int base);
long long int strtoll(const char *nptr, char **endptr, int base);
unsigned long int strtoul(const char *nptr, char **endptr, int base);
unsigned long long int strtoull(const char *nptr, char **endptr, int base);
```

=== Description

These functions convert the initial part of the string #var("nptr") to an
integer of the result type. They skip white space, accept an optional
#cmd("+") or #cmd("-"), and then the digits of the number in base
#var("base"), which is 0 or 2 to 36. The letters #cmd("a") to #cmd("z") or
#cmd("A") to #cmd("Z") stand for the digits 10 to 35.

If #var("base") is 16, the digits may be preceded by #cmd("0x") or
#cmd("0X"). If #var("base") is 0, the base is taken from the number: 16
after #cmd("0x") or #cmd("0X"), 8 after a leading #cmd("0"), 10 otherwise.

If #var("endptr") is not #cmd("NULL"), a pointer to the first character
after the number is stored in #cmd("*")#var("endptr"), or #var("nptr") if
no digits were found.

=== Returns

The converted value, or 0 if no digits were found. If the value is outside
the range of the type, #cmd("strtol()") returns #cmd("LONG_MIN") or
#cmd("LONG_MAX"), #cmd("strtoll()") #cmd("LLONG_MIN") or #cmd("LLONG_MAX"),
#cmd("strtoul()") #cmd("ULONG_MAX") and #cmd("strtoull()")
#cmd("ULLONG_MAX"). For #cmd("strtoul()") and #cmd("strtoull()") a number
with a minus sign is negated in the unsigned type, as C99 prescribes.

=== Errors

#deflist(width: 1.1in,
  [#cmd("ERANGE")], [The value is outside the range of the type.],
  [#cmd("EINVAL")], [#var("base") is negative, 1 or greater than 36. The
    result is 0 and #cmd("*")#var("endptr") is set to #var("nptr").],
)

=== Notes

- #cmd("long") is 32 bits and #cmd("long long") 64 bits.
- The letters are recognized by value, not by code arithmetic, so the
  conversion is correct for EBCDIC, whose letters are not contiguous.
- #cmd("0x") counts as a prefix only when a hexadecimal digit follows it.
  Otherwise the number is the #cmd("0") alone, and #cmd("*")#var("endptr")
  points to the #cmd("x").

=== Example

#code(read("../ex/std-stdlib/strtol.c"))

=== Related

#cmd("atoi()") (@std-stdlib-atoi), #cmd("strtod()") (@std-stdlib-strtod).

== system <std-stdlib-system>

#idx("system")#idx("ATTACH")#idx("TSO", "command from a program")

=== Format

```
#include <stdlib.h>

int system(const char *string);
```

=== Description

#cmd("system()") runs another program as a subtask of the calling program
and waits for it to end. There is no command interpreter: #var("string")
names a load module, optionally followed by a blank and a parameter, for
example #cmd("\"IEBGENER\"") or #cmd("\"MYPGM DEBUG,LIST\"").

The program name -- the characters before the first blank -- may have up to
eight characters\; it is converted to uppercase. The program is found as
for #cmd("ATTACH"): in the job library, the step library or the link list.

- In batch, and when the program runs under TSO by #cmd("CALL"), the
  parameter is passed as #cmd("EXEC PGM=") passes the #cmd("PARM") value: a
  halfword length followed by the text.
- When the calling program was itself started as a TSO command, the named
  program is started as a TSO command processor: it receives a command
  processor parameter list, with the parameter as its operands.

The parameter may be up to 247 characters long. The subtask runs in the same
address space and can use the DD statements of the job step.

=== Returns

The completion code of the subtask, as follows:

#deflist(width: 1.35in,
  [return code #var("n")], [The program ended normally with return code
    #var("n").],
  [#cmd("X'80sss000'")], [The program abended with system completion code
    #var("sss").],
  [#cmd("X'80000uuu'")], [The program abended with user completion code
    #var("uuu").],
  [#cmd("X'048060")#var("nn")#cmd("'")], [The #cmd("ATTACH") failed with
    return code #var("nn"), for example because the program was not found.],
  [#cmd("X'1400000")#var("n")#cmd("'")], [The arguments were not valid: an
    empty program name, a parameter that is too long, or a TSO command
    without a command processor parameter list of the caller.],
  [-1], [The program name is longer than eight characters.],
)

=== Notes

- #var("string") must not be #cmd("NULL"). C99 lets a program call
  #cmd("system(NULL)") to ask whether a command processor exists\; libc370
  does not check for it and takes the contents of low storage as the
  program name.
- The value is a completion code, not a status in the POSIX sense: test
  the high-order bit for an abend, as in the example.

=== Example

#code(read("../ex/std-stdlib/system.c"))

=== Related

#cmd("exit()") (@std-stdlib-exit)\; the program services in @mvs-program.
