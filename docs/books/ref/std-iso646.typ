#import "../bookmaster/bookmaster.typ": *

= \<iso646.h\> — Alternative Spellings <std-iso646>

#idx("iso646.h")
#idx("alternative spellings")
The header #cmd("<iso646.h>") defines eleven macros that spell operators as
words. They were introduced for keyboards and code pages that lack some of
the characters C uses, which gives them a use on MVS: the characters
#cmd("|"), #cmd("^") and #cmd("~") are at different code points in the
various EBCDIC code pages, and a terminal or printer set up for another code
page shows them as something else.

The header contains nothing but these macros. It declares no function and
defines no type.

#tab(caption: [Macros defined in \<iso646.h\>])[
  #table(columns: (1.2in, 1.2in, 1.2in, 1.2in),
    [Macro], [Expands to], [Macro], [Expands to],
    [#cmd("and")], [#cmd("&&")], [#cmd("not_eq")], [#cmd("!=")],
    [#cmd("and_eq")], [#cmd("&=")], [#cmd("or")], [#cmd("||")],
    [#cmd("bitand")], [#cmd("&")], [#cmd("or_eq")], [#cmd("|=")],
    [#cmd("bitor")], [#cmd("|")], [#cmd("xor")], [#cmd("^")],
    [#cmd("compl")], [#cmd("~")], [#cmd("xor_eq")], [#cmd("^=")],
    [#cmd("not")], [#cmd("!")], [], [],
  )
] <std-iso646-tab>

The macros change only how the source is written. The compiler translates
#cmd("|") and #cmd("^") to the code points of code page 037 whichever way
the operator is spelled. Since they are ordinary macros, they may also be
used in #cmd("#if") directives:

```
#include <iso646.h>
#include <limits.h>

#if CHAR_MIN == 0 and UCHAR_MAX == 255
/* char is unsigned and 8 bits wide */
#endif
```

#idx("digraphs")
The characters #cmd("[") #cmd("]") #cmd("{") #cmd("}") #cmd("#"), which the
header does not cover, have digraphs of their own in the language:
#cmd("<:") #cmd(":>") #cmd("<%") #cmd("%>") #cmd("%:"). cc370 accepts them
in its default mode, #cmd("-std=gnu99"), and in every C99 mode, but not
with #cmd("-std=c89").
