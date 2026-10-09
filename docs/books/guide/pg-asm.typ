#import "../bookmaster/bookmaster.typ": *

= Calling Between C and Assembler <pg-asm>

#idx("assembler", "calling from C")#idx("linkage convention")
Some things are easier in assembler: an instruction that C has no word for,
such as #cmd("CVB") or #cmd("TR"), a system macro the library does not wrap,
a routine that already exists. cc370 compiles C to assembler source, and
as370 and ld370 put C and assembler object modules together in one load
module, so the two languages call each other directly. This chapter
explains how C names become external names, the linkage convention that a
call follows, how to write an assembler function that C can call and that
can call C, how to call a routine written for the standard OS convention,
and why a library should hold one function per object module.

== External Names <pg-asm-names>

#idx("external name", "eight characters")
An external name in an MVS object module has at most eight characters, in
upper case. cc370 makes one from each C name with external linkage --
a function or a variable -- in three steps: the name is cut to eight
characters, lowercase letters become uppercase, and the underscore becomes
#cmd("@"). So #cmd("pd2int") is #cmd("PD2INT"), and #cmd("amt_format") is
#cmd("AMT@FORM"). Names with static linkage do not appear in the object
module at all.

Two C names that agree in their first eight characters become the same
external name. When both are defined in one source, cc370 warns and as370
rejects the second definition, as @pg-asm-collide-fig shows: no object
module is written, and the return code is 1. When they are defined in
different sources, nothing reports the collision until the link -- or
at all, if one of them is only referenced and the other one satisfies the
reference.

#fig(caption: [Two C names that become one external name])[
  #code(read("../ex/pg-asm/collide.c"))
  #screen(raw(read("../ex/pg-asm/collide.txt")))
] <pg-asm-collide-fig>

#idx("asm", "external name")
To choose an external name yourself, declare the C name with an
#cmd("asm") label. The label is used exactly as written, so write it in
upper case:

#code(read("../ex/pg-asm/fixed.c"))

#idx("libc370", "external names")
The library gives most of its functions an #cmd("asm") label in the
header: #cmd("cthread_create()") is #cmd("@@CTCRTE"), #cmd("ecb_wait()") is
#cmd("@@ECBWT"). This matters when you read a link map, an unresolved
reference from ld370 or the save area trace of an abend: they show the
eight-character name. Search the headers for it to find the C function.

#note[#cmd("#pragma map") and #cmd("#pragma linkage") of the IBM compilers are
accepted by cc370, have no effect, and draw a warning that says so. Use an #cmd("asm") label to set an
external name\; there is only one linkage convention.]

== The Linkage Convention <pg-asm-linkage>

#idx("parameter list")#idx("register usage")
A call from C follows the standard MVS register conventions, with one
addition: the C stack. @pg-asm-regs-tab shows the registers at the entry of
a function and at its return.

#tab(caption: [Registers at a call])[
  #table(columns: (0.85in, 1fr, 1fr),
    [*Register*], [*At entry*], [*At return*],
    [1], [address of the parameter list], [as at entry],
    [13], [address of the caller's frame, which begins with a
      72-byte save area], [as at entry],
    [14], [return address], [as at entry],
    [15], [entry point address], [the return value],
    [0, 2-12], [--], [as at entry],
  )
] <pg-asm-regs-tab>

The callee saves registers 14 through 12 in the caller's save area and
restores all of them except register 15 before it returns.

*The parameter list holds the values of the arguments*, one fullword for
each #cmd("int"), #cmd("unsigned") or pointer, in the order of the
declaration. It does not hold their addresses, as the standard OS
convention does, and the high-order bit of the last entry is not set. For
other types -- #cmd("char"), #cmd("short"), #cmd("double"), structures --
compile a C caller with #cmd("-S") and read what it passes, before you
rely on a layout.

#idx("C stack", "frame")#idx("NAB")
*The C stack.* The start-up obtains the C stack in one piece before
#cmd("main()") is called (@pg-startup-stack). Each function takes a _frame_
from it as it is called. The fullword at offset 76 of the caller's frame
holds the next available byte of the stack, the _NAB_\; the callee takes its
frame there and stores the new NAB, past its own frame, at offset 76 of
that frame. A frame is laid out like this:

#deflist(width: 1.2in,
  [0-71], [the save area of the function: the registers of its callees are
    stored here.],
  [72-87], [reserved\; offset 76 holds the NAB.],
  [88 on], [the function's own data: the parameter lists for its calls and
    its automatic variables.],
)

Keep a frame a multiple of 8 bytes long, as the compiler does, so that every
frame begins on a doubleword boundary. @pg-asm-call-fig shows a C function
calling #cmd("PD2INT"): it builds the parameter list at offset 88 of its own
frame, points register 1 at it, and calls through a #cmd("V") constant.

#fig(caption: [A call from C, as cc370 compiles it])[
  #grid(columns: (auto, auto), column-gutter: 0.25in,
    code(read("../ex/pg-asm/call.c"), size: 7.5pt),
    code(read("../ex/pg-asm/call.s").split("\n")
      .filter(l => not l.starts-with("*")).join("\n"), size: 7.5pt))
] <pg-asm-call-fig>

#idx("PDPPRLG")#idx("PDPEPIL")
Every C function begins with the macro #cmd("PDPPRLG"), which saves the
registers and takes the frame, and ends with #cmd("PDPEPIL"), which
restores them. The macros come with the toolchain. The assembler functions
in this chapter write the same instructions out, so that every step can be
seen.

== Writing an Assembler Function for C <pg-asm-callee>

#idx("assembler function", "called from C")
To write an assembler function that C programs call:

+ Choose the external name, at most eight characters, and declare the
  function in a C header with an #cmd("asm") label of that name.
+ Name it in an #cmd("ENTRY") statement and put the label on the first
  instruction.
+ Save registers 14 through 12 in the caller's save area, at offset 12 of
  the area addressed by register 13.
+ Take a frame from the C stack, as described above, if the function needs
  a save area for calls of its own or storage to work in.
+ Pick up the arguments from the parameter list addressed by register 1.
+ Leave the result in register 15, restore registers 14 and 0 through 12,
  and return with #cmd("BR 14").

@pg-asm-pd2int-fig converts a packed decimal field into a binary integer
with #cmd("ZAP") and #cmd("CVB"), which need an 8-byte work area. The work
area is taken from the function's own frame on the C stack, at offset 88.

#fig(caption: [PD2INT, an assembler function for C])[
  #code(read("../ex/pg-asm/pd2int.asm"), numbers: true)
] <pg-asm-pd2int-fig>

*Keep the routine reentrant.* A save area or a work area defined with
#cmd("DS") in the control section of the routine is storage of the load
module, and storing into it makes the whole program non-reentrant and ends
it with #cmd("S0C4") when it runs from a link list library (@pg-rent).
The frame on the C stack belongs to the call, costs nothing to obtain, and
is released when the routine returns.

A routine that calls nothing and needs no storage can leave out the
frame: it saves and restores the registers and uses only those.

#cmd("PD2INT") does not check its input: a field that is not valid packed
decimal ends the program with a data exception (#cmd("S0C7")), and a value
that does not fit in 31 bits with a fixed-point divide exception
(#cmd("S0C9")). A routine written for others to call should check, or say
so in its header, as this one does in its comment.

== Calling C from Assembler <pg-asm-callc>

#idx("C function", "called from assembler")
An assembler routine that was itself called from C runs in the C
environment, and can call any C function, including the library's: it
takes a frame, builds a parameter list in it, and calls. @pg-asm-eachchr-fig
calls a C function, whose address it receives as an argument, once for each
character of a string.

#fig(caption: [EACHCHR, an assembler function that calls C])[
  #code(read("../ex/pg-asm/eachchr.asm"), numbers: true)
] <pg-asm-eachchr-fig>

The rules for the call are those of @pg-asm-linkage:

- Register 13 must address a frame on the C stack whose word at offset 76
  is the NAB, so that the C function can take its own frame. The frame of
  #cmd("EACHCHR") is that frame.
- The parameter list holds the argument values, here the character and the
  pointer #var("arg"), in the routine's own frame.
- Register 15 holds the address of the function. A C function pointer is
  simply that address\; for a function known by name, load it with a
  #cmd("V") constant: #cmd("L 15,=V(AMT@FORM)").
- The function returns its value in register 15 and preserves every other
  register, so #cmd("EACHCHR") keeps its own values in registers 2 to 5
  across the call.

@pg-asm-demo-fig declares both functions in a header and calls them from
C. A C function given to an assembler routine as a pointer may be
#cmd("static"), as #cmd("count_digits") is: the pointer carries its
address, and no external name is needed.

#fig(caption: [The header and a program that uses both functions])[
  #code(read("../ex/pg-asm/myutil.h"))
  #code(read("../ex/pg-asm/asmdemo.c"), numbers: true)
] <pg-asm-demo-fig>

#fig(caption: [Assembling and linking C and assembler together])[
  #screen(raw(read("../ex/pg-asm/build.txt")))
] <pg-asm-build-fig>

#fig(caption: [Output of ASMDEMO])[_Output to be captured on MVS._] <pg-asm-demo-out-fig>

The program should print an amount of 1234 and report six digits.

*An assembler program that is not called from C has no C environment*, and
cannot call a C function directly: there is no C stack and no run-time
anchor. Let it call a C _program_ instead -- a load module with the usual C
start-up, #cmd("@@CRT0") -- with #cmd("LINK"), #cmd("ATTACH") or
#cmd("CALL"). The C start-up then builds the environment, and releases it
when #cmd("main()") returns. @pg-startup describes the start-up modules,
including #cmd("crtm") for the special case of a C module called on a task
where a C program is already running.

== Calling a Routine That Expects the OS Convention <pg-asm-oslink>

#idx("VL parameter list")#idx("OS linkage")
Many existing assembler routines, and the IBM programs, expect the standard
OS convention: register 1 addresses a list of the _addresses_ of the
arguments, and the high-order bit of the last address is set. A C call
produces such a list when its arguments are pointers -- but cc370 does not
set the high-order bit. Set it yourself on the last argument:

#code(read("../ex/pg-asm/oslink.c"), numbers: true)

The same manual step is needed wherever an MVS service expects a list with
the last entry flagged, such as the text unit list of dynamic allocation or
the ECB list of #cmd("WAIT")\; the library does it inside the functions
that build those lists.

== One Function per Source File <pg-asm-onefunc>

#idx("autocall", "granularity")#idx("library", "member")
When ld370 resolves a reference from a library, it includes the _whole_
member that defines the symbol: every function in that object module, its
static data, and everything those functions refer to in turn. A member is
one object module, so the unit is the source file.

@pg-asm-lib-fig builds a library from the two assembler functions and
#cmd("amount.c"), which defines two C functions, #cmd("amt_format()") and
#cmd("amt_parse()"). The program #cmd("TOTAL") calls #cmd("pd2int()") and
#cmd("amt_format()") only.

#fig(caption: [A library with two functions in one member])[
  #screen(raw(read("../ex/pg-asm/lib-session.txt")))
] <pg-asm-lib-fig>

The link map shows #cmd("AMT@PARS") in the module as well, and with it
#cmd("sscanf()") and the scanner of the library that #cmd("amt_parse()")
calls. @pg-asm-split-fig puts each function into a source file of its own
and links again: the module is 10,600 bytes smaller (#cmd("X'113F8'")
against #cmd("X'13D60'")), for no change in what the program does.

#fig(caption: [The same library with one function per member])[
  #screen(raw(read("../ex/pg-asm/lib-split.txt")))
] <pg-asm-split-fig>

On MVS 3.8j, where the region below 16 MB holds everything a program uses,
that difference is worth having in every module that uses the library. The
C library itself is built this way, with one function in almost every
member. Do the same in your own libraries:

- Put each external function in a source file of its own. A static helper
  that only one function uses goes into that function's file.
- Name each file after its function, so that a name in a link map leads
  to the file. The library names its files after the external name in
  lower case: #cmd("@@ctcrte.c") holds #cmd("@@CTCRTE").
- Keep the data that several functions share in a source file of its own
  too, or better, on the heap (@pg-rent).

== Building a Library <pg-asm-lib>

#idx("ar370")#idx("library", "building")
A library is an archive of object modules with an index of the external
names they define. To build one and link against it:

+ Compile each C source with #cmd("cc370 -c") and assemble each assembler
  source with #cmd("as370 -o").
+ Create the library with #cmd("ar370 rc lib")#var("name")#cmd(".a")
  followed by all the object modules. ar370 builds the index from the
  external symbol dictionary of each module. A library that exists already
  is _replaced_, not updated, so name every member each time.
+ Check the index with #cmd("ar370 t"): every external name your programs
  call must be listed.
+ Link with #cmd("-L") #var("directory") #cmd("-l")#var("name"), as in
  @pg-asm-lib-fig.
+ To see what was taken from the library, write a link map with
  #cmd("-Wl,--map,")#var("file") and look for #cmd("autocall").

ld370 searches the index only: a symbol that is in a member but not in the
index -- because the library was built by another archiver -- is not found.
An MBT project builds its library this way when it declares one, and
publishes it with its header files for other projects to depend on. ar370
and ld370 are described in the _CC/370 Command Reference_, Chapter 4, “The
ar370 Command”, and Chapter 3, “The ld370 Command”.
