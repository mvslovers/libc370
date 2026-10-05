#import "../bookmaster/bookmaster.typ": *

= Extensions <ext>

#idx("extensions")
The headers under #cmd("ext/") hold functions that are neither standard C
nor MVS services: general helpers that the library uses itself and offers
to programs. They are particular to this library, and a program that uses
them is not portable to other C libraries.

#tab(caption: [Extension headers])[
  #table(columns: (1.35in, 1fr),
    [Header], [Provides],
    [#cmd("<ext/array.h>")], [dynamic arrays of pointers that grow as items
      are added (@ext-array)],
    [#cmd("<ext/int64.h>")], [the #cmd("__64") type: 64-bit unsigned
      arithmetic through function calls (@ext-int64)],
    [#cmd("<ext/time64.h>")], [time values and conversions with 64 bits,
      past the year 2038, and in milliseconds and microseconds
      (@ext-time64)],
    [#cmd("<ext/strutil.h>")], [copies into fixed-length, padded fields, a
      wildcard match, and a fast clear (@ext-strutil)],
    [#cmd("<ext/version.h>")], [the version of the library linked into a
      program (@ext-version)],
  )
] <ext-headers-tab>

== Dynamic Arrays <ext-array>

#idx("dynamic array")
#idx("ARRAY")
The functions of #cmd("<ext/array.h>") manage an array of pointers that
grows as items are added. The program holds a pointer to the first item, as
it would for any array of pointers, and reads the items directly:
#cmd("names[0]"), #cmd("names[1]") and so on. The functions find the size and
the count of the array in a 12-byte header (#cmd("ARRAY"), eyecatcher
#cmd("ARRY")) that the library keeps in front of the first item.

Every function except #cmd("arraynew()") takes the _address_ of the
program's pointer, declared as #cmd("void *"), because adding an item may
move the array and change the pointer. A program declares

```
char **names = NULL;
```

and passes #cmd("&names"). A NULL pointer is an empty array\; the first
#cmd("arrayadd()") creates it.

The array holds pointers only. The functions never copy or free the items
they point to, except that #cmd("arrayaddf()") allocates the string it adds.
The storage of the array comes from #cmd("calloc()") and is returned with
#cmd("free()").

#tab(caption: [Array functions and their other names])[
  #table(columns: (1.2in, 1fr),
    [Function], [Also declared as],
    [#cmd("arraynew()")], [#cmd("array_new")],
    [#cmd("arrayadd()")], [#cmd("array_add")],
    [#cmd("arrayaddf()")], [#cmd("array_adf"), #cmd("array_addf")],
    [#cmd("arraycount()")], [#cmd("arraycou"), #cmd("array_cou"),
      #cmd("array_count")],
    [#cmd("arraysize()")], [#cmd("array_siz"), #cmd("array_size")],
    [#cmd("arrayget()")], [#cmd("array_get")],
    [#cmd("arraydel()")], [#cmd("array_del")],
    [#cmd("arrayeach()")], [#cmd("array_each")],
    [#cmd("arrayfree()")], [#cmd("array_fre"), #cmd("array_free")],
  )
] <ext-array-names-tab>

The names in each row are one function: the header gives them the same
external name. The functions do no locking\; an array shared by tasks must
be serialized by the program.

#fig(caption: [NAMES, a dynamic array of strings])[
  #code(read("../ex/ext/names.c"), numbers: true)
] <ext-names-fig>

== arrayadd, arrayaddf <ext-arrayadd>

#idx("arrayadd")
#idx("arrayaddf")
=== Format

```
#include <ext/array.h>

int arrayadd(void *varray, void *vitem);
int arrayaddf(void *varray, const char *fmt, ...);
```

=== Description

#cmd("arrayadd()") adds the pointer #var("vitem") at the end of the array
whose address is #var("varray"). When the array pointer is NULL, an array
for 20 items is created first. When the array is full, it is replaced by one
with room for 20 more items, and the program's pointer is changed.

#cmd("arrayaddf()") formats a string as #cmd("sprintf()") does, copies it to
storage from #cmd("calloc()"), and adds that.

=== Returns

0, or -1 for an error.

=== Errors

#deflist(width: 1.35in,
  [#cmd("EINVAL")], [#var("varray") is NULL, or its pointer does not
    point to an array made by these functions.],
  [#cmd("ENOMEM")], [there is no storage for the array.],
)

=== Notes

- Because the array may move, a pointer to one of its slots, kept from
  before the call, is no longer valid after it.
- #cmd("arrayaddf()") formats into a 1024-byte buffer on the stack. A string
  longer than 1023 characters overruns it.
- The string added by #cmd("arrayaddf()") belongs to the program, which frees
  it with #cmd("free()") before it frees the array.

=== Related

@ext-arraynew, @ext-arraydel

== arraycount, arraysize <ext-arraycount>

#idx("arraycount")
#idx("arraysize")
=== Format

```
#include <ext/array.h>

unsigned arraycount(void *varray);
unsigned arraysize(void *varray);
```

=== Description

#cmd("arraycount()") returns the number of items in the array whose address
is #var("varray")\; #cmd("arraysize()") returns the number it has room for.

=== Returns

The count or the size\; 0 when #var("varray") or its pointer is NULL, or
the pointer does not point to an array.

=== Notes

The items are #var("array")#cmd("[0]") to
#var("array")#cmd("[count-1]"). The slot after the last item is NULL only
while the array is not full\; use the count, not a NULL, to find the end.

== arraydel <ext-arraydel>

#idx("arraydel")
=== Format

```
#include <ext/array.h>

void *arraydel(void *varray, unsigned index);
```

=== Description

#cmd("arraydel()") removes item #var("index") from the array whose address is
#var("varray") and moves the items after it down by one. #var("index")
counts from 1.

=== Returns

The pointer that was removed, or NULL when #var("index") is 0 or larger
than the count.

=== Notes

The item itself is not freed.

== arrayeach <ext-arrayeach>

#idx("arrayeach")
=== Format

```
#include <ext/array.h>

int arrayeach(void *varray,
              int (*fn)(unsigned index, void *item, void *udata),
              void *udata);
```

=== Description

#cmd("arrayeach()") calls #var("fn") for each item of the array, in order,
with the position of the item, the item and #var("udata"). When #var("fn")
returns a value other than 0, #cmd("arrayeach()") stops and returns that
value.

=== Returns

The value at which #var("fn") stopped, or 0.

=== Notes

The #var("index") passed to #var("fn") counts from 0, unlike the
#var("index") of #cmd("arrayget()") and #cmd("arraydel()"), which counts from
1.

== arrayfree <ext-arrayfree>

#idx("arrayfree")
=== Format

```
#include <ext/array.h>

int arrayfree(void *varray);
```

=== Description

#cmd("arrayfree()") frees the array whose address is #var("varray") and sets
the program's pointer to NULL. The items are not freed\; free them first
if they belong to the program.

=== Returns

0, or -1 when #var("varray") or its pointer is NULL or the pointer does not
point to an array.

== arrayget <ext-arrayget>

#idx("arrayget")
=== Format

```
#include <ext/array.h>

void *arrayget(void *varray, unsigned index);
```

=== Description

#cmd("arrayget()") returns item #var("index") of the array whose address is
#var("varray"). #var("index") counts from 1.

=== Returns

The item, or NULL when #var("index") is 0 or larger than the count.

=== Notes

#cmd("arrayget(&a, n)") is #var("a")#cmd("[n-1]") with a range check.

== arraynew <ext-arraynew>

#idx("arraynew")
=== Format

```
#include <ext/array.h>

void *arraynew(unsigned size);
```

=== Description

#cmd("arraynew()") creates an empty array with room for #var("size") items,
or 20 when #var("size") is 0. Unlike the other array functions, it returns
the array itself, not through an address.

=== Returns

A pointer to the first slot of the array, or NULL when there is no
storage.

=== Notes

#cmd("arraynew()") is needed only to choose the first size: #cmd("arrayadd()")
creates an array when the pointer is NULL.

=== Example

```
void **ecbs = arraynew(4);      /* room for four ECB addresses */
```

== The \_\_64 Type <ext-int64>

#idx("__64")
#idx("64-bit arithmetic")
#cmd("<ext/int64.h>") defines #cmd("__64"), an unsigned 64-bit number, and
functions that compute with it. Each function takes pointers to its
operands and to its result, and the result may be one of the operands:

```
__64 n;

__64_from_u32(&n, 1000);
__64_mul_u32(&n, 1000, &n);         /* n = n * 1000 */
```

#cmd("__64") is the type of #cmd("time64_t") (@ext-time64). It dates from
compilers that had no 64-bit integer type\; cc370 compiles #cmd("long long")
and #cmd("uint64_t"), including their division, and new code uses those.
#cmd("__64_from_u64()") and #cmd("__64_to_u64()") convert between the two.

#tab(caption: [The \_\_64 type])[
  #table(columns: (1.6in, 1fr),
    [Member], [Contents],
    [#cmd("uint64_t u64")], [the value as one 64-bit integer],
    [#cmd("uint32_t u32[2]")], [the high word in #cmd("u32[0]"), the low
      word in #cmd("u32[1]")],
    [#cmd("uint16_t array[4]")], [the most significant halfword in
      #cmd("array[0]")],
  )
] <ext-int64-tab>

The three members are views of the same eight bytes, and the functions use
whichever suits them. The views agree only because System/370 stores the
most significant byte first, so #cmd("__64") gives wrong answers when the
library sources are compiled for a workstation with the opposite byte order.

#cmd("__64") has no sign. A negative #cmd("int32_t") operand is converted
to its two's complement in 64 bits, and #cmd("__64_cmp()") compares without
sign, so a negative value compares larger than any positive one.

The functions with the suffixes #cmd("_i32"), #cmd("_u32") and #cmd("_u64")
take their second operand as an #cmd("int32_t"), #cmd("uint32_t") or
#cmd("uint64_t") instead of a #cmd("__64 *"). Every function does nothing
when one of its pointers is NULL.

== \_\_64\_init, \_\_64\_from\_i32, \_\_64\_from\_u32, \_\_64\_from\_u64, \_\_64\_copy <ext-64-init>

#idx("__64_init")
#idx("__64_from_i32")
#idx("__64_from_u32")
#idx("__64_from_u64")
#idx("__64_copy")
#idx("__64_assign")
=== Format

```
#include <ext/int64.h>

void __64_init(__64 *n);
void __64_from_i32(__64 *n, int32_t i32);
void __64_from_u32(__64 *n, uint32_t u32);
void __64_from_u64(__64 *n, uint64_t u64);
void __64_copy(__64 *src, __64 *dst);
#define __64_assign(dst, src)  __64_copy((src), (dst))
```

=== Description

#cmd("__64_init()") sets #var("n") to 0. The #cmd("__64_from_") functions set
it to the value given\; a negative #var("i32") becomes its two's complement.
#cmd("__64_copy()") copies #var("src") to #var("dst").

=== Notes

The macro #cmd("__64_assign") takes its operands in the order of an
assignment, destination first\; #cmd("__64_copy()") takes them source first.
#cmd("__64_init()") does not test #var("n") for NULL.

== \_\_64\_to\_i32, \_\_64\_to\_u32, \_\_64\_to\_u64 <ext-64-to>

#idx("__64_to_i32")
#idx("__64_to_u32")
#idx("__64_to_u64")
=== Format

```
#include <ext/int64.h>

int32_t  __64_to_i32(__64 *n);
uint32_t __64_to_u32(__64 *n);
uint64_t __64_to_u64(__64 *n);
```

=== Description

These functions return the value of #var("n") as a C integer.
#cmd("__64_to_u32()") returns its low-order 32 bits, and #cmd("__64_to_i32()")
its low-order 31 bits.

=== Returns

The value, or 0 when #var("n") is NULL.

=== Notes

#cmd("__64_to_i32()") never returns a negative number: it does not reverse
#cmd("__64_from_i32()") for a negative value.

== \_\_64\_from\_string, \_\_64\_to\_string <ext-64-string>

#idx("__64_from_string")
#idx("__64_to_string")
=== Format

```
#include <ext/int64.h>

void __64_from_string(__64 *n, char *str);
void __64_to_string(__64 *n, char *str, int maxsize);
```

=== Description

#cmd("__64_from_string()") sets #var("n") to the number in #var("str"). It
skips leading blanks, reads hexadecimal digits after #cmd("0x") and
decimal digits otherwise, and stops at the first character that is not a
digit. There is no sign.

#cmd("__64_to_string()") stores #var("n") in #var("str") as a decimal number
with a terminating NUL. #var("maxsize") is the size of #var("str")\; 21
bytes hold any value.

=== Notes

- #cmd("__64_from_string()") does not report a string without digits, or a
  number too large, which wraps around. It accepts only a lowercase
  #cmd("0x")\; with #cmd("0X") the result is 0.
- The prefix #cmd("0b") is recognized, but no binary digits are read after
  it, and the result is 0.
- In decimal, a digit is added to the low-order 16 bits without a carry.
  When those bits overflow, the result is wrong: #cmd("\"65539\"") gives 3.
  For decimal input, #cmd("strtoull()") with #cmd("__64_from_u64()") is
  reliable.
- When #var("maxsize") is too small, #cmd("__64_to_string()") stores the
  low-order digits that fit, without the high-order ones and without a NUL.

== \_\_64\_add, \_\_64\_sub, \_\_64\_inc, \_\_64\_dec <ext-64-add>

#idx("__64_add")
#idx("__64_sub")
#idx("__64_inc")
#idx("__64_dec")
=== Format

```
#include <ext/int64.h>

void __64_add(__64 *a, __64 *b, __64 *c);       /* c = a + b */
void __64_add_i32(__64 *a, int32_t b, __64 *c);
void __64_add_u32(__64 *a, uint32_t b, __64 *c);
void __64_add_u64(__64 *a, uint64_t b, __64 *c);
void __64_sub(__64 *a, __64 *b, __64 *c);       /* c = a - b */
void __64_sub_i32(__64 *a, int32_t b, __64 *c);
void __64_sub_u32(__64 *a, uint32_t b, __64 *c);
void __64_sub_u64(__64 *a, uint64_t b, __64 *c);
void __64_inc(__64 *n);                         /* n = n + 1 */
void __64_dec(__64 *n);                         /* n = n - 1 */
```

=== Description

These functions add and subtract modulo 2#super[64]: a result that does not fit
wraps around, and no overflow is reported. #cmd("__64_add_i32") with a
negative #var("b") subtracts, and #cmd("__64_sub_i32") with a negative
#var("b") adds.

== \_\_64\_mul, \_\_64\_div, \_\_64\_mod, \_\_64\_divmod <ext-64-mul>

#idx("__64_mul")
#idx("__64_div")
#idx("__64_mod")
#idx("__64_divmod")
=== Format

```
#include <ext/int64.h>

void __64_mul(__64 *a, __64 *b, __64 *c);       /* c = a * b */
void __64_mul_i32(__64 *a, int32_t b, __64 *c);
void __64_mul_u32(__64 *a, uint32_t b, __64 *c);
void __64_mul_u64(__64 *a, uint64_t b, __64 *c);
void __64_div(__64 *a, __64 *b, __64 *c);       /* c = a / b */
void __64_div_i32(__64 *a, int32_t b, __64 *c);
void __64_div_u32(__64 *a, uint32_t b, __64 *c);
void __64_div_u64(__64 *a, uint64_t b, __64 *c);
void __64_mod(__64 *a, __64 *b, __64 *c);       /* c = a % b */
void __64_mod_i32(__64 *a, int32_t b, __64 *c);
void __64_mod_u32(__64 *a, uint32_t b, __64 *c);
void __64_mod_u64(__64 *a, uint64_t b, __64 *c);
void __64_divmod(__64 *a, __64 *b, __64 *c, __64 *d);
void __64_divmod_i32(__64 *a, int32_t b, __64 *c, __64 *d);
void __64_divmod_u32(__64 *a, uint32_t b, __64 *c, __64 *d);
void __64_divmod_u64(__64 *a, uint64_t b, __64 *c, __64 *d);
```

=== Description

#cmd("__64_mul()") multiplies modulo 2#super[64]. #cmd("__64_div()") divides and
discards the remainder, #cmd("__64_mod()") keeps only the remainder, and
#cmd("__64_divmod()") stores the quotient in #var("c") and the remainder in
#var("d").

=== Notes

- *Division by zero does not return:* #cmd("__64_div()") and every function
  built on it loop for ever. Test the divisor first.
- #cmd("__64_mul_i32") with a negative #var("b") divides #var("a") by
  #cmd("-")#var("b"), and #cmd("__64_div_i32") with a negative #var("b")
  multiplies by it. Use the #cmd("_u32") forms.
- #cmd("__64_mod_i32") and #cmd("__64_divmod_i32") use a negative
  #var("b") as its two's complement, a very large divisor.
- Division is done one bit at a time, in up to 64 steps.

== \_\_64\_and, \_\_64\_or, \_\_64\_xor <ext-64-and>

#idx("__64_and")
#idx("__64_or")
#idx("__64_xor")
=== Format

```
#include <ext/int64.h>

void __64_and(__64 *a, __64 *b, __64 *c);       /* c = a & b */
void __64_and_i32(__64 *a, int32_t b, __64 *c);
void __64_and_u32(__64 *a, uint32_t b, __64 *c);
void __64_and_u64(__64 *a, uint64_t b, __64 *c);
void __64_or(__64 *a, __64 *b, __64 *c);        /* c = a | b */
void __64_or_i32(__64 *a, int32_t b, __64 *c);
void __64_or_u32(__64 *a, uint32_t b, __64 *c);
void __64_or_u64(__64 *a, uint64_t b, __64 *c);
void __64_xor(__64 *a, __64 *b, __64 *c);       /* c = a ^ b */
void __64_xor_i32(__64 *a, int32_t b, __64 *c);
void __64_xor_u32(__64 *a, uint32_t b, __64 *c);
void __64_xor_u64(__64 *a, uint64_t b, __64 *c);
```

=== Description

These functions combine their operands bit by bit. An #cmd("int32_t")
operand is extended with its sign to 64 bits first, so
#cmd("__64_and_i32(&a, -1, &c)") copies all of #var("a").

== \_\_64\_lshift, \_\_64\_rshift <ext-64-shift>

#idx("__64_lshift")
#idx("__64_rshift")
=== Format

```
#include <ext/int64.h>

void __64_lshift(__64 *a, __64 *b, int nbits);   /* b = a << nbits */
void __64_rshift(__64 *a, __64 *b, int nbits);   /* b = a >> nbits */
void __64_lshift_one_bit(__64 *a);
void __64_rshift_one_bit(__64 *a);
void __64_lshift_word(__64 *a, int nwords);
void __64_rshift_word(__64 *a, int nwords);
```

=== Description

#cmd("__64_lshift()") and #cmd("__64_rshift()") shift #var("a") by
#var("nbits") bits and store the result in #var("b")\; the bits shifted in
are 0. The other four shift #var("a") in place: by one bit, or by
#var("nwords") 16-bit halfwords.

=== Notes

- A negative #var("nbits") does nothing\; #var("b") is not set.
- A shift by 64 bits or more gives 0. A shift by more than four halfwords
  -- for #cmd("__64_lshift()") and #cmd("__64_rshift()"), by 80 bits or more
  -- also writes a message to the operator's console.

== \_\_64\_cmp, \_\_64\_is\_zero <ext-64-cmp>

#idx("__64_cmp")
#idx("__64_is_zero")
=== Format

```
#include <ext/int64.h>

int __64_cmp(__64 *a, __64 *b);
int __64_cmp_i32(__64 *a, int32_t b);
int __64_cmp_u32(__64 *a, uint32_t b);
int __64_cmp_u64(__64 *a, uint64_t b);
int __64_is_zero(__64 *n);
```

=== Description

#cmd("__64_cmp()") compares #var("a") with #var("b") without sign.
#cmd("__64_is_zero()") tests #var("n") for 0.

=== Returns

#cmd("__64_cmp()") returns #cmd("__64_LARGER") (1) when #var("a") is larger,
#cmd("__64_EQUAL") (0) when the two are equal, and #cmd("__64_SMALLER")
(-1) when #var("a") is smaller. It returns #cmd("__64_EQUAL") when a
pointer is NULL.

#cmd("__64_is_zero()") returns 1 when #var("n") is 0 or NULL, and 0
otherwise.

=== Notes

#cmd("__64_cmp_i32") extends a negative #var("b") with its sign, so that
#cmd("__64_cmp_i32(&n, -1)") compares with the largest value.

== \_\_64\_pow, \_\_64\_isqrt <ext-64-pow>

#idx("__64_pow")
#idx("__64_isqrt")
=== Format

```
#include <ext/int64.h>

void __64_pow(__64 *a, __64 *b, __64 *c);       /* c = a to the power b */
void __64_isqrt(__64 *a, __64 *b);              /* b = square root of a */
```

=== Description

#cmd("__64_pow()") raises #var("a") to the power #var("b"), modulo 2#super[64]\;
any number to the power 0 is 1. #cmd("__64_isqrt()") stores in #var("b") the
integer square root of #var("a"), rounded down.

=== Notes

#cmd("__64_pow()") multiplies #var("b")#cmd("-1") times, so a large exponent
takes a long time.

== 64-Bit Time <ext-time64>

#idx("time64")
#idx("year 2038")
The time functions of #cmd("<time.h>") (see @std-time) count seconds since
1 January 1970 in a 32-bit #cmd("time_t"), which runs out in 2038. The
functions of #cmd("<ext/time64.h>") do the same work with 64-bit values,
and add values in milliseconds and microseconds.

#tab(caption: [Types of \<ext/time64.h\>])[
  #table(columns: (1.1in, 0.9in, 1fr),
    [Type], [Defined as], [Counts since 1 January 1970, 00:00:00 GMT],
    [#cmd("clock64_t")], [#cmd("uint64_t")], [seconds],
    [#cmd("mclock64_t")], [#cmd("uint64_t")], [milliseconds],
    [#cmd("uclock64_t")], [#cmd("uint64_t")], [microseconds],
    [#cmd("time64_t")], [#cmd("__64")], [seconds],
    [#cmd("mtime64_t")], [#cmd("__64")], [milliseconds],
    [#cmd("utime64_t")], [#cmd("__64")], [microseconds],
  )
] <ext-time64-tab>

The #cmd("time64_t") types are unions (see @ext-int64), not integers: they
cannot be compared with #cmd("==") or computed with operators. Use the
#cmd("__64") functions on them, or their member #cmd("u64"). The
#cmd("clock64_t") types are plain integers.

The clock is read with the #cmd("STCK") instruction, and the time-of-day
clock is taken to run on GMT, as #cmd("time()") takes it. Local time differs
from GMT by the offset that #cmd("localtime()") applies.

Years before 1970 are not supported. The functions that return a
#cmd("struct tm") or a string without taking a buffer use storage of the
calling task -- the same storage as #cmd("gmtime()"), #cmd("localtime()"),
#cmd("asctime()") and #cmd("ctime()") -- which the next call of any of them
overwrites.

#fig(caption: [ELAPSED, timing with 64-bit values])[
  #code(read("../ex/ext/elapsed.c"), numbers: true)
] <ext-elapsed-fig>

== asctime64, asctime64\_r <ext-asctime64>

#idx("asctime64")
#idx("asctime64_r")
=== Format

```
#include <ext/time64.h>

char *asctime64(const struct tm *tm);
char *asctime64_r(const struct tm *tm, char *result);
```

=== Description

Both functions format the time in #var("tm") as #cmd("asctime()") does, in
the 25 characters #cmd("Www Mmm dd hh:mm:ss yyyy") and a newline, followed
by a NUL. #cmd("asctime64_r()") stores them in #var("result"), which must have
room for 26 bytes\; #cmd("asctime64()") stores them in the task's storage.

=== Returns

The string, or NULL when #cmd("tm_wday") or #cmd("tm_mon") is out of
range, or the year is after 9999.

=== Related

@ext-ctime64

== clock64, mclock64, uclock64 <ext-clock64>

#idx("clock64")
#idx("mclock64")
#idx("uclock64")
=== Format

```
#include <ext/time64.h>

clock64_t  clock64(void);
mclock64_t mclock64(void);
uclock64_t uclock64(void);
```

=== Description

These functions return the current time as a 64-bit integer: in seconds,
milliseconds or microseconds since 1 January 1970, 00:00:00 GMT.

=== Returns

The time.

=== Notes

Despite its name, #cmd("clock64()") returns the time of day, not processor
time as #cmd("clock()") would. #cmd("clock64()") and #cmd("time64()") return
the same number of seconds.

=== Related

@ext-time64-fn

== ctime64, ctime64\_r, mctime64, uctime64 <ext-ctime64>

#idx("ctime64")
#idx("ctime64_r")
#idx("mctime64")
#idx("uctime64")
=== Format

```
#include <ext/time64.h>

char *ctime64(const time64_t *time);
char *ctime64_r(const time64_t *time, char *result);
char *mctime64(const mtime64_t *mtime);
char *uctime64(const utime64_t *utime);
```

=== Description

These functions convert a time to local time and format it as
#cmd("asctime64()") does. #cmd("mctime64()") and #cmd("uctime64()") take a time
in milliseconds or microseconds and drop the fraction of a second.
#cmd("ctime64_r()") stores the string in #var("result"), which must have room
for 26 bytes\; the others store it in the task's storage.

=== Returns

The string, or NULL for a time that cannot be formatted.
#cmd("ctime64_r()") returns NULL when #var("result") is NULL.

=== Related

@ext-asctime64, @ext-localtime64

== difftime64 <ext-difftime64>

#idx("difftime64")
=== Format

```
#include <ext/time64.h>

double difftime64(time64_t time1, time64_t time0);
```

=== Description

#cmd("difftime64()") returns #var("time1") minus #var("time0"), in seconds.

=== Returns

The difference, negative when #var("time1") is earlier.

=== Notes

Only the low-order 32 bits of the difference are converted. A difference
of more than 2#super[32] seconds, about 136 years, comes out wrong.

== gmtime64, gmtime64\_r, mgmtime64, ugmtime64 <ext-gmtime64>

#idx("gmtime64")
#idx("gmtime64_r")
#idx("mgmtime64")
#idx("ugmtime64")
=== Format

```
#include <ext/time64.h>

struct tm *gmtime64(const time64_t *timer);
struct tm *gmtime64_r(const time64_t *timer, struct tm *result);
struct tm *mgmtime64(const mtime64_t *mtimer);
struct tm *ugmtime64(const utime64_t *utimer);
```

=== Description

These functions break the time down into a #cmd("struct tm") in GMT, as
#cmd("gmtime()") does. #cmd("mgmtime64()") and #cmd("ugmtime64()") take a time in
milliseconds or microseconds and drop the fraction of a second.
#cmd("gmtime64_r()") stores the result in #var("result")\; the others in the
task's storage.

=== Returns

A pointer to the #cmd("struct tm").

=== Notes

#cmd("tm_isdst") is not set.

=== Related

@ext-localtime64, @ext-mktime64

== localtime64, localtime64\_r, mlocaltime64, ulocaltime64 <ext-localtime64>

#idx("localtime64")
#idx("localtime64_r")
#idx("mlocaltime64")
#idx("ulocaltime64")
=== Format

```
#include <ext/time64.h>

struct tm *localtime64(const time64_t *timer);
struct tm *localtime64_r(const time64_t *timer, struct tm *result);
struct tm *mlocaltime64(const mtime64_t *mtimer);
struct tm *ulocaltime64(const utime64_t *utimer);
```

=== Description

These functions break the time down into a #cmd("struct tm") in local time,
with the offset #cmd("localtime()") applies. #cmd("mlocaltime64()") and
#cmd("ulocaltime64()") take a time in milliseconds or microseconds and drop
the fraction of a second. #cmd("localtime64_r()") stores the result in
#var("result")\; the others in the task's storage.

=== Returns

A pointer to the #cmd("struct tm"), or NULL for an error.

=== Notes

- For a year after 2037 the offset is computed for an equivalent year
  within the range of #cmd("localtime()"), and the year is put back.
- #cmd("localtime64_r()") called with a NULL pointer writes a message to the
  operator's console as well as returning NULL.

=== Related

@ext-gmtime64, @ext-ctime64

== mktime64, timegm64 <ext-mktime64>

#idx("mktime64")
#idx("timegm64")
=== Format

```
#include <ext/time64.h>

time64_t mktime64(struct tm *tm);
time64_t timegm64(const struct tm *tm);
```

=== Description

Both functions convert a broken-down time to seconds since 1970.
#cmd("mktime64()") takes #var("tm") as local time, as #cmd("mktime()") does\;
#cmd("timegm64()") takes it as GMT and uses #cmd("tm_year"), #cmd("tm_mon"),
#cmd("tm_mday"), #cmd("tm_hour"), #cmd("tm_min") and #cmd("tm_sec").

=== Returns

The time, or a value with all 64 bits set when the year is before 1970,
or, for #cmd("mktime64()"), after 9999.

=== Notes

- For a year from 1970 to 2037 #cmd("mktime64()") is #cmd("mktime()"), and
  changes #var("tm") as #cmd("mktime()") does. For a later year it fills
  #var("tm") again from the result, as #cmd("gmtime64()") would.
- #cmd("timegm64()") does not normalize #var("tm")\; the fields must be in
  their ranges.

=== Related

@ext-gmtime64, @ext-localtime64

== time64, mtime64, utime64 <ext-time64-fn>

#idx("time64")
#idx("mtime64")
#idx("utime64")
=== Format

```
#include <ext/time64.h>

time64_t  time64(time64_t *timer);
mtime64_t mtime64(mtime64_t *mtimer);
utime64_t utime64(utime64_t *utimer);
```

=== Description

These functions return the current time in seconds, milliseconds or
microseconds since 1 January 1970, 00:00:00 GMT. When the argument is not
NULL, the time is also stored there.

=== Returns

The time.

=== Related

@ext-clock64

== String Helpers <ext-strutil>

#idx("strutil")
#cmd("<ext/strutil.h>") declares helpers for the fixed-length, blank-padded
fields that MVS uses for names, such as the eight characters of a member
name.

== memclr <ext-memclr>

#idx("memclr")
=== Format

```
#include <ext/strutil.h>

static __inline void *memclr(void *s, size_t n);
```

=== Description

#cmd("memclr()") sets #var("n") bytes at #var("s") to 0, with one
#cmd("MVCL") instruction.

=== Returns

#var("s").

=== Notes

- #cmd("memclr()") is an inline function in the header\; it is compiled
  into the program, and no library module is called.
- As for the inline #cmd("memset()") (see @std-string-memset), a program
  compiled against an earlier version of the header, which did not tell the
  compiler that the function writes storage, must be compiled again.

== memcpyp, strcpyp <ext-strcpyp>

#idx("memcpyp")
#idx("strcpyp")
=== Format

```
#include <ext/strutil.h>

void *memcpyp(void *target, int tlen, void *source, int slen, int pad);
char *strcpyp(char *target, int tlen, const void *source, int pad);
```

=== Description

Both functions fill a field of #var("tlen") bytes at #var("target"):
#cmd("memcpyp()") with the #var("slen") bytes at #var("source"),
#cmd("strcpyp()") with the string #var("source"). A source longer than the
field is cut off\; a shorter one is followed by the character #var("pad")
up to the end of the field.

=== Returns

#var("target").

=== Notes

- The field is not terminated with a NUL.
- A NULL #var("source") for #cmd("strcpyp()") fills the field with
  #var("pad").

=== Example

```
char member[8];

strcpyp(member, sizeof(member), "IEFBR14", ' ');   /* "IEFBR14 " */
```

== \_\_patmat <ext-patmat>

#idx("__patmat")
#idx("pattern match")
=== Format

```
#include <ext/strutil.h>

int __patmat(const char *str, const char *pat);
```

=== Description

#cmd("__patmat()") tests whether the string #var("str") matches the pattern
#var("pat"). In the pattern, #cmd("?") matches any one character and
#cmd("*") any characters\; every other character matches itself.

=== Returns

1 when #var("str") matches, 0 when it does not.

=== Notes

- A #cmd("*") at the end of the pattern matches any rest, including none.
  Anywhere else it matches one character or more: #cmd("\"ac\"") does not
  match #cmd("\"a*c\""), and #cmd("\"abc\"") does not match
  #cmd("\"*abc\"").
- Blanks at the end of #var("str") are ignored, so a blank-padded name
  matches as it would without the blanks.
- Upper and lower case differ. There is no way to match a #cmd("*") or
  #cmd("?") itself.
- #cmd("__patmat()") calls itself once for each character it matches, so the
  stack it needs grows with the length of #var("str").

=== Example

```
__patmat("SYS1.MACLIB", "SYS1.*");     /* 1 */
__patmat("SYS1.MACLIB", "SYS1.?ACLIB"); /* 1 */
```

== libc370\_version <ext-version>

#idx("libc370_version")
#idx("version", "of the library")
=== Format

```
#include <ext/version.h>

const char *libc370_version(void);
```

=== Description

#cmd("libc370_version()") returns a string naming the version of the library
that was linked into the program and the source revision it was built
from, in the form

```
LIBC370 2.3.1 (cfa5afd)
```

with #cmd("-dirty") added to the revision when the library was built from
changed sources. The string is fixed when the library is built.

=== Returns

A pointer to the string, which the program must not change.

=== Notes

A load module carries the library it was linked with, whatever is
installed later. A server that writes this string to its log when it
starts records which library it is running.
