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
needed. A cluster's password is given to #cmd("__vsopenx()")
(@mvs-vsam-vsopenx)\; #cmd("vsopen()") passes none.

*Two forms of every function.* Each operation exists as #cmd("vs")#var("name")
and #cmd("__vs")#var("name"). The #cmd("vs") form takes a lock on the handle
(the #cmd("lock()") function with the handle address as resource, see
@mvs-sync) around the call, so that several tasks can share a handle; the
#cmd("__vs") form takes no lock. #cmd("vsopen()") takes no lock either -- there
is no handle yet. #cmd("__vsmdfy()"), #cmd("__vsshwc()"),
#cmd("__vsfdbk()"), #cmd("__vsopenx()") and #cmd("__vsinfo()") exist only
in the #cmd("__") form.

*Errors.* VSAM reports a failed request through the exit routines that
#cmd("__vsopen()") installs (LERAD, SYNAD and EODAD). LERAD and SYNAD
copy the return, component and reason codes of the RPL feedback word,
which VSAM has set when it takes the exit, into the handle's #cmd("rc"),
#cmd("component") and #cmd("rsn") fields and set the error flag\; EODAD
only sets the end-of-file flag. After a failed request #cmd("rc") is 8 or
12. Up to release 2.23.0 #cmd("rc") and #cmd("component") stayed 0
(@mvs-vsam-vsfile-tab).
The request returns 8 (logical error), 12 (physical error) or 4 (end of
data). *The flags stay set* until
#cmd("vsclear()") resets them, as the end-of-file and error indicators of a
stream do, and #cmd("vseof()") and #cmd("vserror()") test them. The value
#cmd("vsread()") returns reflects only its own GET.

*The outcome of each request.* #cmd("__vsfdbk()") (@mvs-vsam-vsfdbk) tells,
after any call, what the last request on the handle did: its return code,
the feedback code, the record length and the RBA. Unlike the codes in the
handle and the flags, these describe that one request and are never left
over from an earlier one, so a program that asks #cmd("__vsfdbk()") after
each call needs neither #cmd("vsclear()") nor the exit routines' codes. The #cmd("errno") values of
@mvs-vsam-errno are set by #cmd("vsopen()") and #cmd("vsread()").

The library's tests cover #cmd("vsopen()"), #cmd("vsread()"),
#cmd("vseof()"), #cmd("vserror()") and #cmd("vsclear()") on an
entry-sequenced cluster on MVS, and on a key-sequenced cluster the
sequential load, #cmd("vswrite()") and #cmd("vsread()") by key with dynamic
access, #cmd("vsupdate()"), and #cmd("__vsfdbk()") after a read, a read of
a missing key, a duplicate write and the end of data. #cmd("__vsopenx()")
and #cmd("__vsinfo()") are tested on MVS with a key-sequenced, an
entry-sequenced and a relative-record cluster opened with
#cmd("VSTYPE_ANY"), a REUSE cluster emptied with #cmd("VSOPT_RST"), the
buffer counts, a read password, #cmd("VSOPT_SKP"), #cmd("VSOPT_SIS") and an
alternate index read through its path with #cmd("VSOPT_AIX"). The other
functions are not tested.

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
    [#cmd("rc")], [X'12'], [The VSAM return code of the last failed
      request: 8 logical error, 12 physical error. The exit routines copy
      it from the RPL feedback word (#cmd("rplrtncd")). Up to release
      2.23.0 they took it from #cmd("SHOWCB FIELDS=FDBK"), which on
      MVS 3.8j gives the reason code alone, so it stayed 0 (measured).],
    [#cmd("rsn")], [X'13'], [The VSAM reason (error) code,
      #cmd("rplerrcd"), for example 16 (X'10') for a record not found or 8
      for a duplicate key.],
    [#cmd("component")], [X'14'], [The component code of the feedback,
      #cmd("rplcmpon"). Up to release 2.23.0 it stayed 0, like
      #cmd("rc").],
    [#cmd("type")], [X'15'], [The #cmd("VSTYPE") given to #cmd("vsopen()")\;
      after #cmd("__vsopenx()") with #cmd("VSTYPE_ANY"), the type OPEN
      found.],
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
    [#cmd("rplfdbwd")], [RPL X'0C'], [The feedback word of the last request
      (read it with #cmd("__vsfdbk()")):
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
    [#cmd("VSTYPE_ANY")], [3], [For #cmd("__vsopenx()") only: the type of
      the cluster, found after OPEN; #cmd("MACRF=(KEY,ADR)"), then the
      #cmd("OPTCD") of that type. #cmd("vsopen()") refuses it with
      #cmd("EVSTYPE"). A macro of type #cmd("VSTYPE"), not a member of
      the enumeration, so that a #cmd("switch") over the three types
      draws no warning.],
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

#cmd("__vsopenx()") with #cmd("VSTYPE_ANY") refuses what is refused for a
KSDS. Every other combination is passed to VSAM, which refuses at OPEN those it
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
- #var("dd") is converted to uppercase\; no #cmd("DD:") prefix is removed,
  and only its first eight characters count. Up to release 2.22.0 it was
  used as given, and a name in lower case failed with #cmd("EVSOPEN").
- A cluster that an earlier program left open, for example by an abend,
  opens with a warning from VSAM (message #cmd("IEC161I") 056-084), and
  #cmd("vsopen()") returns 0 for it. Up to release 2.22.0 it closed the
  handle again and failed with #cmd("errno") 4.

=== Example
#code(read("../ex/mvs-vsam/ksdsread.c"))

=== Related
@mvs-vsam-vsopenx, @mvs-vsam-vsclose, @mvs-vsam-vsread

== \_\_vsopenx <mvs-vsam-vsopenx>
#idx("__vsopenx")#idx("VSOPTS")#idx("password", "VSAM")

=== Format
```
#include <mvs/vsam.h>

int __vsopenx(const char *dd, const VSOPTS *opts, VSFILE **vsfile);
```

=== Description
#cmd("__vsopenx()") opens the cluster allocated to DD #var("dd") as
#cmd("vsopen()") does, with the type, access and mode of #var("*opts"), and
gives OPEN what #cmd("vsopen()") cannot: the number of buffers, a
password, the #cmd("MACRF") options #cmd("RST"), #cmd("SKP"), #cmd("SIS")
and #cmd("AIX"), and the type left to OPEN. It stores the handle in
#var("*vsfile")\; every other function takes the handle as one from
#cmd("vsopen()"). #cmd("vsopen()") itself is unchanged. The caller
allocates the #cmd("VSOPTS") and sets every field\; a field of 0 leaves the
ACB as #cmd("vsopen()") builds it.

#tab(caption: [VSOPTS fields])[
  #table(columns: (0.8in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("type")], [X'00'], [#cmd("VSTYPE_KSDS"), #cmd("VSTYPE_ESDS"),
      #cmd("VSTYPE_RRDS"), or #cmd("VSTYPE_ANY"): the handle is opened keyed
      and addressed, and after OPEN its RPL and its #cmd("type") are set to
      the type of the cluster (#cmd("TESTCB ATRB"), as #cmd("__vsinfo()")
      reports it).],
    [#cmd("access")], [X'04'], [The #cmd("VSACCESS"), as for
      #cmd("vsopen()").],
    [#cmd("mode")], [X'08'], [The #cmd("VSMODE"), as for #cmd("vsopen()").],
    [#cmd("bufnd")], [X'0C'], [Data buffers, the ACB's #cmd("BUFND")
      (#cmd("unsigned short")).],
    [#cmd("bufni")], [X'0E'], [Index buffers, #cmd("BUFNI")
      (#cmd("unsigned short")).],
    [#cmd("bufsp")], [X'10'], [Buffer space in bytes, #cmd("BUFSP").],
    [#cmd("password")], [X'14'], [The password, a string of 1 to 8
      characters as the cluster was defined with it (IDCAMS takes it in
      upper case), or #cmd("NULL") for none. OPEN reads it\; the ACB does
      not keep its address.],
    [#cmd("flags")], [X'18'], [The options of @mvs-vsam-vsopt, or 0.],
  )
] <mvs-vsam-vsopts-tab>

The structure is 28 bytes long.

#tab(caption: [VSOPTS flags])[
  #table(columns: (1.1in, 0.5in, 1fr),
    [Flag], [Value], [Meaning],
    [#cmd("VSOPT_RST")], [X'01'], [#cmd("MACRF=RST"): a cluster defined with
      #cmd("REUSE") is emptied at OPEN. Meant for #cmd("VSMODE_OUT") or
      #cmd("VSMODE_UPD")\; with #cmd("VSMODE_IN") OPEN empties the cluster
      as well, and the next OPEN for input fails, because the cluster is
      empty (message #cmd("IEC161I") 072, measured). A cluster without
      #cmd("REUSE") fails OPEN (#cmd("EVSOPEN"), #cmd("acberflg") X'E8',
      measured).],
    [#cmd("VSOPT_SKP")], [X'02'], [#cmd("MACRF=SKP") and the RPL's
      #cmd("OPTCD=SKP"): skip-sequential processing. Each #cmd("vsread()")
      then takes a key, in ascending order, and reads the record with that
      key without a pass over the records between. Not for an ESDS or
      #cmd("VSACCESS_ADR").],
    [#cmd("VSOPT_SIS")], [X'04'], [#cmd("MACRF=SIS"): the sequential insert
      strategy for keyed inserts, which splits a control interval or area
      at the insert point instead of in the middle.],
    [#cmd("VSOPT_AIX")], [X'08'], [#cmd("MACRF=AIX"): #var("dd") names a
      path\; the alternate index itself is processed, not the base cluster
      through it. Its records are the alternate key and the prime keys
      behind a 5-byte header. On a DD that names a cluster, not a path,
      OPEN ignores it (measured).],
  )
] <mvs-vsam-vsopt>

=== Returns
0 when the cluster is open; otherwise the #cmd("errno") value of the
failure, which is also stored in #cmd("errno"). On failure
#var("*vsfile") is set to #cmd("NULL") and nothing is left allocated.

=== Errors
#deflist(width: 1.0in,
  [#cmd("EINVAL")], [#var("opts") or #var("vsfile") is #cmd("NULL"), a
    flag other than those of @mvs-vsam-vsopt is set, or the password is
    longer than 8 characters.],
  [#cmd("EVSACCESS")], [#cmd("VSOPT_SKP") with #cmd("VSTYPE_ESDS") or
    #cmd("VSACCESS_ADR")\; or with #cmd("VSTYPE_ANY") on a cluster that
    OPEN found to be an ESDS, which is then closed again.],
  [#cmd("EVSTYPE"), #cmd("EVSMODE")], [As for #cmd("vsopen()")
    (@mvs-vsam-open-rules).],
  [#cmd("ENOMEM")], [No storage for the handle.],
  [#cmd("EVSOPEN")], [OPEN did not open the ACB: as for #cmd("vsopen()"),
    and for a missing or wrong password or #cmd("VSOPT_RST") on a cluster
    without #cmd("REUSE"). Also when #cmd("TESTCB") cannot tell the type for
    #cmd("VSTYPE_ANY").],
)

=== Notes
- *Passwords.* VSAM's catalog code checks a cluster's password at OPEN
  only when the catalog holding it has a password itself and the cluster
  is not RACF-indicated (#cmd("RACF") #cmd("(YES)") in the LISTCAT
  output)\; for a RACF-indicated cluster the security product decides. On
  MVS/CE with RAKF the job's ACEE carries automatic data set protection,
  so every entry DEFINE created was RACF-indicated, and the protected
  cluster opened with the right password, a wrong one and none
  (measured)\; the refusal of a wrong password could not be measured
  there. Define a protected cluster with #cmd("ATTEMPTS(0)"), so that
  VSAM does not ask the operator for a missing password and wait for the
  reply.
- *Buffers.* #cmd("__vsinfo()") reports the values the ACB was given:
  0 for a field that was left 0, not the default VSAM chose.
- *VSOPT_SKP and writes.* A #cmd("vswrite()") on a key-sequenced cluster
  opened with #cmd("VSACCESS_DYNAM") and #cmd("VSMODE_UPD") sets the RPL
  back to #cmd("OPTCD=SEQ") after the write\; the reads after it are no
  longer skip-sequential.
- A program that calls only #cmd("vsopen()") links neither
  #cmd("__vsopenx()") nor #cmd("__vsinfo()").
- There is no #cmd("vsopenx()"): the function takes no lock, like
  #cmd("vsopen()").

=== Example
```
VSOPTS  o;
VSINFO  in;
VSFILE  *vs;

memset(&o, 0, sizeof(o));
o.type      = VSTYPE_ANY;
o.access    = VSACCESS_SEQ;
o.mode      = VSMODE_IN;
o.bufnd     = 6;
o.password  = "RDPW";
if (__vsopenx("MASTER", &o, &vs) == 0) {
    __vsinfo(vs, &in);
    printf("type %d, key %u at %u, records up to %u bytes\n",
           in.type, in.keylen, in.rkp, in.lrecl);
    vsclose(vs);
}
```

=== Related
@mvs-vsam-vsopen, @mvs-vsam-vsinfo, @mvs-vsam-enums

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
The return code of CLOSE, 0 when it closed cleanly, and 0 for a handle that
was not open. Up to release 2.22.0 always 0.

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
  returns -2, not -1. The positioning fails, and no #cmd("GET") is issued;
  up to 2.10.0 the #cmd("GET") ran anyway and the handle reported its
  reason code, 88.
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

On a KSDS opened with #cmd("VSACCESS_DYNAM") the write is a direct
#cmd("PUT") (#cmd("OPTCD=DIR")), so a record goes in at its key wherever
the handle is positioned, and the RPL returns to sequential processing
afterwards. A KSDS opened with #cmd("VSACCESS_DIR") writes direct anyway,
and one opened with #cmd("VSACCESS_SEQ") is loaded in key order.

=== Returns
0 when the record was written; 8 for a logical error or 12 for a physical
error, with the VSAM codes in the handle. On a KSDS a key that is there
already is reason code 8.

=== Notes
- Up to 2.10.0 a KSDS opened with #cmd("VSACCESS_DYNAM") wrote
  keyed-sequentially: after a record with a higher key, a lower key and a
  duplicate both failed with reason code 12 (out of sequence), and the two
  could not be told apart.

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

== \_\_vsfdbk <mvs-vsam-vsfdbk>
#idx("__vsfdbk")#idx("VSFDBK")#idx("feedback code", "VSAM")

=== Format
```
#include <mvs/vsam.h>

int __vsfdbk(VSFILE *vs, VSFDBK *fb);
```

=== Description
Stores in #var("*fb") the outcome of the last request -- #cmd("GET"),
#cmd("PUT"), #cmd("POINT") or #cmd("ERASE") -- that a function issued on
the handle #var("vs"): the return code, the component and reason codes of
the RPL feedback word, and the record length and RBA (#cmd("SHOWCB")
#cmd("FIELDS=(RECLEN,RBA)")). Call it after any read, write, update,
delete or positioning call, before the next one.

VSAM sets these fields on every request, so they describe that request
alone: a successful read after a failed one reports return code 0 and
feedback code 0, and the end of the data reports its own code, not the one
of an earlier error. The handle's #cmd("rc"), #cmd("rsn") and
#cmd("component") fields and the flags that #cmd("vseof()") and
#cmd("vserror()") test are not used and not changed.

#tab(caption: [VSFDBK fields])[
  #table(columns: (0.8in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("r15")], [X'00'], [The return code, #cmd("int"): 0 success, 8
      logical error, 12 physical error. It is the return code byte of the
      RPL feedback word (#cmd("rplrtncd")), the value VSAM also returns in
      register 15.],
    [#cmd("fdbk")], [X'04'], [The feedback (reason) code,
      #cmd("rplerrcd"): with return code 8 the logical error, with 12
      the physical error, and with 0 a note such as X'08' (more records
      with this key on a non-unique alternate index path).],
    [#cmd("comp")], [X'05'], [The component code, #cmd("rplcmpon").],
    [#cmd("rsvd1")], [X'06'], [Reserved, set to 0.],
    [#cmd("reclen")], [X'08'], [The record length: of the record read by a
      successful #cmd("GET") or written by a #cmd("PUT").],
    [#cmd("rba")], [X'0C'], [The relative byte address of that record, as
      #cmd("vstell()") returns it.],
    [#cmd("recaddr")], [X'10'], [The address of the record in VSAM's
      buffer for a locate-mode #cmd("GET")\; the functions use move mode,
      so it is #cmd("NULL").],
    [#cmd("rsvd2")], [X'14'], [Reserved, set to 0.],
  )
] <mvs-vsam-vsfdbk-tab>

The structure is 24 bytes long and the caller provides it.

#tab(caption: [Common results])[
  #table(columns: (2.2in, 0.5in, 0.6in, 1fr),
    [Request], [#cmd("r15")], [#cmd("fdbk")], [What the function returned],
    [Record read], [0], [0], [#cmd("vsread()"): the length],
    [Read past the last record], [8], [X'04'], [#cmd("vsread()"): -1],
    [Key not found], [8], [X'10'], [#cmd("vsread()"): -2],
    [Write of a key that is there (direct)], [8], [X'08'],
      [#cmd("vswrite()"): 8],
    [Key out of sequence (load)], [8], [X'0C'], [#cmd("vswrite()"): 8],
  )
] <mvs-vsam-vsfdbk-codes>

The first four rows are measured on MVS 3.8j with a key-sequenced cluster
opened with #cmd("VSACCESS_DYNAM") and #cmd("VSMODE_UPD")\; the reason
codes are VSAM's and the same for every function that issues the request.

=== Returns
0 when #var("*fb") is filled. -1 when #var("vs") or #var("fb") is
#cmd("NULL")\; otherwise the non-zero return code of #cmd("SHOWCB"), and
#var("*fb") then holds the codes but no record length and RBA.

=== Notes
- #cmd("reclen") and #cmd("rba") are those VSAM left in the RPL\; after a
  failed request they need not belong to any record. Use them when
  #cmd("r15") is 0.
- A read by key that does not find the key issues no #cmd("GET"): the
  feedback is that of the #cmd("POINT"), which is the answer.
- The read and write functions report a request VSAM did not accept at all
  (register 15 = 4, another request active on the RPL) only through their
  own return value\; the feedback word then still belongs to the active
  request. The #cmd("vs") forms lock the handle, so it does not happen
  with them.
- #cmd("__vsfdbk()") takes no lock. When tasks share a handle, the task
  that made the request must call it before another task issues the next
  one.
- The handle's #cmd("rc"), #cmd("rsn") and #cmd("component") fields hold
  the same codes for the last failed request, until #cmd("vsclear()")\;
  #cmd("__vsfdbk()") also reports a successful request and the end of the
  data. Up to release 2.23.0 #cmd("rc") stayed 0 (@mvs-vsam-vsfile-tab).

=== Example
```
VSFDBK  fb;
int     len = vsread(vs, rec, sizeof(rec), key, 8);

__vsfdbk(vs, &fb);
if (len == -2 && fb.r15 == 8 && fb.fdbk == 0x10)
    printf("no record with this key\n");
```

=== Related
@mvs-vsam-vsread, @mvs-vsam-vswrite, @mvs-vsam-vstell, @mvs-vsam-vsclear

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
#cmd("SHOWCB") of the open ACB. #cmd("__vsinfo()") (@mvs-vsam-vsinfo)
returns both with the type, the key and the buffers.

== \_\_vsinfo <mvs-vsam-vsinfo>
#idx("__vsinfo")#idx("VSINFO")#idx("TESTCB")

=== Format
```
#include <mvs/vsam.h>

int __vsinfo(VSFILE *vs, VSINFO *info);
```

=== Description
Stores in #var("*info") the attributes of the cluster open on #var("vs"):
its type from #cmd("TESTCB ATRB"), and its key length, key position,
maximum record length, control interval size and buffers from
#cmd("SHOWCB ACB"). It works on a handle from #cmd("vsopen()") as well as
from #cmd("__vsopenx()"), and the type is the cluster's own, not the one
the handle was opened with. The caller allocates the #cmd("VSINFO").

#tab(caption: [VSINFO fields])[
  #table(columns: (0.8in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("type")], [X'00'], [#cmd("VSTYPE_KSDS"), #cmd("VSTYPE_ESDS") or
      #cmd("VSTYPE_RRDS"). An alternate index opened with
      #cmd("VSOPT_AIX"), and a path, report #cmd("VSTYPE_KSDS").],
    [#cmd("keylen")], [X'04'], [The key length, #cmd("KEYLEN")\; 0 for an
      ESDS and an RRDS. Through a path, the alternate key's.],
    [#cmd("rkp")], [X'08'], [The relative key position, #cmd("RKP"). Of an
      alternate index opened with #cmd("VSOPT_AIX"), 5: the key follows the
      record header.],
    [#cmd("lrecl")], [X'0C'], [The maximum record length, #cmd("LRECL"), as
      #cmd("vslrecl()") returns it.],
    [#cmd("cisz")], [X'10'], [The data control interval size, #cmd("CINV"),
      as #cmd("vscinv()") returns it.],
    [#cmd("bufnd")], [X'14'], [#cmd("BUFND") of the ACB.],
    [#cmd("bufni")], [X'18'], [#cmd("BUFNI") of the ACB.],
    [#cmd("bufsp")], [X'1C'], [#cmd("BUFSP") of the ACB.],
  )
] <mvs-vsam-vsinfo-tab>

The structure is 32 bytes long. On MVS 3.8j #cmd("SHOWCB") returns the
buffer fields of the ACB as the program set them: 0 when it set none, not
the numbers VSAM chose (measured).

=== Returns
0 when #var("*info") is filled. -1 when #var("vs") or #var("info") is
#cmd("NULL") or #cmd("TESTCB") fails\; otherwise the non-zero return code
of #cmd("SHOWCB").

=== Notes
- Measured on MVS 3.8j: a KSDS with #cmd("KEYS(8 0)"),
  #cmd("RECORDSIZE(80 80)") and a 4096-byte CI gives #cmd("VSTYPE_KSDS"),
  8, 0, 80, 4096\; an ESDS with #cmd("RECORDSIZE(80 100)") gives
  #cmd("VSTYPE_ESDS"), keylen 0, lrecl 100\; an alternate index with
  #cmd("KEYS(6 10)") read through its path with #cmd("VSOPT_AIX") gives
  keylen 6, rkp 5, lrecl 19.

=== Related
@mvs-vsam-vsopenx, @mvs-vsam-vslrecl

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
