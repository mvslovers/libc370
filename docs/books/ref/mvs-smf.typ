#import "../bookmaster/bookmaster.typ": *

= SMF Records <mvs-smf>

// Narrow cells: ragged right, no hyphenation of the names.
#show table: set par(justify: false)
#show table: set text(hyphenate: false)

#idx("SMF")
#idx("System Management Facilities")
The System Management Facilities (SMF) of MVS collect records about the
work of the system, such as jobs, steps and data sets, and write them to the
SMF data sets #cmd("SYS1.MANX") and #cmd("SYS1.MANY"). A program may write
records of its own, of the types 128 to 255 that are left to installations.
#cmd("<mvs/smf.h>") provides functions to build the header of such a record,
to find out whether SMF is recording, and to write the record with SVC 83.
It also maps the SMF control area (SMCA), in which a program can read the
SMF options of the system.

#idx("authorization", "SMF")
#idx("SVC 83")
#idx("APF authorization", "smf_write")
SVC 83 is restricted: MVS accepts it only from a program in supervisor
state or in key 0. #cmd("smf_write()") therefore switches to supervisor
state and key 0 with MODESET for the call, and back afterwards, unless the
program is in supervisor state already. MODESET in turn is available only
to an APF-authorized program: a program that calls #cmd("smf_write()") must
run authorized, that is, be link-edited with #cmd("AC=1") and fetched from
an APF-authorized library. #cmd("smf_init()"), #cmd("smf_active()"),
#cmd("__smca()") and #cmd("__smfid()") need no authorization.

On MVS 3.8j the SMF options of the system are set at IPL by the
#cmd("SMFPRMxx") member of #cmd("SYS1.PARMLIB"). A record is written only
when SMF is active and the options ask for user records.

== The SMF Record Header <mvs-smf-header>
#idx("SMF_HEADER")

An SMF record begins with an 18-byte header, described by
#cmd("SMF_HEADER"). The record itself is a variable-length record: its first
four bytes are a record descriptor word with the length of the whole record.
A program defines the record as a structure that begins with an
#cmd("SMF_HEADER") and continues with its own data.

#tab(caption: [SMF_HEADER fields])[
  #table(columns: (0.9in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("reclen")], [X'00'], [The length of the record, header included.],
    [#cmd("segdesc")], [X'02'], [The segment descriptor, 0.],
    [#cmd("sysiflags")], [X'04'], [System indicator flags.],
    [#cmd("rectype")], [X'05'], [The record type, 128 to 255 for a user
      record.],
    [#cmd("time")], [X'06'], [The time of day in hundredths of a second
      since midnight, a 4-byte binary number.],
    [#cmd("dtepref")], [X'0A'], [The century: 0 for the years 1900 to 1999, 1 for
      2000 to 2099.],
    [#cmd("date")], [X'0B'], [The date as packed decimal #var("yyddd"),
      with the sign in the last half-byte.],
    [#cmd("sysid")], [X'0E'], [The SMF system identifier, four
      characters.],
  )
] <mvs-smf-header-tab>

== smf\_init <mvs-smf-init>
#idx("smf_init")

=== Format
```
#include <mvs/smf.h>

void smf_init(void *record, unsigned short reclen, unsigned char rectype);
```

=== Description
#cmd("smf_init()") fills in the header of the SMF record at #var("record"):

- #cmd("reclen") is set to #var("reclen"), #cmd("segdesc") to 0,
  #cmd("rectype") to #var("rectype"), and #cmd("sysiflags") to X'02'.
- #cmd("time"), #cmd("dtepref") and #cmd("date") are set to the current
  local time and date.
- #cmd("sysid") is set to the SMF system identifier, when SMF is present.

The function does not touch the data that follows the header. Set the whole
record to zeros before the call, and fill in the data after it.

=== Notes
- The time has a resolution of one second: the hundredths are always 0.
- The sign half-byte of #cmd("date") is #cmd("X'C'"). The comment in the
  header says #cmd("F")\; both are valid signs of a positive packed
  decimal number.
- When SMF is not present, #cmd("sysid") is left as it was.

=== Related
@mvs-smf-write, @mvs-smf-smca

== smf\_active <mvs-smf-active>
#idx("smf_active")

=== Format
```
#include <mvs/smf.h>

int smf_active(void);
```

=== Description
#cmd("smf_active()") tells whether SMF recording was requested in the system:
it tests the flags #cmd("SMCAUSER") and #cmd("SMCAMAN") in the SMCA.

=== Returns
1 when either flag is set, 0 when neither is set or there is no SMCA.

=== Notes
A result of 1 does not promise that a record will be written. A user record
is written only when the options ask for user records, and an SMF data set
must be available.

=== Related
@mvs-smf-write, @mvs-smf-smca

== smf\_write <mvs-smf-write>
#idx("smf_write")

=== Format
```
#include <mvs/smf.h>

int smf_write(void *record);
```

=== Description
#cmd("smf_write()") writes the SMF record at #var("record") with SVC 83.
The record must begin with a complete header (@mvs-smf-header-tab), whose
#cmd("reclen") gives its length.

When #cmd("smf_active()") returns 0, nothing is done. Otherwise the function
switches to supervisor state and key 0 if necessary, issues SVC 83, and
switches back.

=== Returns
0 when SVC 83 accepted the record\; -1 when SMF is not active\; any other
value is the return code of SVC 83.

=== Notes
- The program must be APF-authorized, unless it runs in supervisor state
  already. An unauthorized program abends in MODESET.
- A program in problem state is switched to supervisor state and key 0
  for the duration of SVC 83 only\; one in supervisor state already is
  left as it is.

=== Example
#fig(caption: [Writing a user SMF record])[
  #code(read("../ex/mvs-smf/smfrec.c"), numbers: true)
] <mvs-smf-write-ex>

=== Related
@mvs-smf-init, @mvs-smf-active

== \_\_smca, \_\_smfid <mvs-smf-smca>
#idx("__smca")#idx("__smfid")#idx("SMCA")#idx("SMF system identifier")

=== Format
```
#include <mvs/smf.h>

SMCA *__smca(void);
const unsigned char *__smfid(void);
```

=== Description
#cmd("__smca()") returns the address of the SMF control area (SMCA), taken
from the field #cmd("CVTSMCA") of the communication vector table.
#cmd("__smfid()") returns the address of the SMF system identifier in the
SMCA, the field #cmd("smcasid").

@mvs-smf-smca-tab lists the fields of the SMCA that a program is most likely
to read. The structure #cmd("SMCA") maps the whole area, 180 bytes
(#cmd("SMCASIZE")), and the flag values are defined in the header next to
each field.

#tab(caption: [SMCA fields])[
  #table(columns: (0.9in, 0.6in, 1fr),
    [Field], [Offset], [Contents],
    [#cmd("smcaopt")], [X'00'], [The SMF options for batch work:
      #cmd("SMCAOPT1") (X'80') job accounting, #cmd("SMCAOPT2") (X'40')
      step accounting, #cmd("SMCAEXT") (X'20') user exits are taken,
      #cmd("SMCADSA") (X'10') data set accounting, #cmd("SMCAVOL") (X'08')
      volume accounting.],
    [#cmd("smcamisc")], [X'01'], [#cmd("SMCAUSER") (X'80') and
      #cmd("SMCAMAN") (X'40'): neither set, no recording
      (#cmd("MAN=NONE"))\; #cmd("SMCAMAN") only, user records only
      (#cmd("MAN=USER"))\; both, all records (#cmd("MAN=ALL")).],
    [#cmd("smcajwt")], [X'08'], [The job wait time limit, in timer units.],
    [#cmd("smcabuf")], [X'0C'], [The size of the SMF buffer.],
    [#cmd("smcasid")], [X'10'], [The SMF system identifier, four characters.],
    [#cmd("smcapdev")], [X'18'], [The volume of the SMF data set in use.],
    [#cmd("smcapsta")], [X'1E'], [The status of the SMF data set in use:
      #cmd("SMCAPNAV") (X'80') not available for recording.],
    [#cmd("smcaadev")], [X'28'], [The volume of the other SMF data set.],
    [#cmd("smcafopt")], [X'52'], [The SMF options for TSO work, as in
      #cmd("smcaopt").],
    [#cmd("smcaswa")], [X'68'], [#cmd("SMCADSTR") (X'40') both data sets are
      full, #cmd("SMCAOPFL") (X'20') an SMF data set could not be opened\;
      in either case SMF is not recording.],
    [#cmd("smcadsct")], [X'74'], [The number of records lost because no data
      set was available.],
    [#cmd("smcau83")], [X'A8'], [The address of the SMF output exit
      #cmd("IEFU83").],
  )
] <mvs-smf-smca-tab>

=== Returns
The address of the SMCA, or #cmd("NULL") when the system has none.
#cmd("__smfid()") returns #cmd("NULL") in the same case.

=== Notes
- The system identifier is four characters, *not* terminated by a null
  character. Print it with a precision:
  ```c
  const unsigned char *sid = __smfid();
  if (sid) printf("system %.4s\n", sid);
  ```
- The SMCA belongs to the system. Read it, never change it.

=== Related
@mvs-smf-active, @mvs-smf-init
