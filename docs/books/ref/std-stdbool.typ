#import "../bookmaster/bookmaster.typ": *

= \<stdbool.h\> — Boolean Type and Values <std-stdbool>

#idx("stdbool.h")
#idx("bool")
#idx("_Bool")
The header #cmd("<stdbool.h>") defines the name #cmd("bool") for the
compiler's Boolean type #cmd("_Bool"), and the values #cmd("true") and
#cmd("false"). It declares no function.

#tab(caption: [Macros defined in \<stdbool.h\>])[
  #table(columns: (2.3in, 1.2in, 1fr),
    [Macro], [Value], [Use],
    [#cmd("bool")], [#cmd("_Bool")], [the Boolean type],
    [#cmd("true")], [#cmd("1")], [an #cmd("int") constant],
    [#cmd("false")], [#cmd("0")], [an #cmd("int") constant],
    [#cmd("__bool_true_false_are_defined")], [#cmd("1")], [shows that the
      three names are defined],
  )
] <std-stdbool-tab>

An object of type #cmd("_Bool") occupies one byte. Any scalar value
assigned to it is converted to 0 or 1: a nonzero value, including a nonzero
pointer, becomes 1.

All four names are macros, so a program may #cmd("#undef") them and define
its own, as older programs sometimes do.
