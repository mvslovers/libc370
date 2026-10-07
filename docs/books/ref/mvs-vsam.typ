#import "../bookmaster/bookmaster.typ": *

= VSAM <mvs-vsam>

#idx("mvs/vsam.h")#idx("VSAM")
The header #cmd("<mvs/vsam.h>") reads and writes VSAM clusters -- key
sequenced (KSDS), entry sequenced (ESDS) and relative record (RRDS) -- through
a handle of type #cmd("VSFILE"). Behind the handle are an ACB and an RPL; the
functions issue the VSAM macros #cmd("OPEN"), #cmd("GET"), #cmd("PUT"),
#cmd("POINT"), #cmd("ERASE"), #cmd("CLOSE"), #cmd("MODCB") and
#cmd("SHOWCB"). One more function, #cmd("__vsshwc()"), issues #cmd("SHOWCAT")
to read the catalog description of a VSAM object.

The cluster is named by a DD statement. All requests of one handle use its
single RPL, so one handle has one position in the cluster at a time.

*Authorization.* The functions issue no #cmd("TESTAUTH") and no
#cmd("MODESET")\; they run in problem state, and no APF authorization is
needed. VSAM password protection, where a cluster has it, is not handled: the
ACB carries no password.

*Two forms of every function.* Each operation exists as #cmd("vs")#var("name")
and #cmd("__vs")#var("name"). The #cmd("vs") form takes a lock on the handle
(the #cmd("lock()") function with the handle address as resource, see
@mvs-sync) around the call, so that several tasks can share a handle; the
#cmd("__vs") form takes no lock. #cmd("vsopen()") takes no lock either -- there
is no handle yet. #cmd("__vsmdfy()") and #cmd("__vsshwc()") exist only in the
#cmd("__") form.

*Errors.* VSAM reports a failed request through the exit routines that
#cmd("__vsopen()") installs (LERAD, SYNAD and EODAD). LERAD and SYNAD
record the return, component and reason codes of the RPL feedback word in
the handle and set the error flag\; EODAD only sets the end-of-file flag.
The request returns 8 (logical error), 12 (physical error) or 4 (end of
data). *The flags stay set* until
#cmd("vsclear()") resets them, as the end-of-file and error indicators of a
stream do, and #cmd("vseof()") and #cmd("vserror()") test them. The value
#cmd("vsread()") returns reflects only its own GET. The #cmd("errno") values of
@mvs-vsam-errno are set by #cmd("vsopen()") and #cmd("vsread()").

The library's tests cover #cmd("vsopen()"), #cmd("vsread()"),
#cmd("vseof()"), #cmd("vserror()") and #cmd("vsclear()") on an
entry-sequenced cluster on MVS. The other functions and cluster types are
not tested.

#tab(caption: [VSAM errno values])[
  #table(columns: (1.1in, 0.6in, 1fr),
    [Name], [Value], [Meaning],
    [#cmd("EVSTYPE")], [150], [The #cmd("VSTYPE") is not one of the
      three types.],
    [#cmd("EVSACCESS")], [151], [The #cmd("VSACCESS") is not valid.],
    [#cmd("EVSMODE")], [152], [The #cmd("VSMODE") is not valid, or not
      valid with this type and access (@mvs-vsam-open-rules).],
    [#cmd("EVSOPEN")], [153], [The cluster could not be opened.],
    [#cmd("EVSERROR")], [154], [A VSAM request failed.],
  )
] <mvs-vsam-errno>

== The VSFILE Handle <mvs-vsam-vsfile>
#idx("VSFILE")

#cmd("vsopen()") allocates the handle and #cmd("vsclose()") frees it. A program
reads the fields of @mvs-vsam-vsfile-tab, chiefly the VSAM codes after an
error, and changes none of them.

#tab(caption: [VSFILE fields])[
  #table(columns: (0.95in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("eye")], [X'00'], [#cmd("\"*VSFILE*\""), for dumps.],
    [#cmd("ddname")], [X'08'], [The DD name, padded with blanks.],
    [#cmd("zero")], [X'10'], [Zero, so that #cmd("ddname") can be printed
      as a string when it is eight characters long.],
    [#cmd("flags")], [X'11'], [#cmd("VSFILE_FLAG_OPEN") (X'80') open;
      #cmd("VSFILE_FLAG_WRITE") (X'20') a #cmd("vswrite()") since the
      last #cmd("vsread()");
      #cmd("VSFILE_FLAG_ERROR") (X'02') an error occurred;
      #cmd("VSFILE_FLAG_EOF") (X'01') end of data was reached.
      #cmd("VSFILE_FLAG_STARTGE") (X'40') is defined and never set.],
    [#cmd("rc")], [X'12'], [The VSAM return code of the failed request
      (RPL feedback byte 1): 8 logical, 12 physical error.],
    [#cmd("rsn")], [X'13'], [The VSAM reason (error) code, for example 16
      (X'10') for a record not found.],
    [#cmd("component")], [X'14'], [The component code of the feedback.],
    [#cmd("type")], [X'15'], [The #cmd("VSTYPE") given to #cmd("vsopen()").],
    [#cmd("access")], [X'16'], [The #cmd("VSACCESS").],
    [#cmd("mode")], [X'17'], [The #cmd("VSMODE").],
    [#cmd("acb")], [X'18'], [The ACB (@mvs-vsam-acbrpl).],
    [#cmd("avail")], [X'64'], [Not used.],
    [#cmd("rpl")], [X'68'], [The RPL (@mvs-vsam-acbrpl).],
    [#cmd("vsself")], [X'B4'], [The address of the handle itself.],
    [#cmd("vsreset")], [X'B8'], [Private to the library.],
  )
] <mvs-vsam-vsfile-tab>

The handle is 188 bytes long. The header comments give
#cmd("vsself") as X'B8' and the length as 192 bytes, and so does the
assembler mapping #cmd("clibvsfi.copy") in the library's macro library,
which the library does not use. The offsets in the table are those the
compiler assigns.

#tab(caption: [ACB and RPL fields a caller may read])[
  #table(columns: (0.95in, 0.85in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("acbddnm")], [ACB X'28'], [The DD name (before OPEN).],
    [#cmd("acboflgs")], [ACB X'30'], [Open flags; #cmd("ACBOPEN") (X'10') is
      on while the cluster is open.],
    [#cmd("acberflg")], [ACB X'31'], [The OPEN or CLOSE error code.],
    [#cmd("acblrecl")], [ACB X'3E'], [The logical record length.],
    [#cmd("rplfdbwd")], [RPL X'0C'], [The feedback word of the last request:
      #cmd("rplrtncd") (X'0D') return code, #cmd("rplcmpon") (X'0E')
      component, #cmd("rplerrcd") (X'0F') reason code.],
    [#cmd("rplrlen")], [RPL X'30'], [The length of the last record.],
    [#cmd("rplrbar")], [RPL X'3C'], [The RBA of the last record.],
  )
] <mvs-vsam-acbrpl>

#cmd("ACB") and #cmd("RPL") map the whole control blocks, with the flag
names of the IBM macros #cmd("IFGACB") and #cmd("IFGRPL")\; use
#cmd("vslrecl()"), #cmd("vscinv()") and #cmd("vstell()") rather than reading them
directly.

== Types, Access and Modes <mvs-vsam-enums>
#idx("VSTYPE")#idx("VSACCESS")#idx("VSMODE")

#tab(caption: [VSTYPE, VSACCESS and VSMODE])[
  #table(columns: (1.5in, 0.4in, 1fr),
    [Name], [Value], [Meaning and macro options],
    [#cmd("VSTYPE_KSDS")], [0], [Key sequenced; #cmd("MACRF=KEY"),
      #cmd("OPTCD=KEY"). The search argument is a key.],
    [#cmd("VSTYPE_ESDS")], [1], [Entry sequenced; #cmd("MACRF=ADR"),
      #cmd("OPTCD=ADR"). The argument is a 4-byte RBA.],
    [#cmd("VSTYPE_RRDS")], [2], [Relative record; #cmd("MACRF=KEY"),
      #cmd("OPTCD=KEY"). The argument is a 4-byte relative record
      number.],
    [#cmd("VSACCESS_DIR")], [0], [Direct; #cmd("MACRF=DIR"),
      #cmd("OPTCD=DIR").],
    [#cmd("VSACCESS_SEQ")], [1], [Sequential; #cmd("MACRF=SEQ"),
      #cmd("OPTCD=SEQ").],
    [#cmd("VSACCESS_DYNAM")], [2], [Both: #cmd("MACRF=(SEQ,DIR)"),
      #cmd("OPTCD=SEQ")\; position with a key, then read on.],
    [#cmd("VSACCESS_ADR")], [3], [By address; #cmd("MACRF=ADR"),
      #cmd("OPTCD=ADR").],
    [#cmd("VSMODE_IN")], [0], [Read only; #cmd("MACRF=IN"),
      #cmd("OPTCD=NUP").],
    [#cmd("VSMODE_OUT")], [1], [Write only; #cmd("MACRF=OUT"),
      #cmd("OPTCD=NUP").],
    [#cmd("VSMODE_UPD")], [2], [Read and write; #cmd("MACRF=OUT"),
      #cmd("OPTCD=UPD").],
  )
] <mvs-vsam-enums-tab>

#tab(caption: [Combinations vsopen refuses])[
  #table(columns: (1fr, 1fr, 1fr, 0.9in),
    [Type], [Access], [Mode], [errno],
    [KSDS], [#cmd("VSACCESS_DIR")], [#cmd("VSMODE_OUT")], [#cmd("EVSMODE")],
    [KSDS], [#cmd("VSACCESS_DYNAM")], [#cmd("VSMODE_OUT")], [#cmd("EVSMODE")],
    [RRDS], [#cmd("VSACCESS_DYNAM")], [#cmd("VSMODE_OUT")], [#cmd("EVSMODE")],
    [any other value], [], [], [#cmd("EVSTYPE")],
    [], [any other value], [], [#cmd("EVSACCESS")],
    [], [], [any other value], [#cmd("EVSMODE")],
  )
] <mvs-vsam-open-rules>

Every other combination is passed to VSAM, which refuses at OPEN those it
does not support.

== vsopen, \_\_vsopen <mvs-vsam-vsopen>
#idx("vsopen")#idx("__vsopen")

=== Format
```
#include <mvs/vsam.h>

int vsopen(const char *dd, VSTYPE type, VSACCESS access, VSMODE mode,
           VSFILE **vsfile);
int __vsopen(const char *dd, VSTYPE type, VSACCESS access, VSMODE mode,
           VSFILE **vsfile);
```

=== Description
#cmd("vsopen()") allocates a #cmd("VSFILE"), builds its ACB and RPL for
#var("type"), #var("access") and #var("mode") (@mvs-vsam-enums-tab), opens
the cluster allocated to DD #var("dd"), and stores the handle in
#var("*vsfile"). The two names are the same function.

=== Returns
0 when the cluster is open; otherwise the #cmd("errno") value of the failure,
which is also stored in #cmd("errno"). On failure #var("*vsfile") is set to
#cmd("NULL") and nothing is left allocated.

=== Errors
#deflist(width: 1.0in,
  [#cmd("EVSTYPE")], [#var("type") is not valid.],
  [#cmd("EVSACCESS")], [#var("access") is not valid.],
  [#cmd("EVSMODE")], [#var("mode") is not valid, or not with this type and
    access (@mvs-vsam-open-rules).],
  [#cmd("ENOMEM")], [No storage for the handle.],
  [#cmd("EVSOPEN")], [OPEN did not open the ACB, for example because the DD
    is missing or names no VSAM cluster.],
)

=== Notes
#var("dd") is used as given: it is not converted to uppercase, no
#cmd("DD:") prefix is removed, and only its first eight characters count.

=== Example
#code(read("../ex/mvs-vsam/ksdsread.c"))

=== Related
@mvs-vsam-vsclose, @mvs-vsam-vsread

== vsclose, \_\_vsclos <mvs-vsam-vsclose>
#idx("vsclose")#idx("__vsclos")

=== Format
```
#include <mvs/vsam.h>

int vsclose(VSFILE *vs);
int __vsclos(VSFILE *vs);
```

=== Description
Closes the cluster, when it is open, and frees the handle. A #cmd("NULL")
#var("vs") is accepted by #cmd("__vsclos()").

=== Returns
Always 0; the return code of CLOSE is not reported.

=== Related
@mvs-vsam-vsopen

== vsread, \_\_vsread <mvs-vsam-vsread>
#idx("vsread")#idx("__vsread")#idx("GET", "VSAM")

=== Format
```
#include <mvs/vsam.h>

int vsread(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
int __vsread(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
```

=== Description
Reads one record into #var("rec"), an area of #var("reclen") bytes. With
#var("key") and a non-zero #var("keylen") the function first positions to the
record whose key equals #var("key") (as #cmd("vssteq()") does) and reads it;
otherwise it reads the next record, or for direct access the record the
argument already in the RPL names. For an ESDS #var("key") is an RBA and for
an RRDS a relative record number, each four bytes.

=== Returns
The length of the record read. -1 when the read reached the end of the data,
and for every further read after the end: VSAM answers that one with a
logical error (feedback code X'04'), which the function reports as end of
data. -2 when the GET failed.

=== Errors
#deflist(width: 1.0in,
  [#cmd("EVSERROR")], [The GET failed. #cmd("vs->rc") and #cmd("vs->rsn")
    hold the VSAM codes.],
)

=== Notes
- The return value reflects only this call's GET. The end-of-data and error
  flags that #cmd("vseof()") and #cmd("vserror()") test stay set until
  #cmd("vsclear()"), like #cmd("feof()") and #cmd("ferror()") of a stream,
  but they do not make a later read fail.
- A key that is not found is a logical error (reason code 16), so the read
  returns -2, not -1.
=== Related
@mvs-vsam-vssteq, @mvs-vsam-vsclear

== vswrite, \_\_vswrit <mvs-vsam-vswrite>
#idx("vswrite")#idx("__vswrit")#idx("PUT", "VSAM")

=== Format
```
#include <mvs/vsam.h>

int vswrite(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
int __vswrit(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
```

=== Description
Writes #var("rec"), #var("reclen") bytes long, as a new record with
#cmd("PUT"). #var("key") and #var("keylen") set the search argument for
direct requests: for an RRDS the relative record number to write; for a
KSDS the key is taken from the record itself. On a handle opened with
#cmd("VSMODE_UPD") the RPL is switched to #cmd("NUP") for the write and back
to #cmd("UPD") afterwards.

=== Returns
0 when the record was written; 8 for a logical error (for example a
duplicate key) or 12 for a physical error, with the VSAM codes in the handle.

=== Related
@mvs-vsam-vsupdate

== vsupdate, \_\_vsupdt <mvs-vsam-vsupdate>
#idx("vsupdate")#idx("__vsupdt")

=== Format
```
#include <mvs/vsam.h>

int vsupdate(VSFILE *vs, void *rec, int reclen);
int __vsupdt(VSFILE *vs, void *rec, int reclen);
```

=== Description
Rewrites the record last read with #var("rec") of #var("reclen") bytes, with
#cmd("PUT") under #cmd("OPTCD=UPD"). The handle must be opened with
#cmd("VSMODE_UPD"), and the record read with #cmd("vsread()") on the same
handle immediately before.

=== Returns
0, 8 or 12, as for #cmd("vswrite()").

=== Related
@mvs-vsam-vsread, @mvs-vsam-vsdelete

== vsdelete, \_\_vsdel <mvs-vsam-vsdelete>
#idx("vsdelete")#idx("__vsdel")#idx("ERASE")

=== Format
```
#include <mvs/vsam.h>

int vsdelete(VSFILE *vs, void *rec, int reclen);
int __vsdel(VSFILE *vs, void *rec, int reclen);
```

=== Description
Deletes the record last read with #cmd("ERASE"). As for #cmd("vsupdate()"),
the handle must be opened with #cmd("VSMODE_UPD") and the record read just
before. #var("rec") and #var("reclen") describe the record area.

=== Returns
0, 8 or 12, as for #cmd("vswrite()").

=== Notes
An ESDS record cannot be erased; VSAM rejects the request.

=== Related
@mvs-vsam-vsupdate

== vssteq, vsstge <mvs-vsam-vssteq>
#idx("vssteq")#idx("__vssteq")#idx("vsstge")#idx("__vsstge")#idx("POINT")

=== Format
```
#include <mvs/vsam.h>

int vssteq(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
int __vssteq(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
int vsstge(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
int __vsstge(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
```

=== Description
Position the handle with #cmd("POINT") for the next sequential
#cmd("vsread()"): #cmd("vssteq()") to the record whose key equals #var("key"),
#cmd("vsstge()") to the first record whose key is equal or greater.
#var("keylen") may be shorter than the full key, for a generic search.
#var("rec") and #var("reclen") set the record area of the RPL.

=== Returns
0 when positioned; 8 for a logical error, such as no record with that key,
or 12 for a physical error.

=== Notes
A failed positioning sets the error flag of the handle, which
#cmd("vserror()") reports until #cmd("vsclear()") resets it. It does not
make a later #cmd("vsread()") fail.

=== Related
@mvs-vsam-vsread

== vstell, \_\_vstell <mvs-vsam-vstell>
#idx("vstell")#idx("__vstell")#idx("RBA")

=== Format
```
#include <mvs/vsam.h>

unsigned vstell(VSFILE *vs);
unsigned __vstell(VSFILE *vs);
```

=== Returns
The relative byte address of the record of the last request (#cmd("SHOWCB")
#cmd("FIELDS=RBA") of the RPL).

=== Notes
The RBA can be given back as the #var("key") of an ESDS read.

== vsclear, vseof, vserror <mvs-vsam-vsclear>
#idx("vsclear")#idx("__vsclr")#idx("vseof")#idx("__vseof")#idx("vserror")#idx("__vserr")

=== Format
```
#include <mvs/vsam.h>

int vsclear(VSFILE *vs);
int __vsclr(VSFILE *vs);
int vseof(VSFILE *vs);
int __vseof(VSFILE *vs);
int vserror(VSFILE *vs);
int __vserr(VSFILE *vs);
```

=== Description
#cmd("vsclear()") resets the end-of-data and error flags and the return,
reason and component codes in the handle. #cmd("vseof()") and #cmd("vserror()")
test the flags.

=== Returns
#cmd("vsclear()") returns 0. #cmd("vseof()") returns 1 when end of data was
reached and #cmd("vserror()") 1 when an error occurred; otherwise 0.

== vslrecl, vscinv <mvs-vsam-vslrecl>
#idx("vslrecl")#idx("__vslrec")#idx("vscinv")#idx("__vscinv")

=== Format
```
#include <mvs/vsam.h>

unsigned vslrecl(VSFILE *vs);
unsigned __vslrec(VSFILE *vs);
unsigned vscinv(VSFILE *vs);
unsigned __vscinv(VSFILE *vs);
```

=== Returns
#cmd("vslrecl()") returns the maximum logical record length of the data
component, #cmd("vscinv()") its control interval size, each from
#cmd("SHOWCB") of the open ACB.

== \_\_vsmdfy <mvs-vsam-vsmdfy>
#idx("__vsmdfy")#idx("MODCB")

=== Format
```
#include <mvs/vsam.h>

int __vsmdfy(VSFILE *vs, void *rec, int reclen, void *key, int keylen);
```

=== Description
Sets the record area of the RPL (#cmd("AREA"), #cmd("AREALEN") and
#cmd("RECLEN")) to #var("rec") and #var("reclen") with #cmd("MODCB"). When
#var("key") is not #cmd("NULL") it also sets the search argument: for a KSDS
#cmd("ARG") and #cmd("KEYLEN"), for an ESDS or RRDS #cmd("ARG") alone. The
read, write and positioning functions call it; a program calls it before
issuing its own requests against #cmd("vs->rpl").

=== Returns
Always 0.

== \_\_vsshwc <mvs-vsam-vsshwc>
#idx("__vsshwc")#idx("SHOWCAT")

=== Format
```
#include <mvs/vsam.h>

int __vsshwc(ACB *acb, const char *name, char *buf, int buflen);
```

=== Description
#cmd("__vsshwc()") issues #cmd("SHOWCAT") for the catalog entry
#var("name"), a string of up to 44 characters that the function pads with
blanks, and returns the description in #var("buf"), an area of
#var("buflen") bytes. #var("acb") may be #cmd("NULL")\; otherwise it names an
open ACB whose catalog is searched. The first halfword of #var("buf") is set
to #var("buflen") before the call. The area is mapped by #cmd("SHWOUT"), or
by #cmd("SHWOUTDI") when the entry is a data or index component.

=== Returns
The return code of SHOWCAT: 0 when the entry was found.

#tab(caption: [SHWOUT, for a cluster, alternate index, path or upgrade set])[
  #table(columns: (0.9in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("shwlen1")], [X'00'], [The length of the area (set by
      #cmd("__vsshwc()")).],
    [#cmd("shwlen2")], [X'02'], [The length SHOWCAT needed.],
    [#cmd("shwacbp")], [X'04'], [The address of the catalog ACB.],
    [#cmd("shwtype")], [X'08'], [The entry type: #cmd("'C'") cluster,
      #cmd("'G'") alternate index, #cmd("'R'") path, #cmd("'Y'") upgrade
      set.],
    [#cmd("shwattr")], [X'09'], [#cmd("SHWUP") (X'80'): update path or upgrade
      member.],
    [#cmd("shwact")], [X'0A'], [The number of associations that follow.],
    [#cmd("shwass")], [X'0C'], [The associations.],
  )
] <mvs-vsam-shwout>

#tab(caption: [SHWOUTDI, for a data or index component])[
  #table(columns: (0.9in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("shwlen1")], [X'00'], [As in #cmd("SHWOUT").],
    [#cmd("shwlen2")], [X'02'], [As in #cmd("SHWOUT").],
    [#cmd("shwacbp")], [X'04'], [As in #cmd("SHWOUT").],
    [#cmd("shwtype")], [X'08'], [#cmd("'D'") data or #cmd("'I'") index.],
    [#cmd("shwdsb")], [X'0A'], [Data set flags.],
    [#cmd("shwkeyln")], [X'0C'], [The key length.],
    [#cmd("shwcisz")], [X'0E'], [The control interval size, four bytes.],
    [#cmd("shwmrec")], [X'12'], [The maximum record size, four bytes.],
    [#cmd("shwact")], [X'16'], [The number of associations.],
    [#cmd("shwass")], [X'18'], [The associations.],
  )
] <mvs-vsam-shwoutdi>

Each association is four bytes, mapped by #cmd("struct shwassoc"): the type
of the associated entry (#cmd("shwatype")) and its 3-byte catalog control
interval number (#cmd("shwaci")). #cmd("SHWPL"), the 16-byte SHOWCAT
parameter list, is built by the function and need not be filled by the
caller.
