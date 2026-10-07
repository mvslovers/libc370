#import "../bookmaster/bookmaster.typ": *

= \<wchar.h\> — Wide Characters <std-wchar>

#idx("wchar.h")#idx("wide characters")
In libc370, #cmd("<wchar.h>") provides the wide-character types and their
limits, and nothing else. The C standard also places about sixty functions in
this header -- wide-character input and output (#cmd("fgetwc()"),
#cmd("wprintf()") ...), wide-string handling (#cmd("wcslen()"),
#cmd("wcscpy()") ...), numeric conversions (#cmd("wcstol()") ...) and the
restartable conversions (#cmd("mbrtowc()") ...). libc370 implements none of
them, and the header does not declare them, so a call fails at compile time
rather than at link time. The type #cmd("mbstate_t") is not defined either.

What a program can do with wide characters is in @std-stdlib: the multibyte
conversions #cmd("mblen()"), #cmd("mbtowc()"), #cmd("wctomb()"),
#cmd("mbstowcs()") and #cmd("wcstombs()"). The only multibyte encoding is the
single-byte one: #cmd("MB_CUR_MAX") is 1, every character is one EBCDIC byte,
and its wide value is the byte value, 0 to 255.
#idx("multibyte characters")

== Types and Macros <std-wchar-types>

@std-wchar-tab lists what the header defines. The sizes were measured with
the cc370 compiler.

#tab(caption: [Types and macros of \<wchar.h\>])[
  #table(columns: (1.3in, 1fr),
    [Name], [Definition on this target],
    [#cmd("wchar_t")], [#cmd("int"): 4 bytes, signed. It is the type the
      compiler gives to the elements of a wide string literal, so
      #cmd("wchar_t *p = L\"abc\"") is valid. #cmd("<stdlib.h>") and
      #cmd("<stddef.h>") define the same type.],
    [#cmd("wint_t")], [#cmd("unsigned int"): 4 bytes. It holds every
      #cmd("wchar_t") value and #cmd("WEOF").],
    [#cmd("size_t")], [#cmd("unsigned long"): 4 bytes.],
    [#cmd("WCHAR_MIN")], [#cmd("(-WCHAR_MAX - 1)"), that is -2147483648.],
    [#cmd("WCHAR_MAX")], [2147483647],
    [#cmd("WINT_MIN")], [#cmd("0U")],
    [#cmd("WINT_MAX")], [#cmd("4294967295U")],
    [#cmd("WEOF")], [#cmd("((wint_t)-1)"), that is #cmd("X'FFFFFFFF'").],
    [#cmd("NULL")], [#cmd("((void *)0)")],
  )
] <std-wchar-tab>

#cmd("WCHAR_MIN"), #cmd("WCHAR_MAX"), #cmd("WINT_MIN") and #cmd("WINT_MAX")
have the same values in #cmd("<stdint.h>") and may be used in #cmd("#if").

== Wide Character Constants <std-wchar-constants>

#idx("wide characters", "constants")
A wide character constant or wide string literal gets its values from the
compiler. With cc370, a character up to #cmd("U+00FF") has its code page 037
value, the same value as in a narrow literal: #cmd("L'a' == 'a'") is true,
and both are #cmd("X'81'"). A character above #cmd("U+00FF") keeps its
Unicode code point. A numeric escape such as #cmd("L'\\x41'") keeps the value
written.

Because the conversions in #cmd("<stdlib.h>") handle only the values 0 to
255, a wide string whose characters all come from code page 037 converts to
the same bytes as the narrow string, and back. A character above
#cmd("U+00FF") has no multibyte form: #cmd("wctomb()") and
#cmd("wcstombs()") report it as an error.

=== Example

#code(read("../ex/std-wchar/widen.c"))

=== Related

The multibyte conversions in @std-stdlib\; #cmd("<stdint.h>") limits in
@std-stdint.
