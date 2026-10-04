#import "../bookmaster/bookmaster.typ": *

= System/370 Interfaces <s370>

#idx("System/370")
The headers under #cmd("s370/") describe the machine rather than the
operating system: an atomic update of storage built on the
#cmd("COMPARE AND SWAP") instruction, the layout of a channel command word,
and the layout of a register save area.

#tab(caption: [System/370 headers])[
  #table(columns: (1.35in, 1fr),
    [Header], [Provides],
    [#cmd("<s370/atomic.h>")], [#cmd("__cas()"), #cmd("__swap()") and four
      counters, updated with #cmd("CS") (@s370-atomic)],
    [#cmd("<s370/ccw.h>")], [the #cmd("CCW") types, command codes and flags
      (@s370-ccw)],
    [#cmd("<s370/savearea.h>")], [the #cmd("SAVEAREA") type and inline
      functions that walk the chain of save areas (@s370-savearea)],
  )
] <s370-headers-tab>

== Atomic Updates <s370-atomic>

#idx("atomic update")
#idx("compare and swap")
#idx("CS instruction")
The functions of #cmd("<s370/atomic.h>") change a fullword of storage with
one #cmd("COMPARE AND SWAP") (#cmd("CS")) instruction, so that no other
task -- in the same address space, on any processor -- can change the word
between the time it is read and the time it is replaced. They are the
building blocks for counters, flags and lists that several tasks share
without a lock.

The word must be on a fullword boundary. #cmd("CS") on a word that is not
ends in a specification exception (ABEND S0C6). A variable of type
#cmd("unsigned") or #cmd("int") is aligned by the compiler\; a word inside
a packed structure or a byte buffer may not be.

#fig(caption: [CLAIM, taking a slot that other tasks may want])[
  #code(read("../ex/s370/claim.c"), numbers: true)
] <s370-claim-fig>

== \_\_cas <s370-cas>

#idx("__cas")
=== Format

```
#include <s370/atomic.h>

int __cas(unsigned *mem, unsigned *expect, unsigned new_value);
```

=== Description

#cmd("__cas()") compares the word at #var("mem") with the word at
#var("expect"). When they are equal, it stores #var("new_value") at
#var("mem"). When they are not, it leaves #var("mem") unchanged and stores
the value it found there at #var("expect"), ready for the next attempt.
The comparison and the store are one instruction.

=== Returns

0 when #var("new_value") was stored\; 1 when it was not\; -1 when
#var("mem") or #var("expect") is NULL.

=== Example

See @s370-claim-fig.

=== Related

@s370-swap

== \_\_inc, \_\_dec, \_\_uinc, \_\_udec <s370-inc>

#idx("__inc")
#idx("__dec")
#idx("__uinc")
#idx("__udec")
=== Format

```
#include <s370/atomic.h>

int      __inc(void *mem);
int      __dec(void *mem);
unsigned __uinc(void *mem);
unsigned __udec(void *mem);
```

=== Description

These functions add 1 to, or subtract 1 from, the fullword at #var("mem"),
as a signed (#cmd("__inc()"), #cmd("__dec()")) or unsigned (#cmd("__uinc()"),
#cmd("__udec()")) number. If another task changes the word at the same time,
the update is repeated with the new value, so no update is lost.

=== Returns

The value the word had _before_ the update, or 0 when #var("mem") is NULL.

=== Notes

The value wraps around at the end of its range:

#tab(caption: [Wrapping of the atomic counters])[
  #table(columns: (0.9in, 1.4in, 1fr),
    [Function], [Old value], [New value],
    [#cmd("__inc()")], [#cmd("INT_MAX")], [0],
    [#cmd("__dec()")], [#cmd("INT_MIN")], [#cmd("INT_MAX")],
    [#cmd("__uinc()")], [#cmd("UINT_MAX")], [0],
    [#cmd("__udec()")], [0], [#cmd("UINT_MAX")],
  )
] <s370-inc-tab>

#cmd("__inc()") does not wrap to #cmd("INT_MIN") as two's complement
arithmetic would: a signed counter goes from #cmd("INT_MAX") to 0, and so
never becomes negative by counting up.

=== Related

@s370-cas

== \_\_swap <s370-swap>

#idx("__swap")
=== Format

```
#include <s370/atomic.h>

unsigned __swap(unsigned *mem, unsigned new_value);
```

=== Description

#cmd("__swap()") stores #var("new_value") at #var("mem") and returns the value
that was there, as one update: no other task can change the word in
between.

=== Returns

The previous value, or 0 when #var("mem") is NULL.

=== Notes

- A NULL #var("mem") and a word that held 0 return the same value\; the
  test for NULL only prevents an abend.
- #cmd("__swap()") stores unconditionally. To store only when the word still
  holds an expected value, use #cmd("__cas()").
- Earlier versions of the library had this function under the name
  #cmd("__cs").

=== Related

@s370-cas

== Channel Command Words <s370-ccw>

#idx("CCW")
#idx("channel command word")
#cmd("<s370/ccw.h>") describes the channel command word (CCW), the
eight-byte instruction a channel program is made of. It is for programs
that build channel programs for #cmd("EXCP")\; it declares types and
constants only, and no function.

#tab(caption: [CCW types])[
  #table(columns: (0.7in, 1fr),
    [Type], [Layout],
    [#cmd("CCW0")], [#cmd("struct ccw0"): command code (8 bits) and data
      address (24 bits) in the first word\; #cmd("flags"),
      #cmd("reserved") and #cmd("count") (16 bits) in the second. This is
      the format of System/370.],
    [#cmd("CCW1")], [#cmd("struct ccw1"): #cmd("cmd"), #cmd("flags"),
      #cmd("count"), then a 31-bit #cmd("addr"). This is the format-1 CCW
      of later architectures\; MVS 3.8j channels do not take it.],
    [#cmd("CCW")], [a union of the two.],
  )
] <s370-ccw-tab>

All three are eight bytes. The initializer
#cmd("CCW0 c = { 0x02, 0x123456, 0x40, 0, 80 }") gives the bytes
#cmd("02 123456 40 00 0050"): command, address, flags, reserved, count.

The types are declared packed, so the compiler does not align them. A
channel program must start on a doubleword boundary\; place it where it
is, for example in a union with a #cmd("double").

#tab(caption: [CCW command codes])[
  #table(columns: (2.1in, 0.6in, 1fr),
    [Name], [Value], [Command],
    [#cmd("CCW_CMD_WRITE")], [#cmd("0x01")], [write (low-order bits 01)],
    [#cmd("CCW_CMD_READ")], [#cmd("0x02")], [read (low-order bits 10)],
    [#cmd("CCW_CMD_CONTROL")], [#cmd("0x03")], [control (low-order bits 11)],
    [#cmd("CCW_CMD_CONTROL_NOOP")], [#cmd("0x03")], [no operation],
    [#cmd("CCW_CMD_SENSE")], [#cmd("0x04")], [sense (low-order bits 0100)],
    [#cmd("CCW_CMD_SENSE_BASIC")], [#cmd("0x04")], [basic sense],
    [#cmd("CCW_CMD_SENSE_ID")], [#cmd("0xE4")], [sense ID],
    [#cmd("CCW_CMD_TIC")], [#cmd("0x08")], [transfer in channel],
    [#cmd("CCW_CMD_STLCK")], [#cmd("0x14")], [DASD],
    [#cmd("CCW_CMD_SUSPEND_RECONN")], [#cmd("0x5B")], [DASD],
    [#cmd("CCW_CMD_RDC")], [#cmd("0x64")], [DASD: read device
      characteristics],
    [#cmd("CCW_CMD_RELEASE")], [#cmd("0x94")], [DASD: release],
    [#cmd("CCW_CMD_SET_PGID")], [#cmd("0xAF")], [DASD: set path group ID],
    [#cmd("CCW_CMD_DCTL")], [#cmd("0xF3")], [DASD: diagnostic control],
    [#cmd("CCW_CMD_SENSE_PGID")], [#cmd("0x34")], [DASD: sense path group
      ID],
  )
] <s370-ccw-cmd-tab>

#cmd("SENSE_MAX_COUNT") is #cmd("0x20"), 32 bytes. Not every device
supports every DASD command\; several belong to control units newer than
the devices MVS 3.8j supports.

#tab(caption: [CCW flags])[
  #table(columns: (1.6in, 0.6in, 1fr),
    [Name], [Value], [Meaning],
    [#cmd("CCW_FLAG_CD")], [#cmd("0x80")], [chain data],
    [#cmd("CCW_FLAG_CC")], [#cmd("0x40")], [chain command],
    [#cmd("CCW_FLAG_SLI")], [#cmd("0x20")], [suppress incorrect length],
    [#cmd("CCW_FLAG_SKIP")], [#cmd("0x10")], [skip: transfer no data],
    [#cmd("CCW_FLAG_PCI")], [#cmd("0x08")], [program-controlled interruption],
    [#cmd("CCW_FLAG_IDA")], [#cmd("0x04")], [the address points to an IDAW
      list],
    [#cmd("CCW_FLAG_SUSPEND")], [#cmd("0x02")], [suspend (later
      architectures)],
  )
] <s370-ccw-flag-tab>

== Save Areas <s370-savearea>

#idx("save area")
#idx("SAVEAREA")
#cmd("<s370/savearea.h>") describes the 72-byte register save area of the
MVS linkage conventions, with the two words a C program adds to it, and
four inline functions that follow the chain of save areas. A program uses
them to find out who called it, for example in a trace or an error message.

#tab(caption: [The SAVEAREA structure])[
  #table(columns: (0.6in, 0.8in, 1fr),
    [Offset], [Member], [Contents],
    [0], [#cmd("sa0")], [used by PL/I\; not used by C],
    [4], [#cmd("prev")], [the address of the caller's save area],
    [8], [#cmd("next")], [the address of the save area of the routine
      called last],
    [12], [#cmd("r14")], [register 14, the return address],
    [16], [#cmd("r15")], [register 15, the entry point of the routine
      called last],
    [20--68], [#cmd("r0") -- #cmd("r12")], [registers 0 to 12],
    [72], [#cmd("lws")], [language work space (PL/I)],
    [76], [#cmd("nab")], [the next available byte of the C stack],
  )
] <s370-savearea-tab>

The structure is 80 bytes. A function compiled by cc370 takes its save
area, which is also its stack frame, from the C stack at the address in
#cmd("nab") of its caller's save area, and sets #cmd("nab") in its own to
the end of its frame.

== sa\_get, sa\_prev, sa\_next <s370-sa-get>

#idx("sa_get")
#idx("sa_prev")
#idx("sa_next")
=== Format

```
#include <s370/savearea.h>

static __inline SAVEAREA *sa_get(void);
static __inline SAVEAREA *sa_prev(SAVEAREA *sa);
static __inline SAVEAREA *sa_next(SAVEAREA *sa);
```

=== Description

#cmd("sa_get()") returns the save area of the function that calls it: the
address in register 13. #cmd("sa_prev()") returns the save area before
#var("sa") in the chain, the caller's, and #cmd("sa_next()") the one after
it\; a NULL #var("sa") stands for #cmd("sa_get()").

=== Returns

The address of the save area. #cmd("sa_prev()") and #cmd("sa_next()") return
what the chain holds, which may be 0 at its ends.

=== Notes

The functions are inline and compiled into the program. They do not check
the eyecatchers or addresses they follow.

== sa\_get\_epname <s370-sa-epname>

#idx("sa_get_epname")
=== Format

```
#include <s370/savearea.h>

static __inline void sa_get_epname(SAVEAREA *sa, char *epname);
```

=== Description

#cmd("sa_get_epname()") stores in #var("epname") the name of the routine
whose entry point is in the #cmd("r15") word of #var("sa"): the routine that
was called last by the owner of #var("sa"). It reads the name from the
identifier that the standard entry code places after a branch at the entry
point, which every function compiled by cc370 has. When the entry point
does not begin that way, it stores the address in hexadecimal followed by
#cmd("(unknown)").

The name of the current function is therefore
#cmd("sa_get_epname(sa_prev(sa_get()), buf)"), and that of its caller one
#cmd("sa_prev()") further.

=== Notes

- The identifier can be up to 255 characters\; #var("epname") must have
  room for the name and a NUL.
- The function calls #cmd("sprintf()"), so a program that uses it includes
  the formatting code of #cmd("printf()") in its load module.
- #cmd("<s370/savearea.h>") includes #cmd("<stdio.h>") and
  #cmd("<string.h>").
