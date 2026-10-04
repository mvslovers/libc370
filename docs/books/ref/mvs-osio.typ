#import "../bookmaster/bookmaster.typ": *

= Record I/O <mvs-osio>

#idx("mvs/osio.h")
The header #cmd("<mvs/osio.h>") gives a C program direct use of three MVS
access methods, one block at a time, without the buffering of the
#cmd("stdio") functions:

#deflist(width: 0.8in,
  [BSAM], [#cmd("osbdcb()"), #cmd("osbopen()"), #cmd("osread()"), #cmd("oswrite()"),
    #cmd("oscheck()") and #cmd("osbclose()") issue the #cmd("OPEN"),
    #cmd("READ"), #cmd("WRITE"), #cmd("CHECK") and #cmd("CLOSE") macros of
    the basic sequential access method.],
  [EXCP], [#cmd("osxdcb()"), #cmd("osxopen()"), #cmd("osxcalc()"),
    #cmd("osxread()"), #cmd("osxwrite()") and #cmd("osxclose()") run a channel
    program of the library's own with #cmd("EXCP") (SVC 0), to read or
    rewrite one block of a direct access data set by its block number.],
  [BDAM], [#cmd("osddcb()"), #cmd("osdopen()"), #cmd("osdread()"),
    #cmd("osdwrite()") and #cmd("osdclose()") use the basic direct access method
    with relative block addressing.],
)
#idx("BSAM")#idx("EXCP")#idx("BDAM")

Every function works on a DCB that the library allocates and that names a DD
statement, either one in the JCL or one allocated dynamically (see
@mvs-dynalloc). The header includes the IBM mappings the caller needs:
#cmd("DCB") and #cmd("EXITLIST") (#cmd("<ibm/mvs/dcbd.h>")), #cmd("DECB")
(#cmd("<ibm/mvs/ihadecb.h>")), #cmd("IOB"), #cmd("DEB") and #cmd("JFCB").

*Authorization.* None of these functions issues #cmd("TESTAUTH") or
#cmd("MODESET"), and none needs APF authorization: OPEN, CLOSE, READ, WRITE,
CHECK and EXCP on a data set allocated to the program are problem-state
services. Data set protection still applies, and #cmd("osxopen()") and
#cmd("osdopen()") always open for #cmd("UPDAT"), even when the program only
reads.

*Restrictions.* #cmd("osread()"), #cmd("oswrite()"), #cmd("osdread()") and
#cmd("osdwrite()") start the request and return; the program calls
#cmd("oscheck()") with the same DECB before it touches the buffer or reuses the
DECB. #cmd("osxread()") and #cmd("osxwrite()") wait for their own I/O. The DCBs
the library builds name no #cmd("EODAD") and no #cmd("SYNAD") routine, so
end of data and I/O errors arrive as abends (see @mvs-osio-oscheck). There
is no test of these functions in the library's test suites.

#tab(caption: [DCB fields a caller reads or sets])[
  #table(columns: (1.0in, 0.6in, 1fr),
    [Field], [Offset], [Use],
    [#cmd("dcbeodad")], [X'20'], [The end-of-data routine. The DCBs built
      here name none.],
    [#cmd("dcbexlst")], [X'24'], [The exit list address (before OPEN). See
      @mvs-osio-exitlist.],
    [#cmd("dcbrecfm")], [X'24'], [The record format byte (after OPEN).],
    [#cmd("dcbddnam")], [X'28'], [The DD name, eight characters padded with
      blanks.],
    [#cmd("dcboflg")], [X'30'], [Open flags. #cmd("DCBOFOPN") (X'10') is on
      when the OPEN succeeded.],
    [#cmd("dcbsynad")], [X'38'], [The I/O error routine. The DCBs built here
      name none.],
    [#cmd("dcbblksi")], [X'3E'], [The block size. Set by OPEN for BSAM, from
      the JFCB by #cmd("osxopen()") and #cmd("osdopen()"), and by #cmd("oswrite()"),
      #cmd("osdread()") and #cmd("osdwrite()") when they are given a length.],
    [#cmd("dcblrecl")], [X'52'], [The logical record length.],
    [#cmd("dcbuser")], [X'60'], [A word for the program. It follows the
      DCB proper and the library never uses it.],
    [#cmd("dcbpriv")], [X'64'], [Private to the library (the exit list). Do
      not change it.],
  )
] <mvs-osio-dcbfields>

#tab(caption: [EXITLIST entry])[
  #table(columns: (1.0in, 0.6in, 1fr),
    [Field], [Offset], [Use],
    [#cmd("exit_code")], [X'00'], [The entry code of the DCB #cmd("EXLST")
      operand, for example X'05' for the DCB open exit. Set X'80' in the code
      of the last entry. The header defines no names for the codes.],
    [#cmd("exit_addr")], [X'04'], [The address of the exit routine. The
      system calls it with standard OS linkage, not as a C function.],
  )
] <mvs-osio-exitlist>

#note[#cmd("<mvs/osio.h>") includes control block headers that use
#cmd("#pragma pack"), which cc370 ignores: the structures are laid out with
the natural alignment of their fields, which gives the offsets shown in this
chapter. With #cmd("-Wall -Werror") the warnings stop the compilation\; add
#cmd("-Wno-unknown-pragmas").]

== osbdcb, osxdcb, osddcb <mvs-osio-dcb>
#idx("osbdcb")#idx("osxdcb")#idx("osddcb")#idx("DCB", "allocating")

=== Format
```
#include <mvs/osio.h>

DCB *osbdcb(const char *ddname, EXITLIST *exitlist);
DCB *osxdcb(const char *ddname, EXITLIST *exitlist);
DCB *osddcb(const char *ddname);
```

=== Description
Each function allocates a DCB with #cmd("calloc()"), copies a model DCB into
it, and puts #var("ddname") into #cmd("dcbddnam"), in uppercase and cut at
eight characters. A prefix of two characters and a colon, such as
#cmd("DD:"), is skipped, so #cmd("\"DD:SYSUT1\"") and #cmd("\"SYSUT1\"") name
the same DD. #var("ddname") may be #cmd("NULL") when the program sets
#cmd("dcbddnam") itself.

#deflist(width: 0.8in,
  [#cmd("osbdcb()")], [BSAM: #cmd("DSORG=PS,MACRF=R")\; #cmd("osbopen()") sets
    the #cmd("MACRF") for the mode it is given.],
  [#cmd("osxdcb()")], [EXCP: #cmd("DSORG=PS,MACRF=E").],
  [#cmd("osddcb()")], [BDAM: #cmd("DSORG=DA,MACRF=(RIC,WIC),OPTCD=R") --
    read and write by relative block number, completed by #cmd("CHECK").],
)

When #var("exitlist") is not #cmd("NULL"), #cmd("osbdcb()") and
#cmd("osxdcb()") build a DCB exit list from it and store its address in
#cmd("dcbexlst"). They copy entries up to and including the first whose
#cmd("exit_code") has the X'80' bit on, at most ten, and mark the last entry
copied as the last.

=== Returns
The address of the DCB, or #cmd("NULL") when no storage was available.

=== Notes
- When the storage for the exit list cannot be obtained, the DCB is
  returned without an exit list, and nothing tells the caller.
- Free the DCB with the matching close function and a non-zero
  #var("freedcb")\; that also frees the exit list.

=== Related
@mvs-osio-osbopen, @mvs-osio-osbclose, @mvs-osio-osxopen, @mvs-osio-osdopen

== osbopen <mvs-osio-osbopen>
#idx("osbopen")#idx("OPEN", "BSAM")

=== Format
```
#include <mvs/osio.h>

int osbopen(DCB *dcb, int typej, const char *mode);
```

=== Description
#cmd("osbopen()") opens a DCB made by #cmd("osbdcb()"). Only the first
character of #var("mode") counts, in either case; @mvs-osio-modes lists them.

#tab(caption: [osbopen modes])[
  #table(columns: (1.6in, 0.9in, 1fr),
    [#var("mode")], [MACRF], [OPEN option],
    [#cmd("r") (read), #cmd("i") (input)], [#cmd("R")], [#cmd("INPUT")],
    [#cmd("w") (write), #cmd("o") (output)], [#cmd("W")], [#cmd("OUTPUT")],
    [#cmd("l") (load)], [#cmd("WL")], [#cmd("OUTPUT")],
    [#cmd("u") (update)], [#cmd("R"), #cmd("W")], [#cmd("UPDAT")],
  )
] <mvs-osio-modes>

With #var("mode") #cmd("NULL"), or a first character not in the table, the
#cmd("MACRF") already in the DCB is used. A non-zero #var("typej") issues
#cmd("OPEN TYPE=J") (SVC 22) instead of #cmd("OPEN") (SVC 19), for a program
that has changed the JFCB. The OPEN runs under an ESTAE, so an abend in OPEN
is returned rather than ending the program.

=== Returns
#deflist(width: 0.8in,
  [0], [The data set is open.],
  [8], [OPEN returned, but the DCB is not open.],
  [-1], [#var("dcb") is #cmd("NULL").],
  [other], [OPEN abended: the code as #cmd("0x00sssuuu"), where #cmd("sss")
    is the system and #cmd("uuu") the user completion code. A negative value
    other than -1 means that the ESTAE could not be established.],
)

=== Notes
There is no #cmd("EXTEND") mode: #cmd("w") writes from the start of the
data set unless its DD says #cmd("DISP=MOD").

=== Related
@mvs-osio-dcb, @mvs-osio-osread, @mvs-osio-osbclose

== osread, osbread <mvs-osio-osread>
#idx("osread")#idx("osbread")#idx("READ", "BSAM")

=== Format
```
#include <mvs/osio.h>

void osread(DECB *decb, DCB *dcb, void *buf, int length);
void osbread(DECB *decb, DCB *dcb, void *buf, int length);
```

=== Description
#cmd("osread()") issues #cmd("READ") with type #cmd("SF") for the next block
of the data set, into #var("buf") of #var("length") bytes, and returns
without waiting. #var("decb") is the program's own DECB; call
#cmd("oscheck()") with it before using the block. #cmd("osbread()") is the same
routine under a second name.

=== Notes
The length of the block that was read is not returned. For RECFM=V the block
descriptor word holds it; for RECFM=F every block except a short last one is
#cmd("dcbblksi") bytes long.

=== Related
@mvs-osio-oscheck, @mvs-osio-oswrite

== oswrite, osbwrite <mvs-osio-oswrite>
#idx("oswrite")#idx("osbwrite")#idx("WRITE", "BSAM")

=== Format
```
#include <mvs/osio.h>

int oswrite(DECB *decb, DCB *dcb, void *buf, int length);
int osbwrite(DECB *decb, DCB *dcb, void *buf, int length);
```

=== Description
#cmd("oswrite()") issues #cmd("WRITE") with type #cmd("SF") for a block of
#var("length") bytes from #var("buf") and returns without waiting. A
#var("length") greater than zero is first stored in #cmd("dcbblksi")\; zero
or less writes #cmd("dcbblksi") bytes. #cmd("osbwrite()") is the same routine
under a second name.

=== Returns
The return code of the #cmd("WRITE") macro, normally 0. Whether the block
was written is known only after #cmd("oscheck()").

=== Related
@mvs-osio-oscheck, @mvs-osio-osread

== oscheck <mvs-osio-oscheck>
#idx("oscheck")#idx("CHECK")

=== Format
```
#include <mvs/osio.h>

int oscheck(DECB *decb);
```

=== Description
#cmd("oscheck()") issues #cmd("CHECK") for #var("decb"): it waits for the READ
or WRITE that used the DECB to complete. The CHECK runs under an ESTAE.

=== Returns
0 when the I/O completed normally. Otherwise the abend that ended the CHECK,
as #cmd("0x00sssuuu"), or a negative value when the ESTAE could not be
established.

=== Notes
- At end of data, and on an I/O error, CHECK passes control to the
  #cmd("EODAD") or #cmd("SYNAD") routine of the DCB. The DCB that
  #cmd("osbdcb()") builds names neither, so both conditions end in an abend,
  which #cmd("oscheck()") returns. A program that reads to the end sees the
  end of data as a non-zero return from #cmd("oscheck()").
- #cmd("oscheck()") also completes the BDAM requests of #cmd("osdread()") and
  #cmd("osdwrite()").

=== Example
The program copies a RECFM=F data set block by block until #cmd("oscheck()")
reports the end of the input.

#code(read("../ex/mvs-osio/bsamcopy.c"))

=== Related
@mvs-osio-osread, @mvs-osio-oswrite, @mvs-osio-osdread

== osbclose <mvs-osio-osbclose>
#idx("osbclose")#idx("CLOSE", "BSAM")

=== Format
```
#include <mvs/osio.h>

void osbclose(DCB *dcb, const char *option, int freedcb, int type_t);
```

=== Description
#cmd("osbclose()") closes a BSAM DCB. #var("option") is one of the volume
positioning options of the CLOSE macro: #cmd("\"reread\""),
#cmd("\"leave\""), #cmd("\"rewind\""), #cmd("\"free\"") or #cmd("\"disp\"")\;
#cmd("NULL") or any other string means #cmd("DISP"). After the close the exit
list is freed, and the DCB as well when #var("freedcb") is non-zero.

A non-zero #var("type_t") issues #cmd("CLOSE TYPE=T") (SVC 23) instead: the
data set is repositioned but stays open, and nothing is freed, whatever
#var("freedcb") says.

=== Notes
A #cmd("NULL") #var("dcb") is ignored. The return code of CLOSE is not
reported.

=== Related
@mvs-osio-osbopen, @mvs-osio-dcb

== osxopen <mvs-osio-osxopen>
#idx("osxopen")#idx("OPEN", "EXCP")

=== Format
```
#include <mvs/osio.h>

int osxopen(DCB *dcb, int typej);
```

=== Description
#cmd("osxopen()") opens a DCB made by #cmd("osxdcb()") for #cmd("UPDAT")
(#cmd("OPEN TYPE=J") when #var("typej") is non-zero), under an ESTAE. OPEN
does not fill the data set attributes into an EXCP DCB, so after a
successful open #cmd("osxopen()") reads the JFCB and copies its block size and
record length into #cmd("dcbblksi") and #cmd("dcblrecl").

=== Returns
0 when the data set is open; 8 when it is not, which includes an abend in
OPEN; otherwise the return code of OPEN.

=== Notes
- #var("dcb") must not be #cmd("NULL").
- The block size comes from the JFCB, that is, from what the DD statement
  coded. When the DD does not code #cmd("BLKSIZE"), set #cmd("dcbblksi")
  before reading.

=== Related
@mvs-osio-osxread, @mvs-osio-osxclose

== osxcalc <mvs-osio-osxcalc>
#idx("osxcalc")#idx("MBBCCHHR")

=== Format
```
#include <mvs/osio.h>

int osxcalc(DCB *dcb, unsigned blkstrk, unsigned block, char *mbbcchhr);
```

=== Description
#cmd("osxcalc()") converts a block number of the data set, counted from 0, to
the 8-byte device address #cmd("MBBCCHHR") in #var("mbbcchhr"): extent number
(M), bin (BB, zero on disk), cylinder (CC), head (HH) and record (R).
#var("blkstrk") is the number of blocks on each track, so the block lies on
relative track #var("block")/#var("blkstrk"). The extents come from the DEB of
the open DCB, the tracks per cylinder from its device type
(@mvs-osio-devices).

=== Returns
The record number on the track, from 1; or -1 when the block lies beyond the
last extent or the device type is not in @mvs-osio-devices.

=== Notes
A #var("blkstrk") of 0 maps every block to record 1 of the first track.
#cmd("trkcalc()") (see @mvs-datasets) gives the number of blocks on a track.

#tab(caption: [Devices osxcalc supports])[
  #table(columns: (1fr, 1.3in),
    [Device], [Tracks per cylinder],
    [2311], [10],
    [2305 models 1 and 2], [8],
    [2314, 2319], [20],
    [3330, 3333 (all models)], [19],
    [3340, 3344], [12],
    [3350], [30],
    [3375], [12],
    [3380, 3390], [15],
  )
] <mvs-osio-devices>

=== Related
@mvs-osio-osxread

== osxread, osxwrite <mvs-osio-osxread>
#idx("osxread")#idx("osxwrite")

=== Format
```
#include <mvs/osio.h>

int osxread(DCB *dcb, unsigned blkstrk, unsigned block, void *buf,
            char *sense);
int osxwrite(DCB *dcb, unsigned blkstrk, unsigned block, void *buf,
            char *sense);
```

=== Description
#cmd("osxread()") reads block #var("block") (from 0) of the data set into
#var("buf")\; #cmd("osxwrite()") writes #var("buf") over that block. The
address comes from #cmd("osxcalc()"). The library builds a channel program of
search ID equal followed by read data or write data, issues #cmd("EXCP") and
waits for it. #cmd("dcbblksi") bytes are transferred. When #var("sense") is
not #cmd("NULL"), the first two sense bytes of the request are stored there.

=== Returns
#deflist(width: 0.8in,
  [0], [The block was transferred.],
  [-1], [#cmd("osxcalc()") found no such block; #var("sense[1]") is X'08'
    (no record found).],
  [other], [The ECB completion code, for example X'41' (65) for a permanent
    I/O error. A X'41' from #cmd("osxread()") with no sense data is reported
    with #var("sense[1]") X'08'.],
)

=== Notes
#cmd("osxwrite()") uses write data: it replaces a block that is already on the
track, with one of the same length. It cannot add a block.

=== Example
#code(read("../ex/mvs-osio/excpread.c"))

=== Related
@mvs-osio-osxopen, @mvs-osio-osxcalc

== osxclose <mvs-osio-osxclose>
#idx("osxclose")

=== Format
```
#include <mvs/osio.h>

void osxclose(DCB *dcb, int freedcb);
```

=== Description
#cmd("osxclose()") closes an EXCP DCB, frees its exit list, and frees the DCB
when #var("freedcb") is non-zero. A #cmd("NULL") #var("dcb") is ignored.

=== Related
@mvs-osio-osxopen

== osdopen <mvs-osio-osdopen>
#idx("osdopen")#idx("OPEN", "BDAM")

=== Format
```
#include <mvs/osio.h>

int osdopen(DCB *dcb, int typej);
```

=== Description
#cmd("osdopen()") opens a DCB made by #cmd("osddcb()") for #cmd("UPDAT"), under
an ESTAE, and copies the block size and record length from the JFCB into the
DCB, as #cmd("osxopen()") does.

=== Returns
0 when the data set is open; 8 when it is not; otherwise the return code of
OPEN.

=== Notes
#var("dcb") must not be #cmd("NULL").

=== Related
@mvs-osio-osdread, @mvs-osio-osdclose

== osdread, osdwrite <mvs-osio-osdread>
#idx("osdread")#idx("osdwrite")

=== Format
```
#include <mvs/osio.h>

int osdread(DECB *decb, DCB *dcb, void *buf, int length, unsigned block);
int osdwrite(DECB *decb, DCB *dcb, void *buf, int length, unsigned block);
```

=== Description
#cmd("osdread()") issues a BDAM #cmd("READ") with type #cmd("DI") for
relative block #var("block"), counted from 0; #cmd("osdwrite()") issues the
matching #cmd("WRITE"). Both return without waiting: complete the request
with #cmd("oscheck()"). A #var("length") greater than zero is first stored in
#cmd("dcbblksi")\; zero or less transfers #cmd("dcbblksi") bytes.

=== Returns
The return code of the READ or WRITE macro, normally 0.

=== Notes
Only the low three bytes of #var("block") are passed, so the highest block
number is 16 777 215.

=== Related
@mvs-osio-oscheck, @mvs-osio-osdopen

== osdclose <mvs-osio-osdclose>
#idx("osdclose")

=== Format
```
#include <mvs/osio.h>

void osdclose(DCB *dcb, int freedcb);
```

=== Description
#cmd("osdclose()") closes a BDAM DCB and frees it when #var("freedcb") is
non-zero. A #cmd("NULL") #var("dcb") is ignored.

=== Related
@mvs-osio-osdopen
