#import "../bookmaster/bookmaster.typ": *

= Dynamic Allocation and IDCAMS <mvs-dynalloc>

// Narrow cells: ragged right, no hyphenation of the names.
#show table: set par(justify: false)
#show table: set text(hyphenate: false)

#idx("dynamic allocation")
#idx("SVC 99")
#idx("DYNALLOC")
A program that is to read or write a data set on MVS needs a DD statement
for it. The functions in this chapter create and remove DD statements while
the program runs, through the MVS dynamic allocation service (the DYNALLOC
macro, SVC 99), and run IDCAMS, the access method services utility, to
catalog, delete or rename data sets. Two headers provide them:

#deflist(width: 1.35in,
  [#cmd("<mvs/dynalloc.h>")], [dynamic allocation: a one-call interface
    that takes JCL-like keywords (#cmd("__dsalc()")), the routines that
    build SVC 99 text units one by one, and the SVC itself
    (#cmd("__svc99()")).],
  [#cmd("<mvs/idcams.h>")], [IDCAMS commands, with or without access to
    the messages that IDCAMS prints.],
)

=== The SVC 99 Request

#idx("SVC 99", "request block")
#idx("text unit")
An SVC 99 request is a request block (#cmd("RB99")) that names the
function -- allocate, unallocate, and so on -- and points to a list of
text units. Each text unit (#cmd("TXT99")) carries one keyword of the
request, identified by a key such as #cmd("DALDSNAM") for the data set
name, and corresponds to one parameter of a DD statement. The last
pointer in the list has its high-order bit set. MVS fills in the error and
information codes of the request block when the request fails, and the
value of a text unit that asks for something to be returned, such as the
name of a DD that MVS generated.

A program can use the service at three levels:

+ #cmd("__dsalc()") and #cmd("__dsalcf()") take a string of keywords such
  as #cmd("DSN=A.B;DISP=SHR"), build the text units and the request block,
  and issue the request. #cmd("__dsfree()") removes the DD again. This is
  the interface for ordinary use.
+ The #cmd("__tx...()") routines each add one text unit to a list. A
  program that needs a keyword #cmd("__dsalc()") does not take, or the
  error code of a failed request, builds the list with them and issues
  #cmd("__svc99()") itself (see @mvs-dynalloc-svc99-ex).
+ #cmd("__svc99()") issues SVC 99 for a request block that the program has
  built entirely by itself.

#cmd("__dynal()") is a fourth, older interface: it allocates one existing
data set with #cmd("DISP=SHR").

#idx("authorization", "dynamic allocation")
None of these functions needs APF authorization, and none of them changes
the state or the key of the caller: SVC 99 is issued in the state the
program runs in, and MVS applies its own rules to the request. The library
writes no message to the operator for a failed request\; the outcome is
the return code. (A shortage of storage is the exception: see below.)

=== Restrictions on MVS 3.8j

#idx("SVC 99", "MVS 3.8j restrictions")
- The header #cmd("<ibm/mvs/iefzb4d2.h>") also defines keys of later MVS
  levels, such as #cmd("DALLIKE"), #cmd("DALSTCL") and the other keys from
  #cmd("X'8001'") upward. MVS 3.8j has no such parameters. There is in
  particular no equivalent of #cmd("LIKE=")\; #cmd("__txdcbd()") copies
  the DCB attributes of a model data set, never its space.
- A request that names a volume which is not mounted makes MVS ask the
  operator to mount it (message IEF238D), and the task waits for the
  reply. #cmd("__dsalc()") prevents this unless asked for it\; see
  @mvs-dynalloc-dsalc.
- Storage for text units comes from #cmd("calloc()"). When storage runs
  out, #cmd("malloc()") returns #cmd("NULL") with #cmd("errno")
  #cmd("ENOMEM") and writes #cmd("Out of memory") to the operator, and the
  function returns as described below for that case.

=== Request Block and Text Unit

@mvs-dynalloc-rb99-tab lists the fields of the request block, which a
program fills in when it calls #cmd("__svc99()") directly. The structure is
#cmd("struct rb99") in #cmd("<ibm/mvs/iefzb4d0.h>"), 20 bytes long\;
#cmd("<mvs/dynalloc.h>") includes it. See also @apx-cblocks.

#tab(caption: [Fields of the SVC 99 request block (RB99)])[
  #table(columns: (0.5in, 0.75in, 1fr),
    [Offset], [Field], [Contents],
    [0], [#cmd("len")], [Length of the request block: 20
      (#cmd("sizeof(RB99)")).],
    [1], [#cmd("request")], [The function: #cmd("S99VRBAL") allocation,
      #cmd("S99VRBUN") unallocation, #cmd("S99VRBCC") concatenation,
      #cmd("S99VRBDC") deconcatenation, #cmd("S99VRBRI") remove in-use,
      #cmd("S99VRBDN") DD name allocation, #cmd("S99VRBIN") information
      retrieval.],
    [2], [#cmd("flag1")], [Option flags, for example #cmd("S99NOCNV")
      (do not use an existing allocation to satisfy the request) and
      #cmd("S99NOMNT") (do not mount volumes or consider offline units).
      The header defines the full set.],
    [3], [#cmd("flag2")], [Further option flags.],
    [4], [#cmd("error")], [Error reason code, set by MVS when the request
      fails.],
    [6], [#cmd("info")], [Information reason code, set by MVS.],
    [8], [#cmd("txtptr")], [Address of the list of text unit pointers.
      The last pointer has its high-order bit set.],
    [12], [#cmd("rbx99")], [Reserved\; leave it zero. The structure
      #cmd("struct rbx99") of the header maps the request block extension
      of later MVS levels and is not used by the library.],
    [16], [#cmd("flag3")], [Reserved flags\; leave them zero.],
  )
] <mvs-dynalloc-rb99-tab>

#tab(caption: [Fields of an SVC 99 text unit (TXT99)])[
  #table(columns: (0.5in, 0.75in, 1fr),
    [Offset], [Field], [Contents],
    [0], [#cmd("dal")], [The key: #cmd("DALDDNAM"), #cmd("DALDSNAM") and
      so on for allocation, #cmd("DUNDDNAM") and so on for unallocation.
      The header defines one constant for each key.],
    [2], [#cmd("count")], [Number of values that follow\; 0 for a key that
      takes none, such as #cmd("DALDUMMY").],
    [4], [#cmd("size")], [Length of the first value.],
    [6], [#cmd("text")], [The first value. When #cmd("count") is greater
      than 1, each further value follows as a halfword length and the
      value itself.],
  )
] <mvs-dynalloc-txt99-tab>

A text unit has a variable length, so it is allocated, not declared:
#cmd("__nwtx99()") and its two companions build one in storage obtained
with #cmd("calloc()").

== \_\_dsalc, \_\_dsalcf <mvs-dynalloc-dsalc>
#idx("__dsalc")
#idx("__dsalcf")
#idx("dynamic allocation", "with keywords")

=== Format
```
#include <mvs/dynalloc.h>

int __dsalc(char *ddname, const char *opts);
int __dsalcf(char *ddname, const char *opts, ...);
```

=== Description
#cmd("__dsalc()") allocates a data set dynamically. #var("opts") is a list
of keywords separated by semicolons or commas, written much as on a DD
statement. The function builds the text units, issues SVC 99 with the
allocation function, and, if #var("ddname") is not #cmd("NULL"), stores the
name of the DD in it as a string of up to 8 characters. #var("ddname")
must therefore have room for 9 characters.

#cmd("__dsalcf()") first formats #var("opts") and the further arguments as
#cmd("printf()") does, and passes the result to #cmd("__dsalc()").

@mvs-dynalloc-dsalc-tab lists the keywords. They may be written in
uppercase or lowercase\; the whole string, data set name included, is
converted to uppercase. A keyword that is not in the table is ignored.

#tab(caption: [Keywords of \_\_dsalc()])[
  #table(columns: (1.7in, 1fr),
    [Keyword], [Meaning],
    [#cmd("DD=")#var("name"), #cmd("DDNAME=")#var("name")], [The DD
      name. Without it MVS generates a name, which is returned in
      #var("ddname").],
    [#cmd("DSN=")#var("dsn"), #cmd("DSNAME=")#var("dsn")], [The data set
      name. A member cannot be given: #cmd("DSN=A.B(MEM)") reaches the
      allocation as #cmd("A.B=MEM"), a defect (libc370 issue 408).],
    [#cmd("DISP=")#var("status"), #cmd("DISP=(")#var("status")#cmd(",")#var("normal")#cmd(",")#var("abnormal")#cmd(")")],
      [#var("status") is #cmd("NEW"), #cmd("OLD"), #cmd("MOD") or
      #cmd("SHR"). #var("normal") and #var("abnormal") are #cmd("KEEP"),
      #cmd("DELETE") (or #cmd("DEL")), #cmd("CATLG") (or #cmd("CAT")) or
      #cmd("UNCATLG"). Any other value, #cmd("PASS") among them, makes the
      call fail without allocating anything.],
    [#cmd("DCBDSN=")#var("dsn")], [The model data set of
      #cmd("DCB=(")#var("dsn")#cmd(")") in JCL: its DCB attributes are
      copied. Explicit #cmd("DSORG="), #cmd("RECFM="), #cmd("LRECL=") and
      #cmd("BLKSIZE=") override them.],
    [#cmd("DSORG=")#var("org")], [#cmd("PS"), #cmd("PO"), #cmd("DA") and
      the others of @mvs-dynalloc-tx-dcb-tab.],
    [#cmd("RECFM=")#var("fmt")], [For example #cmd("FB") or #cmd("VB").],
    [#cmd("LRECL=")#var("n")], [Logical record length\;
      #var("n")#cmd("K") and #cmd("X") are accepted as in JCL.],
    [#cmd("BLKSIZE=")#var("n")], [Block size, at most 32760.],
    [#cmd("SPACE=TRK(")#var("p")#cmd(",")#var("s")#cmd(",")#var("d")#cmd(")"),
     #cmd("SPACE=CYL(")#var("p")#cmd(",")#var("s")#cmd(",")#var("d")#cmd(")"),
     #cmd("SPACE=")#var("blklen")#cmd("(")#var("p")#cmd(",")#var("s")#cmd(",")#var("d")#cmd(")")],
      [Space in tracks, cylinders or blocks of #var("blklen") bytes:
      primary quantity #var("p"), and optionally secondary quantity
      #var("s") and directory blocks #var("d").],
    [#cmd("UNIT=")#var("unit")], [Unit name, for example #cmd("SYSDA").],
    [#cmd("VOLSER=")#var("vol"), #cmd("VOLSER=(")#var("v1")#cmd(",")#var("v2")#cmd(",...)")],
      [Volume serial numbers.],
    [#cmd("MOUNT")], [Allow MVS to ask the operator to mount a volume\;
      see Notes.],
  )
] <mvs-dynalloc-dsalc-tab>

The text units are issued in this order, whatever the order of the
keywords: DD name, data set name, disposition, DCB model, DSORG, RECFM,
LRECL, BLKSIZE, space, unit, volumes.

=== Returns
0 when the data set was allocated. 1 when #var("opts") holds a value the
function cannot turn into a text unit (a bad disposition, a bad space
specification) or storage for the request ran out. Otherwise the return
code of SVC 99. #cmd("__dsalcf()") returns 4 when #var("opts") is
#cmd("NULL").

=== Notes
- *Mount requests.* When #var("opts") names #cmd("UNIT=") or
  #cmd("VOLSER="), the request is made with the flag #cmd("S99NOMNT"): a
  volume that is not mounted makes the call fail at once instead of asking
  the operator. The keyword #cmd("MOUNT") turns the flag off, for a tape
  or a volume the operator is expected to mount\; the task then waits for
  the operator. Without #cmd("UNIT=") and #cmd("VOLSER=") the flag is not
  set.
- Every request carries the flag #cmd("S99NOCNV"), so MVS never satisfies
  it with an existing allocation.
- The JCL form #cmd("SPACE=(TRK,(1,1))"), with nested parentheses, is not
  understood and makes the call return 1. Write #cmd("SPACE=TRK(1,1)").
- A disposition of #cmd("UNCATLG") or #cmd("UNCAT") is taken as
  #cmd("CATLG"), because the value is searched for #cmd("CAT") before
  #cmd("UNCAT"). To uncatalog, build the request with #cmd("__txucat()") or
  #cmd("__txauca()") and issue it with #cmd("__svc99()").
- The error and information codes of a failed request are not returned.
  To see them, build the request with the #cmd("__tx...()") routines and
  issue it with #cmd("__svc99()").
- #cmd("__dsalc()") uses #cmd("strtok()") internally and restores the
  position of the caller's #cmd("strtok()") sequence before it returns.
  Do not pass #cmd("NULL") for #var("opts"): the function then continues
  the caller's #cmd("strtok()") sequence.
- #cmd("__dsalcf()") formats into a buffer of 512 bytes\; a longer
  keyword string is truncated.
- The data set is released, and its normal disposition takes effect, when
  the DD is freed with #cmd("__dsfree()") or at the end of the job step.

=== Example
#fig(caption: [Creating a data set with \_\_dsalcf()])[
  #code(read("../ex/mvs-dynalloc/alloc.c"), numbers: true)
] <mvs-dynalloc-alloc-ex>

=== Related
@mvs-dynalloc-dsfree, @mvs-dynalloc-tx, @mvs-dynalloc-svc99

== \_\_dsfree <mvs-dynalloc-dsfree>
#idx("__dsfree")
#idx("dynamic allocation", "freeing a DD")

=== Format
```
#include <mvs/dynalloc.h>

int __dsfree(const char *ddname);
```

=== Description
#cmd("__dsfree()") removes the DD named #var("ddname") with an SVC 99
unallocation request. The data set receives the normal disposition it was
allocated with, as at the end of a job step.

=== Returns
0 when the DD was freed\; 1 when #var("ddname") is #cmd("NULL") or storage
ran out\; otherwise the return code of SVC 99.

=== Notes
#var("ddname") is used as given: it is not converted to uppercase and its
length is not checked. Pass the name exactly as #cmd("__dsalc()") returned
it.

The request carries the unallocation option of key #cmd("DUNUNALC"), which
also frees a DD that is permanently allocated.

=== Related
@mvs-dynalloc-dsalc

== \_\_dynal <mvs-dynalloc-dynal>
#idx("__dynal")

=== Format
```
#include <mvs/dynalloc.h>

int __dynal(size_t ddn_len, char *ddn, size_t dsn_len, char *dsn);
```

=== Description
#cmd("__dynal()") allocates the existing data set #var("dsn") with
#cmd("DISP=SHR"). The DD stays allocated after the data set is closed:
#cmd("FREE=CLOSE") is meant, but the request does not pass it (a defect,
libc370 issue 408). Free the DD when it is no longer needed. #var("dsn") is #var("dsn_len") characters long, 1 to
44, and need not be terminated by a null character.

#var("ddn") names the DD, #var("ddn_len") characters, 1 to 8. If the name
is all blanks or all zeros and #var("ddn_len") is 8, MVS generates a DD
name, which is stored into the 8 bytes at #var("ddn"), padded with blanks
and not terminated. If a name is given, a DD of that name is freed first,
whether it exists or not.

=== Returns
The high-order halfword holds the return code of SVC 99, and the
low-order halfword the contents of register 0 after the SVC. Two values
report an error of #cmd("__dynal()") itself: #cmd("0x14000001") for a
length out of range, and #cmd("0x048040")#var("nn") when its work area
could not be obtained, #var("nn") being the GETMAIN return code.

=== Notes
The call is an ordinary cc370 call, with register 1 pointing to a list of
the four argument values, which is the parameter list the assembler routine
expects. A member name in #var("dsn") is not recognized.

A call with a DD name and a #var("dsn_len") of 0 frees that DD and then
returns #cmd("0x14000001").

=== Related
@mvs-dynalloc-dsalc

== \_\_svc99 <mvs-dynalloc-svc99>
#idx("__svc99")
#idx("SVC 99", "issuing")

=== Format
```
#include <mvs/dynalloc.h>

int __svc99(void *rb);
```

=== Description
#cmd("__svc99()") issues SVC 99 for the request block at #var("rb"), an
#cmd("RB99") that the caller has built (@mvs-dynalloc-rb99-tab). The
pointer that SVC 99 needs, with its high-order bit set, is built by the
function\; the caller passes the request block itself.

=== Returns
The return code of SVC 99: 0 when the request succeeded. When it is not
0, the fields #cmd("error") and #cmd("info") of the request block hold the
reason. -1 when the function could not obtain its work area\; SVC 99 has
then not been issued and the request block is unchanged.

=== Notes
The text unit pointers are not checked. The last one must have its
high-order bit set, and every one must point to a valid text unit.

=== Example
#fig(caption: [Issuing SVC 99 with text units built by the library])[
  #code(read("../ex/mvs-dynalloc/svc99.c"), numbers: true)
] <mvs-dynalloc-svc99-ex>

The list of text unit pointers is a dynamic array of the library
(@ext)\; #cmd("arraycount()") returns the number of entries.

=== Related
@mvs-dynalloc-tx, @mvs-dynalloc-nwtx99

== \_\_nwtx99, \_\_nwtx9a, \_\_nwtx9s <mvs-dynalloc-nwtx99>
#idx("__nwtx99")
#idx("__nwtx9a")
#idx("__nwtx9s")
#idx("NewTXT99")
#idx("text unit", "building")

=== Format
```
#include <mvs/dynalloc.h>

TXT99 *__nwtx99(int dal, int count, int size, const char *text);
TXT99 *__nwtx9a(int dal, int count, char **array);
TXT99 *__nwtx9s(int dal, int count, ...);

#define NewTXT99(dal,count,size,text)   __nwtx99((dal),(count),(size),(text))
#define NewTXT99a(dal,count,array)      __nwtx9a((dal),(count),(array))
#define NewTXT99s(dal,count, ...)       __nwtx9s((dal),(count),## __VA_ARGS__)
```

=== Description
Each function allocates one text unit with key #var("dal") and
#var("count") values, and returns its address.

#cmd("__nwtx99()") gives every value the same length #var("size"), and
copies #cmd("count * size") bytes from #var("text") into the text unit. If
#var("text") is #cmd("NULL") the values are left zero, for a key whose
value MVS returns. A key without a value is built with #var("count") and
#var("size") 0.

#cmd("__nwtx9a()") takes the values from an array of #var("count")
strings, and #cmd("__nwtx9s()") from #var("count") string arguments. Each
value has the length of its string\; the terminating null characters are
not copied. A #cmd("NULL") element of the array is stored as a value of
length 0\; a #cmd("NULL") argument to #cmd("__nwtx9s()") ends the list.

The macros #cmd("NewTXT99"), #cmd("NewTXT99a") and #cmd("NewTXT99s") are
other names for the three functions.

=== Returns
The address of the text unit, or #cmd("NULL") when no storage was
available.

=== Notes
The text unit is obtained with #cmd("calloc()") and freed with
#cmd("__frtx99()"), or with #cmd("__frtx9a()") together with the list that
holds it.

When #cmd("__nwtx9s()") meets a #cmd("NULL") argument before #var("count")
strings, the field #cmd("count") of the text unit still says
#var("count"), and the missing values are empty.

=== Related
@mvs-dynalloc-frtx99, @mvs-dynalloc-tx

== \_\_frtx99, \_\_frtx9a <mvs-dynalloc-frtx99>
#idx("__frtx99")
#idx("__frtx9a")
#idx("FreeTXT99")
#idx("FreeTXT99Array")

=== Format
```
#include <mvs/dynalloc.h>

void __frtx99(TXT99 **txt99);
void __frtx9a(TXT99 ***txt99array);

#define FreeTXT99(txt99)                __frtx99(txt99)
#define FreeTXT99Array(txt99array)      __frtx9a(txt99array)
```

=== Description
#cmd("__frtx99()") frees the text unit that #cmd("*txt99") points to and
sets #cmd("*txt99") to #cmd("NULL"). #cmd("__frtx9a()") frees every text
unit in the list #cmd("*txt99array"), then the list, and sets
#cmd("*txt99array") to #cmd("NULL").

Both do nothing when the argument or the pointer it addresses is
#cmd("NULL"). The macros #cmd("FreeTXT99") and #cmd("FreeTXT99Array") are
other names for them.

=== Notes
A list whose last pointer carries the high-order bit for SVC 99 may be
freed as it is\; the library frees its own lists that way after the
request.

=== Related
@mvs-dynalloc-nwtx99, @mvs-dynalloc-tx

== \_\_txddn, \_\_txdsn, ... -- Adding Text Units <mvs-dynalloc-tx>
#idx("__txddn")
#idx("__txdsn")
#idx("text unit", "adding to a list")

=== Format
```
#include <mvs/dynalloc.h>

int __txddn(TXT99 ***txt99, const char *ddname);
int __txdsn(TXT99 ***txt99, const char *dataset);
int __txnew(TXT99 ***txt99, const char *unused);
   ...
```

Every routine in @mvs-dynalloc-tx-alloc-tab, @mvs-dynalloc-tx-other-tab,
@mvs-dynalloc-tx-sysout-tab and @mvs-dynalloc-tx-dcb-tab has this form: the address of a list of text
unit pointers, and one string.

=== Description
Each routine builds the text unit for one parameter and adds it at the
end of the list #cmd("*txt99"). The list is a dynamic array of the library
(@ext): start with a #cmd("TXT99 **") that is #cmd("NULL"), pass its
address to every call, and the first call creates the array.

The string argument carries the value of the parameter, written as in JCL:
a number as decimal digits, a name as characters. A routine whose
parameter has no value ignores its argument\; it is named #var("unused")
in the header, and #cmd("NULL") may be passed. The value is used as given:
no routine converts it to uppercase, except that #cmd("__txrecf()")
accepts lowercase letters.

To issue the request, set the high-order bit of the last pointer in the
list, build an #cmd("RB99") that points to the list, and call
#cmd("__svc99()"), as @mvs-dynalloc-svc99-ex shows. Free the list with
#cmd("__frtx9a()").

#tab(caption: [Text units for the data set, its disposition and its volumes])[
  #table(columns: (0.85in, 1.3in, 0.85in, 1fr),
    [Routine], [DD parameter], [Key], [Argument],
    [#cmd("__txddn")], [DD name], [#cmd("DALDDNAM")], [The DD name, as
      given.],
    [#cmd("__txrddn")], [return the DD name], [#cmd("DALRTDDN")], [Unused.
      MVS stores the generated name in the 8-byte value.],
    [#cmd("__txdsn")], [#cmd("DSN=")], [#cmd("DALDSNAM"),
      #cmd("DALMEMBR")], [#cmd("dsn") or #cmd("dsn(member)"), shorter than
      80 characters. A member adds a #cmd("DALMEMBR") unit ahead of the
      #cmd("DALDSNAM") unit.],
    [#cmd("__txdmy")], [#cmd("DUMMY")], [#cmd("DALDUMMY")], [Unused.],
    [#cmd("__txnew"), #cmd("__txold"), #cmd("__txmod"), #cmd("__txshr")],
      [#cmd("DISP=NEW"), #cmd("OLD"), #cmd("MOD"), #cmd("SHR")],
      [#cmd("DALSTATS")], [Unused.],
    [#cmd("__txkeep"), #cmd("__txdel"), #cmd("__txcat"), #cmd("__txucat")],
      [#cmd("DISP=(,KEEP)"), #cmd("DELETE"), #cmd("CATLG"),
      #cmd("UNCATLG")], [#cmd("DALNDISP")], [Unused.],
    [#cmd("__txakee"), #cmd("__txadel"), #cmd("__txacat"),
      #cmd("__txauca")], [#cmd("DISP=(,,KEEP)"), #cmd("DELETE"),
      #cmd("CATLG"), #cmd("UNCATLG")], [#cmd("DALCDISP")], [Unused.],
    [#cmd("__txdcbd")], [#cmd("DCB=(")#var("dsn")#cmd(")")],
      [#cmd("DALDCBDS")], [The model data set name, 1 to 44 characters.
      DCB attributes only, never space.],
    [#cmd("__txunit")], [#cmd("UNIT=")], [#cmd("DALUNIT")], [The unit
      name.],
    [#cmd("__txunct")], [#cmd("UNIT=(,")#var("n")#cmd(")")],
      [#cmd("DALUNCNT")], [Unit count, greater than 0.],
    [#cmd("__txpara")], [#cmd("UNIT=(,P)")], [#cmd("DALPARAL")],
      [Unused.],
    [#cmd("__txvols")], [#cmd("VOL=SER=")], [#cmd("DALVLSER")], [One or
      more volume serials, separated by commas or blanks.],
    [#cmd("__txvlct")], [#cmd("VOL=(,,,")#var("n")#cmd(")")],
      [#cmd("DALVLCNT")], [Volume count, 1 to 255.],
    [#cmd("__txvseq")], [#cmd("VOL=(,,")#var("n")#cmd(")")],
      [#cmd("DALVLSEQ")], [Volume sequence number, 1 to 255.],
    [#cmd("__txpriv")], [#cmd("VOL=PRIVATE")], [#cmd("DALPRIVT")],
      [Unused.],
  )
] <mvs-dynalloc-tx-alloc-tab>

#tab(caption: [Text units for space, labels and other options])[
  #table(columns: (0.85in, 1.3in, 0.85in, 1fr),
    [Routine], [DD parameter], [Key], [Argument],
    [#cmd("__txtrk"), #cmd("__txcyl")], [#cmd("SPACE=(TRK,...)"),
      #cmd("(CYL,...)")], [#cmd("DALTRK"), #cmd("DALCYL")], [Unused.],
    [#cmd("__txblk")], [#cmd("SPACE=(")#var("blklen")#cmd(",...)")],
      [#cmd("DALBLKLN")], [Block length, greater than 0.],
    [#cmd("__txspac")], [#cmd("SPACE=(,(")#var("p")#cmd(",")#var("s")#cmd(",")#var("d")#cmd("))")],
      [#cmd("DALPRIME"), #cmd("DALSECND"), #cmd("DALDIR")],
      [#cmd("\"p[,s[,d]]\""). Primary and secondary are always added, a
      missing secondary as 0\; directory blocks only when not 0.],
    [#cmd("__txdir")], [directory blocks], [#cmd("DALDIR")], [Number of
      blocks, greater than 0.],
    [#cmd("__txrlse")], [#cmd("SPACE=(,,RLSE)")], [#cmd("DALRLSE")],
      [Unused.],
    [#cmd("__txrnd")], [#cmd("SPACE=(,,,,ROUND)")], [#cmd("DALROUND")],
      [Unused.],
    [#cmd("__txlabe")], [#cmd("LABEL=(,")#var("type")#cmd(")")],
      [#cmd("DALLABEL")], [#cmd("NL"), #cmd("SL"), #cmd("NSL"),
      #cmd("SUL"), #cmd("BLP"), #cmd("LTM"), #cmd("AL") or #cmd("AUL")\;
      at least two characters, matched as a prefix.],
    [#cmd("__txseq")], [#cmd("LABEL=")#var("n")], [#cmd("DALDSSEQ")],
      [Data set sequence number, 1 to 9999.],
    [#cmd("__txinpu"), #cmd("__txoutp")], [#cmd("LABEL=(,,,IN)"),
      #cmd("(,,,OUT)")], [#cmd("DALINOUT")], [Unused.],
    [#cmd("__txexpd")], [#cmd("EXPDT=")], [#cmd("DALEXPDT")],
      [#cmd("yyddd"), exactly 5 characters.],
    [#cmd("__txretp")], [#cmd("RETPD=")], [#cmd("DALRETPD")], [Days, 1 to
      93000.],
    [#cmd("__txprot")], [#cmd("PROTECT=YES")], [#cmd("DALPROT")],
      [Unused.],
    [#cmd("__txterm")], [#cmd("TERM=TS")], [#cmd("DALTERM")], [Unused.],
    [#cmd("__txperm")], [permanently allocated], [#cmd("DALPERMA")],
      [Unused.],
    [#cmd("__txclos")], [#cmd("FREE=CLOSE")], [#cmd("DALCLOSE")],
      [Unused.],
    [#cmd("__txunal")], [unallocation option], [#cmd("DUNUNALC")],
      [Unused. For an unallocation request (#cmd("S99VRBUN")) only.],
  )
] <mvs-dynalloc-tx-other-tab>

#tab(caption: [Text units for SYSOUT and unit record devices])[
  #table(columns: (0.85in, 1.3in, 0.85in, 1fr),
    [Routine], [DD parameter], [Key], [Argument],
    [#cmd("__txsyso")], [#cmd("SYSOUT=")], [#cmd("DALSYSOU")], [The
      output class\; only its first character is used. #cmd("NULL") builds
      the text unit without a class.],
    [#cmd("__txpgm")], [#cmd("SYSOUT=(,")#var("pgm")#cmd(")")],
      [#cmd("DALSPGNM")], [Writer name, 1 to 8 characters.],
    [#cmd("__txform")], [#cmd("SYSOUT=(,,")#var("form")#cmd(")")],
      [#cmd("DALSFMNO")], [Form number, 1 to 4 characters.],
    [#cmd("__txdest")], [#cmd("DEST=")], [#cmd("DALSUSER"),
      #cmd("DALUSRID")], [A destination, or #cmd("node.user"), shorter
      than 40 characters, which adds the user as #cmd("DALUSRID") and the
      node as #cmd("DALSUSER").],
    [#cmd("__txhold")], [#cmd("HOLD=YES")], [#cmd("DALSHOLD")], [Unused.],
    [#cmd("__txcopy")], [#cmd("COPIES=")], [#cmd("DALCOPYS")], [1 to
      255.],
    [#cmd("__txfcb")], [#cmd("FCB=")], [#cmd("DALFCBIM"),
      #cmd("DALFCBAV")], [An FCB image name, or #cmd("ALIGN") or
      #cmd("VERIFY"), which build a #cmd("DALFCBAV") unit instead.],
    [#cmd("__txucs")], [#cmd("UCS=")], [#cmd("DALUCS"), #cmd("DALUFOLD"),
      #cmd("DALUVRFY")], [A character set name, or #cmd("FOLD") or
      #cmd("VERIFY"), which build the unit of that option instead.],
  )
] <mvs-dynalloc-tx-sysout-tab>

#tab(caption: [Text units for DCB attributes])[
  #table(columns: (0.85in, 1.3in, 0.85in, 1fr),
    [Routine], [DD parameter], [Key], [Argument],
    [#cmd("__txorg")], [#cmd("DSORG=")], [#cmd("DALDSORG")],
      [#cmd("PS"), #cmd("PSU"), #cmd("PO"), #cmd("POU"), #cmd("DA"),
      #cmd("DAU"), #cmd("CQ"), #cmd("CX"), #cmd("GS"), #cmd("MQ"),
      #cmd("TQ"), #cmd("TX"), #cmd("TCAM"), #cmd("3705") or
      #cmd("VSAM"), in uppercase.],
    [#cmd("__txrecf")], [#cmd("RECFM=")], [#cmd("DALRECFM")], [Any
      combination of #cmd("F"), #cmd("V"), #cmd("U"), #cmd("B"),
      #cmd("S"), #cmd("A"), #cmd("M"), #cmd("T"), #cmd("D"), #cmd("G")
      and #cmd("R")\; other letters are ignored.],
    [#cmd("__txlrec")], [#cmd("LRECL=")], [#cmd("DALLRECL")], [A number,
      #var("n")#cmd("K"), or #cmd("X"). Values above 32760 are reduced to
      32760.],
    [#cmd("__txbksz")], [#cmd("BLKSIZE=")], [#cmd("DALBLKSZ")], [Greater
      than 0\; values above 32760 are reduced to 32760.],
    [#cmd("__txkeyl")], [#cmd("KEYLEN=")], [#cmd("DALKYLEN")], [0 to
      255.],
    [#cmd("__txbfal")], [#cmd("BFALN=")], [#cmd("DALBFALN")],
      [#cmd("FULL") or #cmd("DOUBLE"), or an abbreviation such as
      #cmd("F") or #cmd("D").],
    [#cmd("__txbfte")], [#cmd("BFTEK=")], [#cmd("DALBFTEK")],
      [#cmd("DYNAMIC"), #cmd("EXCHANGE"), #cmd("RECORD"), #cmd("SIMPLE")
      or #cmd("AREA"), or an abbreviation.],
    [#cmd("__txbufl")], [#cmd("BUFL=")], [#cmd("DALBUFL")], [Greater than
      0\; values above 32760 are reduced to 32760.],
    [#cmd("__txbufn")], [#cmd("BUFNO=")], [#cmd("DALBUFNO")], [Greater
      than 0\; values above 255 are reduced to 255.],
    [#cmd("__txbufo")], [#cmd("BUFOFF=")], [#cmd("DALBUFOF")], [0 to 99,
      or #cmd("L").],
    [#cmd("__txden")], [#cmd("DEN=")], [#cmd("DALDEN")], [Tape density in
      bits per inch: #cmd("200"), #cmd("556"), #cmd("800"), #cmd("1600")
      or #cmd("6250").],
    [#cmd("__txtrtc")], [#cmd("TRTCH=")], [#cmd("DALTRTCH")], [#cmd("C"),
      #cmd("COMP"), #cmd("E"), #cmd("ET"), #cmd("NOCOMP") or #cmd("T"),
      matched as a prefix.],
    [#cmd("__txerop")], [#cmd("EROPT=")], [#cmd("DALEROPT")],
      [#cmd("ACCEPT"), #cmd("SKIP") (or #cmd("SKP")), #cmd("ABEND"), or
      #cmd("BSAM") (or #cmd("TEST")).],
    [#cmd("__txlmct")], [#cmd("LIMCT=")], [#cmd("DALLIMCT")], [Greater
      than 0\; values above 32760 are reduced to 32760.],
    [#cmd("__txncp")], [#cmd("NCP=")], [#cmd("DALNCP")], [1 to 255.],
  )
] <mvs-dynalloc-tx-dcb-tab>

=== Returns
0 when the text unit was added to the list. Nonzero when the argument is
#cmd("NULL") where a value is needed, is not one of the accepted values,
is out of range, or when storage ran out\; nothing is added then, except
that #cmd("__txdsn()") may leave the #cmd("DALMEMBR") unit of a member
name in the list. A routine whose argument is unused fails only for lack
of storage. Two exceptions: #cmd("__txkeyl(NULL)") and
#cmd("__txbufo(NULL)") build a value of 0 and return 0, and
#cmd("__txblk()") and #cmd("__txdir()") accept a negative value, since
they test only that the length is not 0.

=== Notes
- The routines check the form of the value, not whether MVS will accept
  the request. #cmd("__txddn()") and #cmd("__txunit()") do not check the
  length of the name\; SVC 99 rejects a name that is too long.
- A number is converted with #cmd("atoi()"): characters after the digits
  are ignored, and a value that is not a number counts as 0.
- #cmd("__txdsn()") recognizes a member name in parentheses\; a missing
  closing parenthesis is accepted.
- #cmd("__txunct()") stores only the low-order byte of the count.

=== Related
@mvs-dynalloc-svc99, @mvs-dynalloc-nwtx99, @mvs-dynalloc-frtx99,
@mvs-dynalloc-dsalc

== idcams, idcams\_sysprint <mvs-dynalloc-idcams>
#idx("idcams")
#idx("idcams_sysprint")
#idx("IDCAMS")
#idx("access method services")

=== Format
```
#include <mvs/idcams.h>

int idcams(const char *fmt, ...);

typedef void (*IDCAMS_SYSPRINT)(void *arg, int msgno,
                                const char *text, int len);
int idcams_sysprint(IDCAMS_SYSPRINT fn, void *arg, const char *fmt, ...);
```

=== Description
#cmd("idcams()") formats its arguments under the control of #var("fmt"),
as #cmd("printf()") does, and runs the result as an IDCAMS command. The
function calls the program IDCAMS with the MVS LINK macro and supplies its
input and output through the I/O exits of IDCAMS, so no SYSIN or SYSPRINT
DD statement is needed. The output of IDCAMS is discarded.

#cmd("idcams_sysprint()") runs the command in the same way and calls
#var("fn") once for every line IDCAMS prints, in the order IDCAMS writes
them:

#deflist(width: 0.8in,
  [#var("arg")], [the argument #var("arg") of #cmd("idcams_sysprint()"),
    passed through unchanged.],
  [#var("msgno")], [the message number of the line: 3012 for a line that
    holds message IDC3012I, 2 for IDC0002I. 0 for a line that is not a
    message, such as a page heading, a blank line, the echo of the command
    or the counts of LISTCAT.],
  [#var("text")], [the line as IDCAMS writes it: a carriage control
    character (#cmd("'1'") new page, #cmd("'0'") skip a line, a blank for
    the next line), then the text. A message starts with #cmd("IDC") at
    #cmd("text[1]").],
  [#var("len")], [the length of the line, carriage control included.],
)

=== Returns
The highest condition code of IDCAMS: 0, 4, 8, 12 or 16. A negative value
when the LINK to IDCAMS failed.

=== Notes
- The condition code alone does not say why a command failed: 8 is
  returned both for an entry that does not exist and for one that cannot
  be processed. The message explains it, and #cmd("idcams_sysprint()")
  delivers it. IDCAMS ends every command with message IDC0001I and the run
  with IDC0002I\; the first other message is usually the one to look at.
  On MVS 3.8j, a DELETE or ALTER of a missing entry gives IDC3012I with
  condition code 8, and ALTER NEWNAME with a qualifier of nine characters
  gives IDC3203I with condition code 12.
- #var("text") is not terminated by a null character and is valid only
  during the call. Copy what is needed.
- #var("fn") runs inside the output exit of IDCAMS, on a stack of 8000
  bytes that the library provides for the exit and everything it calls.
  Keep it short, and do not call #cmd("idcams()") from it.
- The formatted command is limited to 255 characters and is passed to
  IDCAMS as a single input record. Begin it with a blank, as
  the example below does: IDCAMS does not read column 1 of its
  input.
- Both functions are reentrant\; each call has its own exit data, so
  several tasks may run IDCAMS at the same time.

=== Example
#fig(caption: [Running IDCAMS and reading its messages])[
  #code(read("../ex/mvs-dynalloc/idcams.c"), numbers: true)
] <mvs-dynalloc-idcams-ex>

=== Related
@mvs-dynalloc-idcams-asm

== \_\_idcams <mvs-dynalloc-idcams-asm>
#idx("__idcams")

=== Format
```
#include <mvs/idcams.h>

int __idcams(size_t len, char *data);
```

=== Description
#cmd("__idcams()") runs the IDCAMS command of #var("len") characters at
#var("data") and returns the condition code of IDCAMS. The output of
IDCAMS is discarded.

=== Returns
The highest condition code of IDCAMS.

=== Notes
#cmd("__idcams()") is written in assembler and is not reentrant: it keeps
the request and its save areas in its own module. It cannot run in two
tasks at once, and it must not be used in a program that is loaded into
storage it cannot modify. Use #cmd("idcams()") instead.

The call is an ordinary cc370 call, with register 1 pointing to a list of
the two argument values, the length and the address of the text, which
is the parameter list the assembler routine expects.

=== Related
@mvs-dynalloc-idcams
