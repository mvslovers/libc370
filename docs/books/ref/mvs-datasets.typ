#import "../bookmaster/bookmaster.typ": *

= Data Sets and DD Statements <mvs-datasets>

// Narrow cells: ragged right, no hyphenation of the names.
#show table: set par(justify: false)
#show table: set text(hyphenate: false)

#idx("data set")
#idx("DD statement")
A C program on MVS reaches its files through DD statements and data sets,
not through a directory tree. The functions in this chapter look at both:
the DD statements allocated to a task, the catalog, the volume table of
contents (VTOC) of a disk, the directory of a partitioned data set (PDS),
and the volumes that are online. Two of them, #cmd("__fabandon()") and the
#cmd("ropen()") family, complete the input and output functions of
#cmd("<stdio.h>") for cases that a stream does not handle. The headers are:

#deflist(width: 1.35in,
  [#cmd("<mvs/dd.h>")], [the DD statements of a task: the data set
    association blocks (DSAB) and the task input/output table (TIOT), and
    the job file control block (JFCB) of a DD, read with the RDJFCB macro.],
  [#cmd("<mvs/dscb.h>")], [the data set control blocks (DSCB) of the VTOC,
    read with the OBTAIN macro (SVC 27), and the catalog, read with the
    LOCATE macro (SVC 26).],
  [#cmd("<mvs/dasd.h>")], [device types, and the number of blocks that fit
    on a track (the TRKCALC macro).],
  [#cmd("<mvs/dslist.h>")], [lists: the data sets of a catalog level
    (through IDCAMS LISTCAT), the members of a PDS, the online disk volumes
    and their free space (LSPACE, SVC 78), and the DD statements of a
    task.],
  [#cmd("<mvs/pds.h>")], [the PDS directory: the BLDL macro and STOW
    (SVC 21), and the renaming and deleting of members.],
  [#cmd("<mvs/file.h>")], [#cmd("__fabandon()"), which closes a stream that
    a failed write has left unusable.],
  [#cmd("<mvs/rfile.h>")], [sequential record input and output through
    BSAM, one logical record at a time.],
)

#idx("authorization", "data set functions")
None of these functions needs APF authorization. They read control blocks
of the caller's own task (TCB, JSCB, TIOT, DSAB) and of the system (CVT,
unit control blocks), which a program in problem state may read, and they
issue SVCs that an unauthorized program may issue. The data set security of
the system applies as it does to any program: #cmd("__renmem()") and
#cmd("__delmem()") open the data set for output, and need the authority to
update it.

#idx("data set name", "in a function argument")
The functions take a data set name or a DD name as an ordinary C string,
terminated by a null character. Where MVS wants a fixed-length field, the
library pads the string with blanks itself. This holds even where a
prototype reads #cmd("const char dsn[44]") or #cmd("const char vol[6]"): the
array size documents the field that MVS sees, and the argument is still a
string. A name is not converted to upper case unless the entry says so.

== Lists <mvs-datasets-lists>
#idx("list", "returned by a function")
#idx("arraycount")

#cmd("__listds()"), #cmd("__listpd()"), #cmd("__listvl()") and
#cmd("__listal()") return a list as a dynamic array of pointers to records:
a pointer of type #cmd("DSLIST **"), #cmd("PDSLIST **"), #cmd("VOLLIST **")
or #cmd("ALCLIST **"). The number of records is returned by
#cmd("arraycount()") from #cmd("<ext/array.h>"), which takes the address of
the list:

```
DSLIST   **list = __listds("SYS1", "VOLUME", NULL);
unsigned n      = arraycount(&list);
```

Each list is freed by its own function, which frees the records and the
array and sets the list pointer to #cmd("NULL"): #cmd("__freeds()"),
#cmd("__freepd()"), #cmd("__freevl()") or #cmd("__freeal()").

The four functions share one convention for an empty result and for a
failure. Both are returned as #cmd("NULL"), and #cmd("errno") tells them
apart:

#deflist(width: 1.35in,
  [#cmd("errno") 0], [The result is empty. Each function sets #cmd("errno")
    to 0 on entry, so a stale value from an earlier call does not show.],
  [#cmd("ENOMEM")], [Storage ran out. The records built so far have been
    freed.],
  [other], [The request failed for the reason given (#cmd("__listpd()")
    only).],
)

A list that is returned is complete: no function returns a shortened list
after a failure.

== get\_dsab, next\_dsab <mvs-datasets-get-dsab>
#idx("get_dsab")#idx("next_dsab")#idx("DSAB")

=== Format
```
#include <mvs/dd.h>

DSAB *get_dsab(void *tcbptr, const char *ddname);
DSAB *next_dsab(DSAB *dsab, void *tcbptr, const char *ddname);
```

=== Description
Each DD statement of a job step, whether it comes from the JCL or from
dynamic allocation, has a data set association block (DSAB) on a chain
anchored in the job step control block (JSCB). A concatenation has one DSAB
for each data set\; the second and later ones belong to TIOT entries with a
blank DD name.

#cmd("get_dsab()") returns the first DSAB of the task whose TCB is
#var("tcbptr"), or of the calling task when #var("tcbptr") is #cmd("NULL"):

- With #var("ddname") #cmd("NULL"), an empty string or a string that begins
  with a blank, the first DSAB on the chain.
- Otherwise the DSAB of the DD named #var("ddname"). The name is converted
  to upper case, and its first eight characters are compared.

#cmd("next_dsab()") returns the DSAB after #var("dsab"):

- With #var("ddname") #cmd("NULL") or beginning with a blank, the next DSAB
  on the chain, whatever its DD name. A loop from #cmd("get_dsab(NULL,
  NULL)") through #cmd("next_dsab()") visits every DSAB of the step.
- Otherwise the next data set of the same concatenation: the next DSAB if
  its DD name is blank, and #cmd("NULL") when the next DD has a name of its
  own. A loop from #cmd("get_dsab(NULL, ddname)") visits the data sets of
  one DD.

With #var("dsab") #cmd("NULL"), #cmd("next_dsab()") returns what
#cmd("get_dsab()") returns for #var("tcbptr") and #var("ddname").

@mvs-datasets-dsab-tab lists the fields of a DSAB that a program most often
reads. The DD name and the JFCB are reached through the TIOT entry.

#tab(caption: [DSAB fields])[
  #table(columns: (0.95in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("dsabid")], [X'00'], [#cmd("DSAB"), in EBCDIC.],
    [#cmd("dsabfchn")], [X'04'], [The next DSAB, or #cmd("NULL").],
    [#cmd("dsabbchn")], [X'08'], [The previous DSAB, or #cmd("NULL").],
    [#cmd("dsabopct")], [X'0E'], [The number of open DCBs for the DD.],
    [#cmd("dsabtiot")], [X'10'], [The TIOT entry of the DD (#cmd("TIOTDD")).
      Its #cmd("TIOEDDNM") is the DD name, padded with blanks, and its
      #cmd("TIOEJFCB") locates the JFCB.],
    [#cmd("dsabflg1")], [X'22'], [Allocation flags: permanent, dynamic,
      concatenated, in use, and others.],
    [#cmd("dsabssnm")], [X'30'], [The name of the subsystem, for a subsystem
      data set such as a JES2 SYSOUT data set.],
  )
] <mvs-datasets-dsab-tab>

=== Returns
The DSAB, or #cmd("NULL") when there is none. #cmd("NULL") is also returned
when a block on the chain does not carry the identifier #cmd("DSAB").

=== Notes
- #var("tcbptr") is used only to find the first DSAB: #cmd("next_dsab()")
  follows the chain from #var("dsab").
- To #cmd("get_dsab()") an empty #var("ddname") is the same as
  #cmd("NULL"). To #cmd("next_dsab()") it is not: it selects the
  concatenation that #var("dsab") belongs to, as a name does.
- The chain belongs to the job step, and dynamic allocation in another task
  of the step changes it. A program with several tasks serializes the walk
  itself.

=== Related
@mvs-datasets-listal, @mvs-datasets-tiot, @mvs-datasets-rdjfcb

== \_\_tiot, \_\_jobname <mvs-datasets-tiot>
#idx("__tiot")#idx("__jobname")#idx("TIOT")#idx("job name")

=== Format
```
#include <mvs/dd.h>

TIOT *__tiot(void);
const char *__jobname(void);
```

=== Description
#cmd("__tiot()") returns the address of the task input/output table (TIOT)
of the calling task, taken from #cmd("TCBTIO") of the current TCB.
#cmd("__jobname()") returns the address of the job name in that TIOT, the
field #cmd("TIOCNJOB").

=== Returns
The address of the TIOT, or of the job name.

=== Notes
The job name is eight characters long, padded with blanks, and is *not*
terminated by a null character. Print it with a precision:

```
printf("job %.8s\n", __jobname());
```

=== Related
@mvs-datasets-get-dsab

== rdjfcb, \_\_rdjfcb <mvs-datasets-rdjfcb>
#idx("rdjfcb")#idx("__rdjfcb")#idx("JFCB")#idx("RDJFCB macro")

=== Format
```
#include <mvs/dd.h>

int rdjfcb(DCB *dcb, JFCB *jfcb);
int __rdjfcb(DCB *dcb, JFCB *jfcb);
```

=== Description
#cmd("rdjfcb()") reads the job file control block (JFCB) of the DD that
#var("dcb") names into #var("jfcb"), with the RDJFCB macro. The JFCB
describes the data set as the DD statement gives it: the data set name
(#cmd("jfcbdsnm")), the member name (#cmd("jfcbelnm")), the volume serial
numbers (#cmd("jfcbvols")), the disposition, and the DCB attributes coded on
the DD.

The DCB must carry the DD name\; a DCB from #cmd("osbdcb()") (see
@mvs-osio) does. For the duration of the call the function replaces the
exit list of the DCB with a list of its own, whose one entry points at
#var("jfcb"), and restores the original afterwards.

#cmd("__rdjfcb()") is the same code under a second name.

=== Returns
The return code of RDJFCB, 0 when the JFCB was read\; -1 when #var("dcb") or
#var("jfcb") is #cmd("NULL").

=== Notes
#var("jfcb") must point to a whole #cmd("JFCB") of 176 bytes.

=== Related
@mvs-datasets-get-dsab, @mvs-osio

== \_\_locate <mvs-datasets-locate>
#idx("__locate")#idx("LOCATE macro")#idx("catalog", "locating a data set")

=== Format
```
#include <mvs/dscb.h>

int __locate(const char dsn[44], LOCWORK *workarea);
```

=== Description
#cmd("__locate()") looks up the data set #var("dsn") in the catalog with the
LOCATE macro (SVC 26). #var("dsn") is a string of up to 44 characters\; the
function pads it with blanks. When the data set is cataloged, the serial
number of its first volume is returned in #var("workarea").

#tab(caption: [LOCWORK fields])[
  #table(columns: (0.95in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("unused1")], [X'00'], [Work area.],
    [#cmd("volser")], [X'06'], [The serial number of the first volume, six
      characters, not terminated by a null character.],
    [#cmd("unused2")], [X'0C'], [Work area.],
  )
] <mvs-datasets-locwork-tab>

The structure is 268 bytes long. Set it to zeros before the call.

=== Returns
The return code of LOCATE: 0 when the data set was found.

=== Notes
- The name is passed to LOCATE as given. Pass it in upper case, fully
  qualified and without quotes.
- The list functions of this chapter use #cmd("__locate()") for a data set
  that the catalog shows on the volume #cmd("******"), the system residence
  volume, to find its real volume.

=== Example
```
LOCWORK lw;

memset(&lw, 0, sizeof(lw));
if (__locate("SYS1.PARMLIB", &lw) == 0)
    printf("on %.6s\n", lw.volser);
```

=== Related
@mvs-datasets-dscbdv

== \_\_dscbdv, \_\_dscbav, \_\_dscbv <mvs-datasets-dscbdv>
#idx("__dscbdv")#idx("__dscbav")#idx("__dscbv")#idx("DSCB")
#idx("OBTAIN macro")#idx("VTOC")

=== Format
```
#include <mvs/dscb.h>

int __dscbdv(const char dsn[44], const char vol[6], DSCB *dscb);
int __dscbav(const char cchhr[5], const char vol[6], DSCB *dscb);
int __dscbv(const char vol[6], DSCB *dscb);
```

=== Description
These functions read a data set control block (DSCB) from the volume table
of contents of the disk volume #var("vol"), with the OBTAIN macro (SVC 27).
The volume must be mounted. #var("dsn") and #var("vol") are strings\; the
functions pad them with blanks to 44 and 6 characters.

#deflist(width: 1.1in,
  [#cmd("__dscbdv()")], [reads the format-1 DSCB of the data set
    #var("dsn") (OBTAIN SEARCH). The result is the 96-byte data portion of
    the DSCB, laid out as #cmd("dscb->dscb1").],
  [#cmd("__dscbav()")], [reads the DSCB at the disk address #var("cchhr")
    (OBTAIN SEEK), for example the format-3 DSCB that #cmd("dscb1.ptrds")
    points to. The result is the whole DSCB of 140 bytes, key and data,
    laid out as #cmd("dscb->dscb3") for a format-3 DSCB. See the notes
    before you use it.],
  [#cmd("__dscbv()")], [reads the format-4 DSCB, which describes the VTOC
    and the device (OBTAIN SEARCH for the name of 44 bytes X'04'). The
    result is the 96-byte data portion: see the notes.],
)

#var("dscb") must point to a #cmd("DSCB") union, which provides the 140
bytes of work area that OBTAIN needs.

@mvs-datasets-dscb1-tab lists the fields of the format-1 DSCB. The flag
values of each field are defined in #cmd("<mvs/dscb.h>") next to it.

#tab(caption: [DSCB1 fields (format-1 DSCB, data portion)])[
  #table(columns: (0.75in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("fmtid")], [X'00'], [Format identifier, #cmd("'1'") (X'F1').],
    [#cmd("dssn")], [X'01'], [Serial number of the volume.],
    [#cmd("volsq")], [X'07'], [Volume sequence number.],
    [#cmd("credt")], [X'09'], [Creation date: a binary year since 1900, then
      a binary halfword day of the year.],
    [#cmd("expdt")], [X'0C'], [Expiration date, in the same form.],
    [#cmd("noepv")], [X'0F'], [Number of extents on the volume.],
    [#cmd("nobdb")], [X'10'], [Bytes used in the last directory block.],
    [#cmd("syscd")], [X'12'], [System code.],
    [#cmd("refd")], [X'1F'], [Date of last reference, in the form of
      #cmd("credt").],
    [#cmd("dsorg1"), #cmd("dsorg2")], [X'26'], [Data set organization:
      #cmd("DSGPS") (X'40') sequential, #cmd("DSGPO") (X'02') partitioned,
      #cmd("DSGDA") (X'20') direct, #cmd("DSGIS") (X'80') indexed
      sequential\; in #cmd("dsorg2"), #cmd("ORGAM") (X'08') VSAM.],
    [#cmd("recfm")], [X'28'], [Record format: #cmd("RECFF") (X'80') F,
      #cmd("RECFV") (X'40') V, #cmd("RECFU") (X'C0') U, with
      #cmd("RECFB") (X'10') blocked, #cmd("RECFS") (X'08') standard or
      spanned, #cmd("RECFA") (X'04') ANSI and #cmd("RECMC") (X'02') machine
      control characters.],
    [#cmd("optcd")], [X'29'], [Option codes.],
    [#cmd("blksz")], [X'2A'], [Block size.],
    [#cmd("lrecl")], [X'2C'], [Logical record length.],
    [#cmd("keyl")], [X'2E'], [Key length.],
    [#cmd("rkp")], [X'2F'], [Relative key position.],
    [#cmd("dsind")], [X'31'], [Data set indicators.],
    [#cmd("scal1")], [X'32'], [Secondary allocation flags: #cmd("CYL")
      (X'C0'), #cmd("TRK") (X'80'), #cmd("AVR") (X'40'), and
      #cmd("CONTG"), #cmd("MXIG"), #cmd("ALX"), #cmd("ROUND").],
    [#cmd("scal3")], [X'33'], [Secondary allocation quantity, 3 bytes.],
    [#cmd("lstar")], [X'36'], [Last used track and block (TTR).],
    [#cmd("trbal")], [X'39'], [Bytes remaining on the last track.],
    [#cmd("extent")], [X'3D'], [The first three extents, ten bytes each
      (@mvs-datasets-extent-tab).],
    [#cmd("ptrds")], [X'5B'], [Disk address (CCHHR) of a format-3 DSCB with
      further extents, or zeros.],
  )
] <mvs-datasets-dscb1-tab>

#tab(caption: [EXTENT fields])[
  #table(columns: (0.75in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("flag")], [X'00'], [Extent type: #cmd("EXTDAT") (X'01') data,
      #cmd("EXTCYL") (X'81') on cylinder boundaries, #cmd("EXTLAB")
      (X'40') user labels.],
    [#cmd("seq")], [X'01'], [Extent sequence number.],
    [#cmd("lower")], [X'02'], [First track of the extent, CCHH.],
    [#cmd("upper")], [X'06'], [Last track of the extent, CCHH.],
  )
] <mvs-datasets-extent-tab>

The format-3 DSCB (#cmd("DSCB3"), 140 bytes) holds four extents in its key
(#cmd("extent4")), its identifier #cmd("fmtid") (X'F3') at X'2C', nine more
extents (#cmd("extent9")), and at X'87' the address of the next format-3
DSCB (#cmd("ptrds")).

=== Returns
The return code of OBTAIN:

#deflist(width: 0.6in,
  [0], [The DSCB was read.],
  [4], [The volume is not mounted.],
  [8], [The DSCB was not found in the VTOC.],
  [12], [A permanent I/O error, or an invalid DSCB.],
  [16], [The work area address is not valid.],
)

=== Notes
- *#cmd("__dscbv()") and #cmd("DSCB4") do not match.* The #cmd("dscb4")
  member of the union describes the whole DSCB, beginning with its 44-byte
  key, but #cmd("__dscbv()") returns the data portion, which begins with
  #cmd("fmtid"). A field of #cmd("dscb4") is therefore found 44 bytes
  before its structure offset, in #cmd("dscb.work"): the number of tracks
  in a cylinder, #cmd("dstrk") at structure offset 64, is the halfword at
  #cmd("dscb.work[20]"). Moreover #cmd("DSCB4") is 146 bytes long, six more
  than a DSCB, so not every field in it can be at its true offset. The
  library itself reads #cmd("dstrk") only.
- *#cmd("__dscbav()") does not work for an address that contains a byte of
  zeros.* It takes the length of #var("cchhr") with #cmd("strlen()"), so
  the address is cut at the first X'00' and padded with blanks. Every
  address on a cylinder below 256 begins with X'00'.
- A format-1 DSCB describes only the part of a data set on one volume.
  #cmd("noepv") counts the extents on that volume.
- #cmd("sizeof(DSCB)") is 146, because of #cmd("DSCB4")\; #cmd("DSCB1") is
  96 bytes long and #cmd("DSCB3") 140.

=== Example
```
DSCB    dscb;
DSCB1   *f1 = &dscb.dscb1;

if (__dscbdv("SYS1.MACLIB", "MVSRES", &dscb) == 0)
    printf("blksize %u, %u extent(s)\n", f1->blksz, f1->noepv);
```

=== Related
@mvs-datasets-locate, @mvs-datasets-listds, @mvs-datasets-trkcalc

== trkcalc <mvs-datasets-trkcalc>
#idx("trkcalc")#idx("TRKCALC macro")#idx("track capacity")

=== Format
```
#include <mvs/dasd.h>

int trkcalc(DEVTYPE devtype, int keylen, int blksize);
```

=== Description
#cmd("trkcalc()") returns the number of blocks of #var("blksize") bytes,
each with a key of #var("keylen") bytes, that fit on one track of a device
of type #var("devtype"). It uses the TRKCALC macro (function TRKCAP).
#var("devtype") is one of the values of @mvs-datasets-devtype-tab. Only its
low-order four bits are used, so the device code from the last byte of the
device type in a unit control block may be passed as it is.

#tab(caption: [DEVTYPE values])[
  #table(columns: (1.1in, 0.6in, 1fr),
    [Name], [Value], [Device],
    [#cmd("DEV2311")], [X'01'], [2311],
    [#cmd("DEV2301")], [X'02'], [2301],
    [#cmd("DEV2303")], [X'03'], [2303],
    [#cmd("DEV2302")], [X'04'], [2302],
    [#cmd("DEV2321")], [X'05'], [2321],
    [#cmd("DEV23051")], [X'06'], [2305 model 1],
    [#cmd("DEV23052")], [X'07'], [2305 model 2],
    [#cmd("DEV2314")], [X'08'], [2314],
    [#cmd("DEV3330")], [X'09'], [3330],
    [#cmd("DEV3340")], [X'0A'], [3340],
    [#cmd("DEV3350")], [X'0B'], [3350],
    [#cmd("DEB3375")], [X'0C'], [3375. The name is spelled with #cmd("DEB")
      in the header.],
    [#cmd("DEV333011")], [X'0D'], [3330 model 11],
    [#cmd("DEV3380")], [X'0E'], [3380],
    [#cmd("DEV3390")], [X'0F'], [3390],
  )
] <mvs-datasets-devtype-tab>

=== Returns
The number of blocks, or 0 when TRKCALC fails, for example for a device
type it does not know.

=== Example
```
int n = trkcalc(DEV3350, 0, 6160);    /* blocks of 6160 on a 3350 track */
```

=== Related
@mvs-datasets-dscbdv, @mvs-osio

== \_\_listc <mvs-datasets-listc>
#idx("__listc")#idx("LISTCAT")#idx("IDCAMS", "LISTCAT")

=== Format
```
#include <mvs/dslist.h>

int __listc(const char *level, const char *option,
            int (*prt)(void *udata, const char *fmt, ...),
            void *udata);
```

=== Description
#cmd("__listc()") runs the IDCAMS command

```
LISTC LEVEL('level') OUTFILE(dd) option
```

and passes each line of its output, in turn, to #var("prt"). #var("level")
is put in quotes, so it is taken as given, without a TSO prefix.
#var("option") is #cmd("NULL") or a string of further LISTCAT keywords, for
example #cmd("\"NONVSAM VOLUME\"") or #cmd("\"ALL\"").

The output goes to a temporary data set, which the function allocates,
reads back, and deletes again. For each record it calls

```
prt(udata, "%s", line);
```

where #var("line") is the record as a string, its first character the
carriage control character (#cmd("'1'") for a new page). The shape of the
call lets a #cmd("printf()")-like function be passed directly.
#var("udata") is passed through, and the return value of #var("prt") is
ignored.

=== Returns
0 when the output was read, -1 when the temporary data set could not be
allocated or read back.

=== Notes
- The return code of IDCAMS is not returned, contrary to the comment in the
  header: a level that does not exist and a level that IDCAMS could not list
  both return 0. The listing says which.
- Calls of #cmd("__listc()") in one address space are serialized, so tasks
  that list at the same time wait for one another.
- When the temporary data set cannot be allocated or read back, a message
  is written to the console.

=== Related
@mvs-datasets-listds, @mvs-dynalloc-idcams

== \_\_listds, \_\_freeds <mvs-datasets-listds>
#idx("__listds")#idx("__freeds")#idx("DSLIST")#idx("catalog", "listing")

=== Format
```
#include <mvs/dslist.h>

DSLIST **__listds(const char *level, const char *option,
                  const char *filter);
void __freeds(DSLIST ***dslist);
```

=== Description
#cmd("__listds()") returns a list of the data sets cataloged under
#var("level"), one #cmd("DSLIST") record for each data set that has a
volume. It runs #cmd("__listc()") with #var("level") and #var("option"),
reads the entry names and volume serial numbers from the listing, and then
reads the format-1 DSCB of each data set with #cmd("__dscbdv()") to fill in
its attributes.

#var("option") is passed to LISTCAT, and must make LISTCAT print the volume
of each entry: #cmd("\"VOLUME\""), #cmd("\"ALL\""), or one of them with an
entry type, such as #cmd("\"NONVSAM VOLUME\""). An entry for which no volume
appears is left out. #var("filter") is #cmd("NULL") for all data sets, or a
pattern that each data set name must match, in the form of
#cmd("__patmat()"): #cmd("*") stands for any characters and #cmd("?") for
one.

The entries listed are non-VSAM data sets, VSAM clusters, page spaces and
user catalogs. A cluster takes the volume of its data component.

#cmd("__freeds()") frees a list returned by #cmd("__listds()"), including
the catalog names, and sets #cmd("*dslist") to #cmd("NULL").

#tab(caption: [DSLIST fields])[
  #table(columns: (0.85in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("dsn")], [X'00'], [The data set name, a string.],
    [#cmd("volser")], [X'2D'], [The first volume, a string.],
    [#cmd("dsorg")], [X'34'], [The organization: #cmd("\"PS\""),
      #cmd("\"PO\""), #cmd("\"DA\""), #cmd("\"IS\"") or #cmd("\"VS\"").],
    [#cmd("recfm")], [X'39'], [The record format: #cmd("F"), #cmd("V") or
      #cmd("U"), followed by #cmd("B"), #cmd("S"), #cmd("A") and #cmd("M")
      as they apply, for example #cmd("\"FB\"") or #cmd("\"VBA\"").],
    [#cmd("extents")], [X'3E'], [Number of extents on the volume.],
    [#cmd("lrecl")], [X'40'], [Logical record length.],
    [#cmd("blksize")], [X'42'], [Block size.],
    [#cmd("cryear"), #cmd("crjday"), #cmd("crmon"), #cmd("crday")], [X'44'],
      [Creation date: year (1980 to 2079), day of the year, month (1 to
      12), day of the month.],
    [#cmd("rfyear"), #cmd("rfjday"), #cmd("rfmon"), #cmd("rfday")], [X'4A'],
      [Date of last reference, in the same form.],
    [#cmd("spacu")], [X'50'], [Unit of the secondary allocation:
      #cmd("'C'") cylinders, #cmd("'T'") tracks or blocks.],
    [#cmd("scal1")], [X'51'], [The secondary allocation flags of the DSCB.],
    [#cmd("secondary")], [X'52'], [Secondary allocation quantity.],
    [#cmd("used_trks")], [X'54'], [Tracks used, from the last used track.],
    [#cmd("alloc_trks")], [X'56'], [Tracks allocated in the first three
      extents.],
    [#cmd("dev")], [X'58'], [The device type, #cmd("\"3350\""),
      #cmd("\"3375\""), #cmd("\"3380\"") or #cmd("\"3390\""), judged by the
      number of tracks in a cylinder.],
    [#cmd("disp")], [X'5D'], [The disposition. Filled in by
      #cmd("__listal()") only.],
    [#cmd("catnm")], [X'64'], [The catalog in which the entry was found, for
      example #cmd("\"SYS1.VSAM.MASTER.CATALOG\""), or #cmd("NULL") when not
      known. The string belongs to the list and is shared by the records of
      one catalog: copy it to keep it after #cmd("__freeds()").],
  )
] <mvs-datasets-dslist-tab>

=== Returns
The list, or #cmd("NULL") for an empty result or a failure
(@mvs-datasets-lists).

=== Errors
#deflist(width: 1.1in,
  [#cmd("ENOMEM")], [Storage ran out.],
)

=== Notes
- When the DSCB of a data set cannot be read, for example because its
  volume is not mounted, the record is still returned, with #cmd("dsn"),
  #cmd("volser") and #cmd("catnm") filled in and the other fields zero.
- For a data set that the catalog shows on the volume #cmd("******"), the
  system residence volume, the real volume is found with
  #cmd("__locate()").
- #cmd("alloc_trks") counts the extents in the format-1 DSCB only. A data
  set with more than three extents on the volume shows fewer tracks than it
  has. A device with a number of tracks per cylinder that the function does
  not know is shown as #cmd("\"3390\"").
- #cmd("secondary") is 16 bits wide. A larger secondary quantity is cut.
- The DSCB of every data set is read, so a level with many data sets costs
  one OBTAIN for each.

=== Example
#fig(caption: [Listing the data sets of a catalog level])[
  #code(read("../ex/mvs-datasets/listds.c"), numbers: true)
] <mvs-datasets-listds-ex>

=== Related
@mvs-datasets-listc, @mvs-datasets-dscbdv, @mvs-datasets-listal

== \_\_walkpd <mvs-datasets-walkpd>
#idx("__walkpd")#idx("PDS", "directory")#idx("PDSLIST")

=== Format
```
#include <mvs/dslist.h>

typedef int (*PDS_WALK)(void *arg, const PDSLIST *entry);

int __walkpd(const char *dataset, const char *filter, PDS_WALK fn, void *arg);
```

=== Description
#cmd("__walkpd()") reads the directory of the partitioned data set
#var("dataset") and calls

```
fn(arg, entry)
```

for each member, in directory order, which is the order of the names.
#var("dataset") is any name that #cmd("fopen()") accepts for a data set,
such as #cmd("\"SYS1.MACLIB\"") or #cmd("\"dd:syslib\"").
#var("filter") is #cmd("NULL") for every member, or a pattern in the form of
#cmd("__patmat()") that the member name must match, for example
#cmd("\"IEF*\""). #var("arg") is passed to #var("fn") untouched.

#var("entry") is the directory entry as MVS stores it
(@mvs-datasets-pdslist-tab). It points into a buffer that the next directory
block overwrites: copy what you keep. #var("fn") returns 0 to go on, and any
other value to stop the walk\; the entry at which it stopped counts as
delivered.

The function allocates no storage for the entries, so a directory of any
size costs the same: one buffer of a 256-byte directory block.

#tab(caption: [PDSLIST fields (a directory entry)])[
  #table(columns: (0.75in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("name")], [X'00'], [The member name, eight characters, padded
      with blanks, not terminated by a null character.],
    [#cmd("ttr")], [X'08'], [The relative address of the member (TTR).],
    [#cmd("idc")], [X'0B'], [#cmd("PDSLIST_IDC_ALIAS") (X'80'): the name is
      an alias. #cmd("PDSLIST_IDC_TTRS") (X'60'): the number of TTRs in the
      user data. #cmd("PDSLIST_IDC_UDATA") (X'1F'): the length of the user
      data in halfwords.],
    [#cmd("udata")], [X'0C'], [The user data, 0 to 62 bytes: ISPF
      statistics for a source library (#cmd("ISPFDATA")), or the attributes
      of a load module (#cmd("LOADDATA")).],
  )
] <mvs-datasets-pdslist-tab>

=== Returns
The number of entries passed to #var("fn"), 0 for none, or -1 when the
directory could not be opened or read.

=== Errors
#deflist(width: 1.1in,
  [#cmd("EINVAL")], [#var("dataset") or #var("fn") is #cmd("NULL").],
  [#cmd("EIO")], [The directory could not be read, or could not be opened
    and #cmd("fopen()") gave no reason, as for a data set that does not
    exist.],
  [other], [The value #cmd("fopen()") set when it could not open the
    data set.],
)

=== Notes
- An entry whose user data runs past the end of its directory block is not
  delivered, and the rest of that block is skipped. The walk goes on with
  the next block.
- Prefer #cmd("__walkpd()") to #cmd("__listpd()") for a large directory:
  #cmd("__listpd()") allocates one record for each member.

=== Example
#fig(caption: [Walking a PDS directory])[
  #code(read("../ex/mvs-datasets/walkpd.c"), numbers: true)
] <mvs-datasets-walkpd-ex>

=== Related
@mvs-datasets-listpd, @mvs-datasets-fmtisp, @mvs-datasets-bldl

== \_\_listpd, \_\_freepd <mvs-datasets-listpd>
#idx("__listpd")#idx("__freepd")

=== Format
```
#include <mvs/dslist.h>

PDSLIST **__listpd(const char *dataset, const char *filter);
void __freepd(PDSLIST ***pdslist);
```

=== Description
#cmd("__listpd()") returns a list of the directory entries of the
partitioned data set #var("dataset") whose names match #var("filter"), with
the arguments of #cmd("__walkpd()"). Each record is a copy of one entry
(@mvs-datasets-pdslist-tab), as long as the entry itself: 12 bytes and its
user data.

#cmd("__freepd()") frees the list and sets #cmd("*pdslist") to
#cmd("NULL").

=== Returns
The list, or #cmd("NULL") for an empty directory or a failure
(@mvs-datasets-lists).

=== Errors
#deflist(width: 1.1in,
  [#cmd("ENOMEM")], [Storage ran out.],
  [other], [As for #cmd("__walkpd()").],
)

=== Notes
Each member costs an allocation. A directory of many thousands of members
can exhaust the region: use #cmd("__walkpd()") for it.

=== Related
@mvs-datasets-walkpd, @mvs-datasets-fmtisp

== \_\_fmtisp, \_\_fmtloa <mvs-datasets-fmtisp>
#idx("__fmtisp")#idx("__fmtloa")#idx("ISPF statistics")
#idx("load module", "attributes")

=== Format
```
#include <mvs/dslist.h>

int __fmtisp(PDSLIST *pdslist, ISPFSTAT *ispfstat);
int __fmtloa(PDSLIST *pdslist, LOADSTAT *loadstat);
```

=== Description
These functions format the user data of a directory entry as strings, for
a member listing. #cmd("__fmtisp()") reads ISPF statistics, the user data
that ISPF and many editors keep for a member of a source library.
#cmd("__fmtloa()") reads the user data of a load module in a load library.
Both clear the output structure, then fill in the name and the TTR (six
hexadecimal digits).

#tab(caption: [ISPFSTAT fields])[
  #table(columns: (0.75in, 1fr),
    [Field], [Contents],
    [#cmd("name")], [The member name.],
    [#cmd("ttr")], [The TTR, #cmd("\"ttttrr\"").],
    [#cmd("ver")], [Version and modification level, #cmd("\"vv.mm\"").],
    [#cmd("created")], [Creation date, #cmd("\"yy-mm-dd\"").],
    [#cmd("changed")], [Date and time of the last change,
      #cmd("\"yy-mm-dd hh:mm:ss\"").],
    [#cmd("init")], [Initial number of lines.],
    [#cmd("size")], [Current number of lines.],
    [#cmd("mod")], [Number of modified lines.],
    [#cmd("userid")], [The user who changed the member last.],
  )
] <mvs-datasets-ispfstat-tab>

#tab(caption: [LOADSTAT fields])[
  #table(columns: (0.75in, 1fr),
    [Field], [Contents],
    [#cmd("name")], [The member name.],
    [#cmd("ttr")], [The TTR, #cmd("\"ttttrr\"").],
    [#cmd("size")], [Storage the module needs, six hexadecimal digits.],
    [#cmd("aliasof")], [For an alias, the name of the main member.],
    [#cmd("ac")], [The authorization code, two hexadecimal digits.],
    [#cmd("ep")], [The entry point address, six hexadecimal digits.],
    [#cmd("attr")], [The attributes, each as two letters or two blanks at
      a fixed place: #cmd("NE") not executable, #cmd("OL") only loadable,
      #cmd("PG") page aligned, #cmd("RF") refreshable, #cmd("RN")
      reentrant, #cmd("RU") reusable, #cmd("OV") overlay, #cmd("TS")
      test.],
    [#cmd("ssi")], [The system status index, eight hexadecimal digits, when
      the module has one.],
  )
] <mvs-datasets-loadstat-tab>

=== Returns
0 when the structure was filled in, also when the entry has no user data\;
then only #cmd("name") and #cmd("ttr") are set. -1 when an argument is
#cmd("NULL").

=== Notes
- The functions do not check that the user data has the format they read.
  Call #cmd("__fmtisp()") for a source library and #cmd("__fmtloa()") for a
  load library.
- #cmd("created") and #cmd("changed") are left empty when the date in the
  statistics is not valid.

=== Related
@mvs-datasets-walkpd, @mvs-datasets-listpd

== \_\_listvl, \_\_freevl <mvs-datasets-listvl>
#idx("__listvl")#idx("__freevl")#idx("VOLLIST")#idx("volume", "list")
#idx("LSPACE")#idx("VATLST")

=== Format
```
#include <mvs/dslist.h>

VOLLIST **__listvl(const char *filter, int dolspace, const char *vatlst);
void __freevl(VOLLIST ***vollst);
```

=== Description
#cmd("__listvl()") returns a list of the disk volumes that are online, one
#cmd("VOLLIST") record for each, in the order of the unit control blocks
(UCB) of the system. A volume mounted on more than one unit is listed once.
2321 data cells are not listed.

#var("filter") is #cmd("NULL") or empty for all volumes, or a pattern in the
form of #cmd("__patmat()") that the volume serial number must match.

When #var("dolspace") is not 0, the free space of each volume is requested
with LSPACE (SVC 78) and filled into its record.

When #var("vatlst") is not #cmd("NULL"), the comment of each volume is read
from the volume attribute list (VATLSTxx). A name of up to eight characters
without a period or parenthesis names a member of #cmd("SYS1.PARMLIB"), for
example #cmd("\"VATLST00\"")\; anything else is taken as the name of the
data set and member, for example #cmd("\"SYS1.PARMLIB(VATLST00)\""). The
comment is columns 40 to 79 of the first VATLST line whose volume serial
number, which may be a pattern, matches the volume.

#cmd("__freevl()") frees the list, including the comments, and sets
#cmd("*vollst") to #cmd("NULL").

#tab(caption: [VOLLIST fields])[
  #table(columns: (0.95in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("volser")], [X'00'], [The volume serial number, a string.],
    [#cmd("status")], [X'07'], [#cmd("VOLLIST_STATUS_ONLI") (X'80') online,
      #cmd("VOLLIST_STATUS_RESV") (X'40') reserved,
      #cmd("VOLLIST_STATUS_PRES") (X'20') permanently resident,
      #cmd("VOLLIST_STATUS_PRV") (X'08') private,
      #cmd("VOLLIST_STATUS_PUB") (X'04') public,
      #cmd("VOLLIST_STATUS_STG") (X'02') storage.],
    [#cmd("ucbdasd")], [X'08'], [The address of the UCB.],
    [#cmd("cuu")], [X'0C'], [The unit address.],
    [#cmd("dasdtype")], [X'0E'], [The device type as hexadecimal digits,
      for example #cmd("0x3350"), or 0 when not known.],
    [#cmd("freecyls")], [X'10'], [Free cylinders (LSPACE).],
    [#cmd("freetrks")], [X'14'], [Free tracks (LSPACE).],
    [#cmd("freeexts")], [X'18'], [Free extents (LSPACE).],
    [#cmd("maxfreecyls")], [X'1C'], [Cylinders of the largest free extent
      (LSPACE).],
    [#cmd("maxfreetrks")], [X'20'], [Tracks of the largest free extent
      (LSPACE).],
    [#cmd("comment")], [X'24'], [The VATLST comment, or #cmd("NULL").],
  )
] <mvs-datasets-vollist-tab>

=== Returns
The list, or #cmd("NULL") when no volume matched or storage ran out
(@mvs-datasets-lists).

=== Errors
#deflist(width: 1.1in,
  [#cmd("ENOMEM")], [Storage ran out.],
)

=== Notes
- When LSPACE fails for a volume, its free space fields stay 0 and a
  message with the return code is written to the console.
- When the VATLST cannot be opened, the list is returned without comments,
  and a message naming the data set is written to the console.

=== Related
@mvs-datasets-trkcalc, @mvs-datasets-listds

== \_\_listal, \_\_freeal <mvs-datasets-listal>
#idx("__listal")#idx("__freeal")#idx("ALCLIST")#idx("allocation", "list")

=== Format
```
#include <mvs/dslist.h>

ALCLIST **__listal(void *tcbptr, const char *ddname, unsigned opt);
void __freeal(ALCLIST ***alclist);
```

=== Description
#cmd("__listal()") returns a list of the DD statements allocated to the job
step of a task: one #cmd("ALCLIST") record for each DD, and in it one
#cmd("DSLIST") record for each data set of the DD, so that a concatenation
appears as one DD with several data sets. #var("tcbptr") is the TCB of the
task, or #cmd("NULL") for the calling task. #var("ddname") is #cmd("NULL")
for all DDs, or the name of one DD.

The data set name and the first volume are taken from the JFCB of each data
set. The other fields of the #cmd("DSLIST") record
(@mvs-datasets-dslist-tab) come from the JFCB as well, unless
#cmd("__LISTAL_OPT_DSCB") asks for the format-1 DSCB.

#var("opt") is 0, or the sum of values from @mvs-datasets-listal-opt-tab.
Each of the first four admits only the DDs whose first DSAB carries one of
the flags for it.

#tab(caption: [Options of #cmd("__listal()")])[
  #table(columns: (1.6in, 0.8in, 1fr),
    [Option], [Value], [Effect],
    [#cmd("__LISTAL_OPT_PERM")], [X'00000001'], [Only permanent (JCL)
      allocations.],
    [#cmd("__LISTAL_OPT_DYNM")], [X'00000002'], [Only dynamic allocations.],
    [#cmd("__LISTAL_OPT_CONCAT")], [X'00000004'], [Only concatenations.],
    [#cmd("__LISTAL_OPT_INUSE")], [X'00000008'], [Only allocations in use.],
    [#cmd("__LISTAL_OPT_NOCAT")], [X'00000010'], [Leave out the catalogs
      that the system allocated.],
    [#cmd("__LISTAL_OPT_NOJES")], [X'00000020'], [Leave out the data sets of
      the job entry subsystem, such as SYSOUT and SYSIN data sets.],
    [#cmd("__LISTAL_OPT_DSCB")], [X'00000100'], [Fill the #cmd("DSLIST")
      records from the format-1 DSCB rather than from the JFCB.],
  )
] <mvs-datasets-listal-opt-tab>

#tab(caption: [ALCLIST fields])[
  #table(columns: (0.75in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("ddname")], [X'00'], [The DD name, a string.],
    [#cmd("count")], [X'0B'], [The number of data sets of the DD.],
    [#cmd("dslist")], [X'0C'], [The data sets, a list of #cmd("count")
      #cmd("DSLIST") records.],
  )
] <mvs-datasets-alclist-tab>

#cmd("__freeal()") frees the list, with the #cmd("DSLIST") records of each
DD, and sets #cmd("*alclist") to #cmd("NULL").

=== Returns
The list, or #cmd("NULL") when no DD was selected or storage ran out
(@mvs-datasets-lists).

=== Errors
#deflist(width: 1.1in,
  [#cmd("ENOMEM")], [Storage ran out.],
)

=== Notes
- Filled from the JFCB, a record has no extents (#cmd("extents") is 0), and
  its date of last reference is today's date, because the JFCB has none.
  #cmd("disp") is #cmd("\"NEW\""), #cmd("\"OLD\""), #cmd("\"MOD\""),
  #cmd("\"SHR\"") or #cmd("\"???\"").
- Filled from the DSCB, a record has no disposition (#cmd("disp") is
  empty) and no space fields. When the DSCB cannot be read, for example for
  a SYSOUT data set, the record is filled from the JFCB.
- #cmd("catnm") is #cmd("NULL") in every record.

=== Related
@mvs-datasets-get-dsab, @mvs-datasets-listds

== \_\_bldl <mvs-datasets-bldl>
#idx("__bldl")#idx("BLDL macro")#idx("PDS", "directory search")

=== Format
```
#include <mvs/pds.h>

int __bldl(BLDL *bldl, void *dcb);
```

=== Description
#cmd("__bldl()") looks up members in the directory of a partitioned data set
with the BLDL macro. #var("bldl") is a BLDL list: a count of entries, the
length of each, and the entries, each beginning with a member name that the
caller fills in, padded with blanks. BLDL fills in the rest of each entry
that it finds. Several entries must be sorted by name.

#var("dcb") is an open DCB of the library, for example from
#cmd("osbdcb()") and #cmd("osbopen()") (see @mvs-osio). When #var("dcb") is
#cmd("NULL"), BLDL searches the libraries from which programs are fetched:
the job or step library and the link library.

#tab(caption: [BLDL list and entries])[
  #table(columns: (0.95in, 0.7in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("ff")], [X'00'], [The number of entries. 0 is taken as 1.],
    [#cmd("ll")], [X'02'], [The length of each entry: 12 (#cmd("DE12")),
      14 (#cmd("DE14")) or up to 76 (#cmd("DE76")). 0 is taken as 76.],
    [#cmd("name")], [entry + 0], [The member name, eight characters, padded
      with blanks.],
    [#cmd("ttr")], [entry + 8], [The TTR of the member.],
    [#cmd("k")], [entry + 11], [The concatenation number of the data set
      in which the member was found.],
    [#cmd("z")], [entry + 12], [Where it was found: #cmd("DE_PRIVATE") (0)
      a private library, #cmd("DE_LINK") (1) the link library,
      #cmd("DE_JOB") (2) a job, task or step library.],
    [#cmd("c")], [entry + 13], [#cmd("DE_ALIAS") (X'80') alias,
      #cmd("DE_TTRNS") (X'60') number of TTRs, #cmd("DE_UDATA") (X'1F')
      length of the user data in halfwords.],
    [#cmd("udata")], [entry + 14], [The user data, in a #cmd("DE76")
      entry.],
  )
] <mvs-datasets-bldl-tab>

=== Returns
The return code of BLDL: 0 when every member was found.

=== Example
```
BLDL bl = {0};

memcpy(bl.de76[0].name, "IEFBR14 ", 8);
if (__bldl(&bl, NULL) == 0)
    printf("found, z=%d\n", bl.de76[0].z);
```

=== Related
@mvs-datasets-walkpd, @mvs-datasets-stow

== \_\_stow <mvs-datasets-stow>
#idx("__stow")#idx("STOW")#idx("PDS", "directory update")

=== Format
```
#include <mvs/pds.h>

int __stow(void *dcb, void *area, int func);
```

=== Description
#cmd("__stow()") updates the directory of a partitioned data set with STOW
(SVC 21). #var("dcb") is a DCB open for output or update on the data set,
with #cmd("DSORG=PO"). #var("func") selects the action, in upper or lower
case, and the layout of #var("area"):

#deflist(width: 1.1in,
  [#cmd("'A'") add], [#var("area") is a directory entry: the name (8
    bytes), the TTR (3), a byte with the user data length in halfwords, and
    the user data.],
  [#cmd("'R'") replace], [#var("area") as for add.],
  [#cmd("'D'") delete], [#var("area") is the 8-byte member name.],
  [#cmd("'C'") change], [#var("area") is the old name and the new name,
    8 bytes each. The member keeps its TTR and its user data.],
)

Names are eight characters, in upper case, padded with blanks.

=== Returns
The return code of STOW, 0 on success. A change, for example, returns 4
when the new name exists already, and 8 when the old name is not found. -1
for an unknown #var("func").

=== Related
@mvs-datasets-renmem, @mvs-datasets-bldl

== \_\_renmem, \_\_delmem <mvs-datasets-renmem>
#idx("__renmem")#idx("__delmem")#idx("member", "renaming")
#idx("member", "deleting")

=== Format
```
#include <mvs/pds.h>

int __renmem(const char *dsn, const char *oldmem, const char *newmem);
int __delmem(const char *dsn, const char *mem);
```

=== Description
#cmd("__renmem()") renames the member #var("oldmem") of the partitioned data
set #var("dsn") to #var("newmem"). #cmd("__delmem()") deletes the member
#var("mem"). Both allocate the data set with #cmd("DISP=SHR"), open it for
output, update the directory with STOW, then close and free it again.

The member names are converted to upper case and end at the first blank or
after eight characters. #var("dsn") is passed to dynamic allocation as
#cmd("DSN=")#var("dsn").

A rename keeps the member's data, its TTR and its user data, such as ISPF
statistics, so it works for members of every record format, load modules
included. A delete removes the directory entry only\; the space of the
member is reclaimed when the data set is compressed.

=== Returns
#deflist(width: 0.6in,
  [0], [Success.],
  [4], [The new name exists already (#cmd("__renmem()")).],
  [8], [The member was not found.],
  [12], [A permanent I/O error.],
  [16], [No space is left in the directory (#cmd("__renmem()")).],
  [20], [Not enough virtual storage.],
  [-1], [An argument is #cmd("NULL"), a member name is empty, or storage
    ran out.],
  [-2], [The data set could not be allocated: it does not exist, or is in
    use.],
  [-3], [The data set could not be opened for output.],
)

The positive values are the return codes of STOW.

=== Notes
- #cmd("DISP=SHR") lets other users go on reading the library while the
  directory changes. Two programs that update the same directory at the
  same time are not serialized against each other\; that is the task of
  the programs.
- #cmd("remove()") of a name with a member, such as
  #cmd("\"A.B(MEM)\""), deletes the member through #cmd("__delmem()").

=== Related
@mvs-datasets-stow, @mvs-dynalloc

== \_\_fabandon <mvs-datasets-fabandon>
#idx("__fabandon")#idx("x37 abend", "closing after")#idx("D37 abend")

=== Format
```
#include <mvs/file.h>

int __fabandon(FILE *fp);
```

=== Description
#cmd("__fabandon()") closes the stream #var("fp") without writing what is
still in its buffer, and frees it, with the DD statement that
#cmd("fopen()") allocated for it.

It is meant for one situation. When a write runs out of space and MVS ends
it with an x37 abend (B37, D37 or E37), a program that recovers from the
abend with its own ESTAE is left with a stream that looks healthy, because
to the library an abend is not an I/O error. #cmd("fclose()") would flush
the buffer, drive the failing write again, and abend inside the close. That
leaves the DD allocated to the end of the job step, so the data set can be
deleted neither by the program nor by its user.

#cmd("__fabandon()") discards the buffer instead, marks the DCB as having
nothing to write, and closes it under an ESTAE of its own, so that a close
that fails anyway is returned rather than propagated. The data set can then
be deleted.

=== Returns
#deflist(width: 0.6in,
  [0], [Nothing was written, the close completed, and the DD was freed.],
  [\> 0], [The close abended. The value is the completion code as
    #cmd("0x00sssuuu"), with the system code in #cmd("sss") and the user
    code in #cmd("uuu"): a D37 is #cmd("0x00D37000"). The stream is freed,
    but the DD is not: the failed close still holds it.],
  [-1], [#var("fp") is #cmd("NULL") or not a stream.],
  [-2], [The close completed, but the DD could not be freed.],
  [-3], [The ESTAE could not be established, and the close was not
    attempted. The stream is unchanged and may be passed again.],
)

=== Notes
- After any return except -1 and -3, #var("fp") no longer exists.
- Data written to the buffer after the last block went out is lost.
- A write that runs out of space without an abend, which the library
  reports as an error with #cmd("errno") #cmd("ENOSPC"), leaves a stream
  that #cmd("fclose()") closes normally. #cmd("__fabandon()") is needed only
  when a write did abend.

=== Related
@mvs-datasets-ropen

== ropen, rclose, rread, rwrite <mvs-datasets-ropen>
#idx("ropen")#idx("rclose")#idx("rread")#idx("rwrite")#idx("RFILE")
#idx("record input and output")

=== Format
```
#include <mvs/rfile.h>

int ropen(const char *fnm, int write, RFILE **fp);
int rclose(RFILE *fp);
int rread(RFILE *fp, void *ptr, size_t *read);
int rwrite(RFILE *fp, const void *ptr, size_t size);
```

=== Description
These functions read or write a sequential data set, or a member of a
partitioned data set, one logical record at a time through BSAM. A record
is passed as it is stored: no newline character is added or removed, and
no character is translated.

#cmd("ropen()") opens #var("fnm") for input when #var("write") is 0, and
for output otherwise, and stores a handle in #cmd("*fp"). #var("fnm") is
one of:

#deflist(width: 1.6in,
  [#cmd("dd:")#var("ddname")], [the data set of an existing DD.],
  [#cmd("dd:")#var("ddname")#cmd("(")#var("member")#cmd(")")], [a member of
    the library of an existing DD.],
  [#var("name")], [a data set. When the program runs as a TSO command
    processor, the user's prefix is put in front of the name\; in batch,
    and under TSO by #cmd("CALL"), it is not.],
  [#cmd("'")#var("name")#cmd("'")], [a data set, fully qualified.],
  [#var("name")#cmd("(")#var("member")#cmd(")"),
    #cmd("'")#var("name")#cmd("(")#var("member")#cmd(")'")], [a member of a
    library.],
)

Names are converted to upper case. For a data set name, #cmd("ropen()")
allocates a DD with dynamic allocation, and #cmd("rclose()") frees it again.
The record format, the record length and the block size are those of the
data set.

#cmd("rread()") reads the next record into #var("ptr"), which must hold
#cmd("lrecl") bytes, and stores its length in #cmd("*read") unless
#var("read") is #cmd("NULL"). A RECFM=V record comes with its record
descriptor word (RDW): bytes 0 and 1 hold the length of the record including
the RDW, bytes 2 and 3 are zero, and the data follows. The length stored
counts the RDW.

#cmd("rwrite()") writes one record of #var("size") bytes from #var("ptr"),
or of #cmd("lrecl") bytes when #var("size") is 0. A RECFM=F record shorter
than #cmd("lrecl") is padded with blanks. A RECFM=V record must carry its
RDW, as #cmd("rread()") returns it: bytes 0 and 1 equal to #var("size"),
bytes 2 and 3 zero. So a record that was read can be written unchanged to a
data set of the same format.

#cmd("rclose()") writes the last block, closes the data set, frees a DD that
#cmd("ropen()") allocated, and frees the handle.

#tab(caption: [RFILE fields])[
  #table(columns: (0.75in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("eye")], [X'00'], [#cmd("\"*RFILE*\""), for dumps.],
    [#cmd("dyn")], [X'08'], [Not 0 when #cmd("ropen()") allocated the DD.],
    [#cmd("recfm")], [X'0C'], [#cmd("RFILE_RECFM_F") (0),
      #cmd("RFILE_RECFM_V") (1) or #cmd("RFILE_RECFM_U") (2).],
    [#cmd("lrecl")], [X'10'], [The logical record length.],
    [#cmd("write")], [X'14'], [Not 0 when open for output.],
    [#cmd("hfile"), #cmd("asmbuf")], [X'18'], [Private to the library.],
    [#cmd("ddname")], [X'20'], [The DD name, a string.],
    [#cmd("member")], [X'29'], [The member name, a string, or empty.],
  )
] <mvs-datasets-rfile-tab>

=== Returns
#cmd("ropen()"): 0 when the data set is open, 1 otherwise.
#cmd("rread()"): 0 when a record was read, 1 at the end of the data or on
an error. #cmd("rwrite()"): 0 when the record was written, 1 otherwise.
#cmd("rclose()"): 0, or -1 when the last block could not be written or the
DD could not be freed\; the handle is freed in either case.

=== Errors
#deflist(width: 1.1in,
  [#cmd("EINVAL")], [#cmd("rwrite()"): the record is longer than
    #cmd("lrecl")\; or, for RECFM=V, shorter than 4 bytes, or its RDW does
    not match #var("size"). Nothing was written.],
  [#cmd("ENOSPC")], [#cmd("rwrite()"), #cmd("rclose()"): the data set is
    out of space.],
  [#cmd("EIO")], [#cmd("rwrite()"), #cmd("rclose()"): an I/O error\; or
    #cmd("rclose()") could not free the DD.],
  [other], [#cmd("ropen()"): the reason the data set could not be
    allocated or opened.],
)

=== Notes
- #cmd("rread()") does not tell the end of the data from an error.
- On a spanned data set (RECFM=VS or VBS) a record can be at most
  #cmd("lrecl") - 4 bytes long.
- A handle has no lock. Do not use one handle in two tasks at the same
  time.

=== Example
#fig(caption: [Copying a data set record by record])[
  #code(read("../ex/mvs-datasets/rcopy.c"), numbers: true)
] <mvs-datasets-rcopy-ex>

=== Related
@mvs-osio, @mvs-datasets-fabandon
