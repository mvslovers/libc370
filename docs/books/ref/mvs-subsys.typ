#import "../bookmaster/bookmaster.typ": *

= Subsystems, JES2 and ISPF <mvs-subsys>

#idx("subsystem interface")
#idx("JES2")
#idx("ISPF")
This chapter describes five headers that connect a program to the
subsystems of MVS:

#deflist(width: 1.35in,
  [#cmd("<mvs/subsys.h>")], [the subsystem control blocks: the subsystem
    communication vector table (SSCT), the subsystem vector table (SSVT) and
    the subsystem identification block (SSIB) of the job, and
    #cmd("iefssreq()"), which passes a request to the subsystem interface.],
  [#cmd("<mvs/jes2.h>")], [the JES2 job queue and its spool: a list of the
    jobs with their output data sets, the lines of an output data set,
    requests to cancel a job and to delete or requeue its output, and the
    internal reader.],
  [#cmd("<mvs/jes2ckpt.h>")], [the JES2 checkpoint data set, read
    directly.],
  [#cmd("<mvs/jes2spool.h>")], [the JES2 spool data set, read directly.],
  [#cmd("<mvs/ispf.h>")], [calls to ISPF dialog services from a TSO
    command processor.],
)

The header #cmd("<mvs/jes2.h>") includes #cmd("<mvs/jes2ckpt.h>"),
#cmd("<mvs/jes2spool.h>") and #cmd("<mvs/vsam.h>"), so a program that uses
JES2 needs only the one #cmd("#include").

== Authorization <mvs-subsys-auth>

#idx("authorization", "subsystem functions")
#idx("APF authorization", "subsystem functions")
The functions of this chapter fall into three groups, according to what
they require of the caller. @mvs-subsys-auth-tab lists them.

#tab(caption: [Authorization required by the subsystem functions])[
  #set par(justify: false)
  #table(columns: (2.2in, 1fr),
    [Function], [Requirement],
    [#cmd("ssct_new()"), #cmd("ssct_free()"), #cmd("ssct_install()"),
     #cmd("ssct_remove()"), #cmd("ssct_remove_by_name()"), #cmd("ssvt_new()"),
     #cmd("ssvt_free()"), #cmd("ssvt_set()"), #cmd("ssvt_reset()"),
     #cmd("ssvt_funcmap()")],
    [Supervisor state. The function switches to PSW key 0 itself while it
     changes the control block, and back to the caller's key afterwards.
     The control blocks are in common storage (CSA, subpool 241) and are
     changed for the whole system.],
    [#cmd("iefssreq()"), and through it #cmd("jescanj()"), #cmd("jesdelj()"),
     #cmd("jesreque()"), #cmd("jesxwrtr()"), #cmd("jesxdone()")],
    [APF authorization. #cmd("iefssreq()") issues #cmd("MODESET MODE=SUP")
     around the call of the subsystem interface, which abends in a program
     that is not authorized.],
    [All other functions],
    [None. #cmd("jesopen()"), #cmd("jesjob()") and #cmd("jesprint()") read the
     checkpoint and spool data sets with ordinary #cmd("OPEN") and
     #cmd("READ") and need only the DD statements described in
     @mvs-subsys-jes. The ISPF functions need a TSO environment.],
  )
] <mvs-subsys-auth-tab>

#note[The headers #cmd("<mvs/subsys.h>") and #cmd("<mvs/jes2.h>") include
mappings of IBM control blocks that do not compile cleanly under
#cmd("-Wall"): #cmd("<ibm/mvs/iefssobh.h>") and #cmd("<ibm/mvs/iefssso.h>")
contain #cmd("/*") inside a comment, and #cmd("<ibm/mvs/dcbd.h>") and
#cmd("<mvs/vsam.h>") use #cmd("#pragma pack"), which cc370 reports as
ignored. A program compiled with #cmd("-Wall -Werror") that includes either
header therefore fails to compile. Compile it without #cmd("-Werror"), or
add #cmd("-Wno-comment -Wno-unknown-pragmas").]

== The Subsystem Control Blocks <mvs-subsys-cb>

#idx("SSCT")
#idx("SSVT")
Every subsystem known to MVS is represented by a subsystem communication
vector table, an #cmd("SSCT"). The SSCTs form a chain that starts in the
JES control table (#cmd("JESCT"), field #cmd("JESSSCT")), which the CVT
addresses. Each SSCT carries the four-character subsystem name
(#cmd("ssctsnam")), the address of the next SSCT (#cmd("ssctscta")) and
the address of the subsystem's vector table, the #cmd("SSVT")
(#cmd("ssctssvt")). The SSVT holds a 256-byte function matrix
(#cmd("ssvtfcod")), indexed by the function code of a request minus one,
and after it the addresses of the routines that serve the functions
(#cmd("ssvtfrtn")). A function byte of 0 means that the subsystem does not
support the function; any other value #var("n") selects routine
#var("n").

The functions below create, install and remove an SSCT and its SSVT, which
is what a program does to make itself a subsystem. They change system-wide
control blocks in common storage: an SSCT that is installed and then freed
without being removed leaves a chain that every subsystem request in the
system follows.

== ssct\_find <mvs-subsys-ssct-find>

#idx("ssct_find")
=== Format

```
#include <mvs/subsys.h>

SSCT *ssct_find(const char *name);
```

=== Description

#cmd("ssct_find()") searches the SSCT chain for the subsystem named
#var("name") and returns its SSCT. The name is padded with blanks to four
characters and compared exactly; it is not changed to uppercase.

When #var("name") is #cmd("NULL"), an empty string or a string that begins
with a blank, the function returns the first SSCT of the chain.

=== Returns

The address of the SSCT, or #cmd("NULL") when no subsystem of that name is
installed.

=== Example

@mvs-subsys-ex-ssct lists the subsystems of the system.

#fig(caption: [Listing the installed subsystems])[
  #code(read("../ex/mvs-subsys/ssctlist.c"))
] <mvs-subsys-ex-ssct>

=== Related

@mvs-subsys-ssct-new, @mvs-subsys-ssct-install

== ssct\_new, ssct\_free <mvs-subsys-ssct-new>

#idx("ssct_new")
#idx("ssct_free")
=== Format

```
#include <mvs/subsys.h>

SSCT *ssct_new(const char *name, SSVT *ssvt, void *suse);
void  ssct_free(SSCT *ssct);
```

=== Description

#cmd("ssct_new()") obtains storage for an SSCT in subpool 241 (common
storage, key 0), clears it, and fills in the identifier #cmd("SSCT"), the
subsystem name #var("name") padded with blanks to four characters, the
vector table #var("ssvt") and the subsystem's own word #var("suse"). The
SSCT is not yet part of the chain; see @mvs-subsys-ssct-install.

#cmd("ssct_free()") releases the storage of an SSCT. #cmd("NULL") is
ignored.

=== Returns

#cmd("ssct_new()") returns the address of the new SSCT, or #cmd("NULL") when
the storage could not be obtained.

=== Notes

Both functions require supervisor state. Do not free an SSCT that is still
installed: remove it first with #cmd("ssct_remove()").

=== Related

@mvs-subsys-ssct-install, @mvs-subsys-ssct-remove, @mvs-subsys-ssvt-new

== ssct\_install <mvs-subsys-ssct-install>

#idx("ssct_install")
=== Format

```
#include <mvs/subsys.h>

int ssct_install(SSCT *ssct, const char *after_name);
```

=== Description

#cmd("ssct_install()") inserts #var("ssct") into the SSCT chain behind the
subsystem named #var("after_name"). When #var("after_name") is
#cmd("NULL"), the SSCT is placed behind the first subsystem of the chain,
usually the master subsystem or JES2. When no subsystem of that name exists,
it is placed at the end of the chain.

=== Returns

0.

=== Notes

Requires supervisor state. The function does not check whether
#var("ssct") is already in the chain or whether another subsystem of the
same name exists.

=== Related

@mvs-subsys-ssct-remove, @mvs-subsys-ssct-find

== ssct\_remove, ssct\_remove\_by\_name <mvs-subsys-ssct-remove>

#idx("ssct_remove")
#idx("ssct_remove_by_name")
=== Format

```
#include <mvs/subsys.h>

int ssct_remove(SSCT *ssct);
int ssct_remove_by_name(const char *name);
```

=== Description

#cmd("ssct_remove()") takes #var("ssct") out of the SSCT chain and sets its
#cmd("ssctscta") field to zero. #cmd("ssct_remove_by_name()") finds the SSCT
with #cmd("ssct_find()") and removes it. Neither function frees the
storage.

=== Returns

#deflist(width: 0.6in,
  [0], [The SSCT was removed.],
  [4], [The SSCT is not in the chain, or no subsystem has that name.],
)

=== Notes

Require supervisor state and key 0. The first SSCT of the chain cannot be
removed: the search looks for the SSCT that points to #var("ssct"), and
the first one has none. #cmd("ssct_remove_by_name(NULL)") therefore always
returns 4.

=== Related

@mvs-subsys-ssct-install, @mvs-subsys-ssct-new

== ssvt\_new, ssvt\_free <mvs-subsys-ssvt-new>

#idx("ssvt_new")
#idx("ssvt_free")
=== Format

```
#include <mvs/subsys.h>

SSVT *ssvt_new(unsigned funcmax);
void  ssvt_free(SSVT *ssvt);
```

=== Description

#cmd("ssvt_new()") obtains a cleared SSVT in subpool 241 with room for
#var("funcmax") routine addresses, and sets #cmd("ssvtfnum") to
#var("funcmax"). All function bytes are 0, so the new subsystem supports no
function until #cmd("ssvt_set()") and #cmd("ssvt_funcmap()") are called.

#cmd("ssvt_free()") releases an SSVT. #cmd("NULL") is ignored.

=== Returns

#cmd("ssvt_new()") returns the address of the SSVT, or #cmd("NULL") when
#var("funcmax") is greater than 256 or the storage could not be obtained.

=== Notes

Both functions require supervisor state.

=== Related

@mvs-subsys-ssvt-set, @mvs-subsys-ssvt-funcmap

== ssvt\_set, ssvt\_reset <mvs-subsys-ssvt-set>

#idx("ssvt_set")
#idx("ssvt_reset")
=== Format

```
#include <mvs/subsys.h>

int ssvt_set(SSVT *ssvt, unsigned index, void *func);
static __inline int ssvt_reset(SSVT *ssvt, unsigned index);
```

=== Description

#cmd("ssvt_set()") stores the routine address #var("func") in slot
#var("index") of the routine table #cmd("ssvtfrtn"). Slots are numbered
from 1. #cmd("ssvt_reset()") is an inline function in the header that calls
#cmd("ssvt_set()") with a #cmd("NULL") address.

=== Returns

#deflist(width: 0.9in,
  [0], [The slot was set.],
  [#cmd("EPERM")], [#var("ssvt") is #cmd("NULL"), or #var("index") is 0 or
    greater than 256.],
)

The error value is returned, not stored in #cmd("errno").

=== Notes

Requires supervisor state. The function checks #var("index") against 256,
not against the #var("funcmax") the SSVT was created with: keep the index
within #cmd("ssvtfnum") yourself.

=== Related

@mvs-subsys-ssvt-funcmap, @mvs-subsys-ssvt-new

== ssvt\_funcmap <mvs-subsys-ssvt-funcmap>

#idx("ssvt_funcmap")
=== Format

```
#include <mvs/subsys.h>

int ssvt_funcmap(SSVT *ssvt, unsigned index, unsigned funcnum);
```

=== Description

#cmd("ssvt_funcmap()") makes subsystem function #var("funcnum") (1 to 256)
call the routine in slot #var("index") of the SSVT, by setting the function
byte #var("funcnum") of the matrix to #var("index"). Several function
numbers may be mapped to the same slot. An #var("index") of 0 marks the
function as not supported.

=== Returns

#deflist(width: 0.9in,
  [0], [The function was mapped.],
  [#cmd("EPERM")], [#var("ssvt") is #cmd("NULL"), #var("index") is
    greater than 256, or #var("funcnum") is 0 or greater than 256.],
)

=== Notes

Requires supervisor state.

=== Related

@mvs-subsys-ssvt-set

== \_\_ssib, \_\_jobid <mvs-subsys-ssib>

#idx("__ssib")
#idx("__jobid")
#idx("SSIB")
#idx("job identifier")
=== Format

```
#include <mvs/subsys.h>

SSIB       *__ssib(void);
const char *__jobid(void);
```

=== Description

#cmd("__ssib()") returns the subsystem identification block of the job the
program runs in, found through the current TCB, its JSCB and the active
JSCB. #cmd("__jobid()") returns the address of the job identifier in that
SSIB (#cmd("SSIBJBID")), for example #cmd("JOB01234") or
#cmd("STC00042").

=== Returns

The address of the SSIB, or of its job identifier.

=== Notes

The job identifier is eight characters and is not followed by a null
character: print it with #cmd("%.8s"). The SSIB itself is mapped by
#cmd("<ibm/mvs/iefjssib.h>").

== iefssreq <mvs-subsys-iefssreq>

#idx("iefssreq")
#idx("IEFSSREQ")
=== Format

```
#include <mvs/subsys.h>

int iefssreq(SSOB *ssob);
```

=== Description

#cmd("iefssreq()") passes the subsystem options block #var("ssob") to the
subsystem interface, as the #cmd("IEFSSREQ") macro does: it calls the
routine whose address is in the JES control table, in supervisor state,
with the high-order bit of the SSOB address set. The SSOB must be
complete: its header (see @mvs-subsys-initssob), the function code in
#cmd("SSOBFUNC"), the SSIB in #cmd("SSOBSSIB") or zero for the job's own
subsystem, and the function's extension in #cmd("SSOBINDV").

=== Returns

The return code of the subsystem interface, which says whether the request
reached a subsystem. The result of the function itself is in
#cmd("SSOBRETN").

#tab(caption: [Return codes of the subsystem interface])[
  #table(columns: (0.5in, 0.9in, 1fr),
    [Code], [Name], [Meaning],
    [0], [#cmd("SSRTOK")], [The request went to a subsystem.],
    [4], [#cmd("SSRTNSUP")], [The subsystem does not support the
      function.],
    [8], [#cmd("SSRTNTUP")], [The subsystem exists but is not active.],
    [12], [#cmd("SSRTNOSS")], [The subsystem does not exist.],
    [16], [#cmd("SSRTDIST")], [The function was not completed (disastrous
      error).],
    [20], [#cmd("SSRTLERR")], [Logical error: the SSOB is not valid, for
      example its length.],
  )
] <mvs-subsys-ssrt-tab>

=== Notes

Requires APF authorization. The names of the return codes are defined in
#cmd("<ibm/mvs/iefssobh.h>"), which maps the SSOB header.

=== Related

@mvs-subsys-initssob

== initssob <mvs-subsys-initssob>

#idx("initssob")
#idx("SSOB")
=== Format

```
#include <mvs/jes2.h>

int initssob(SSOB *ssob, void *ssobindv);
```

=== Description

#cmd("initssob()") clears the SSOB header #var("ssob"), sets its
identifier #cmd("SSOB") and its length, and points #cmd("SSOBINDV") at the
function extension #var("ssobindv"). The caller then sets
#cmd("SSOBFUNC") and fills in the extension.

=== Returns

0.

=== Notes

Declared in #cmd("<mvs/jes2.h>"), not in #cmd("<mvs/subsys.h>").

=== Related

@mvs-subsys-iefssreq

== Reading the JES2 Queue <mvs-subsys-jes>

#idx("JES2", "job queue")
#idx("HASPCKPT DD statement")
#idx("HASPACE1 DD statement")
#idx("spool")
#idx("checkpoint data set")
The functions #cmd("jesopen()"), #cmd("jesjob()") and #cmd("jesprint()")
read the JES2 job queue and spool without asking JES2: they open the JES2
checkpoint data set and the spool data set themselves and interpret their
records. Their caller needs no authorization, but the job step needs two DD
statements:

#deflist(width: 1.1in,
  [#cmd("HASPCKPT")], [the JES2 checkpoint data set, for example
    #cmd("SYS1.HASPCKPT").],
  [#cmd("HASPACE1")], [the JES2 spool data set, for example
    #cmd("SYS1.HASPACE").],
)

#fig(caption: [DD statements for the JES2 functions])[
```
//HASPCKPT DD DISP=SHR,DSN=SYS1.HASPCKPT
//HASPACE1 DD DISP=SHR,DSN=SYS1.HASPACE
```
] <mvs-subsys-jes-dd>

Only one spool volume is read: a system whose spool spans several volumes
is read as far as the first one goes.

The checkpoint is read once, when #cmd("jesopen()") opens it. Every later
#cmd("jesjob()") call works on that copy, so jobs submitted after the open
are not seen, and a job may have moved on since. To see the current state,
close the handle and open a new one. The copy is read without serializing
with JES2; a job that changes while it is read can appear in an
intermediate state.

A JES handle, #cmd("JES"), holds the open checkpoint and spool. A job is
described by a #cmd("JESJOB"), and each of its data sets by a
#cmd("JESDD"). @mvs-subsys-jesjob-tab and @mvs-subsys-jesdd-tab list the
fields a program reads.

#tab(caption: [Fields of JESJOB])[
  #table(columns: (1.15in, 1fr),
    [Field], [Contents],
    [#cmd("jobname")], [The job name, a string of up to 8 characters.],
    [#cmd("jobid")], [The job identifier: #cmd("JOB"), #cmd("TSU") or
      #cmd("STC") followed by five digits, for example #cmd("JOB01234").
      #cmd("???") replaces the prefix when the job type is not known.],
    [#cmd("owner")], [For a batch job, the user ID of the submitter, or the
      #cmd("USER=") of the JOB statement when one was given. For a TSO user,
      the user ID. For a started task, #cmd("SYSTEM").],
    [#cmd("eclass")], [The execution class.],
    [#cmd("priority")], [The priority.],
    [#cmd("q_type")], [The JES2 queue of the job, as in #cmd("JQETYPE")
      (#cmd("<ibm/jes2/jqe.h>")): for example #cmd("_INPUT"),
      #cmd("_XEQ"), #cmd("_OUTPUT"), #cmd("_HARDCPY").],
    [#cmd("q_flag1"), #cmd("q_flag2")], [The JQE flags
      #cmd("JQEFLAGS") and #cmd("JQEFLAG2"), for example
      #cmd("QUEHOLD1") for a held job.],
    [#cmd("completion")], [After execution, when the high-order byte is
      #cmd("0x77"): the system abend code in bits 12 to 23 and the highest
      condition code in bits 0 to 11. Before execution, the return code of
      the converter: 0 for no error, 4 for a JCL error, 8 for an I/O error,
      36 for an abend.],
    [#cmd("jtflg")], [Job termination flags: #cmd("JESJOB_JF") (the job
      failed), #cmd("JESJOB_CF") (failed on a condition code),
      #cmd("JESJOB_ABD") (abended).],
    [#cmd("submit_time64")], [When the job was read in, as a
      #cmd("time64_t").],
    [#cmd("start_time64"), #cmd("end_time64")], [When execution began and
      ended. 0 when the job has not started or not ended.],
    [#cmd("sysid")], [The system identifier of the input processor, a string
      of up to 4 characters.],
    [#cmd("jobkey")], [The JES2 job key, which every spool block of the job
      carries.],
    [#cmd("jesdd")], [The data sets of the job, a dynamic array of
      #cmd("JESDD") pointers (see #cmd("arraycount()")), sorted by data set
      ID. #cmd("NULL") when #cmd("jesjob()") was called without data set
      information.],
  )
] <mvs-subsys-jesjob-tab>

The three times are the local time of day at which JES2 recorded them,
stored so that #cmd("gmtime64()") returns that time of day unchanged. Do
not convert them with #cmd("localtime64()").

#tab(caption: [Fields of JESDD])[
  #table(columns: (1.15in, 1fr),
    [Field], [Contents],
    [#cmd("ddname")], [The DD name. JES2's own data sets are named
      #cmd("JESJCLIN"), #cmd("JESMSGLG"), #cmd("JESJCL"),
      #cmd("JESYSMSG"), #cmd("JESINTXT") and #cmd("JESJRNL"). A data set
      whose DD name cannot be found is named #cmd("UNK")#var("nnnn"), and
      the data sets of the system log #cmd("JES")#var("nnnnn"), after the
      data set ID.],
    [#cmd("stepname"), #cmd("procstep")], [The step and procedure step that
      allocated the data set, where known.],
    [#cmd("dsname")], [The data set name, or a name built from the job
      identifier and the data set ID, such as
      #cmd("JES2.JOB01234.SO0002").],
    [#cmd("oclass")], [The output class.],
    [#cmd("recfm"), #cmd("lrecl")], [The record format, with the bits of the
      DCB #cmd("RECFM") (#cmd("RECFM_F"), #cmd("RECFM_V"),
      #cmd("RECFM_CA") ...), and the record length.],
    [#cmd("flag")], [#cmd("FLAG_JES2") for a data set of JES2,
      #cmd("FLAG_SYSOUT") or #cmd("FLAG_SYSIN").],
    [#cmd("records")], [The number of records.],
    [#cmd("dsid")], [The data set ID. Values 1 to 6 are JES2's own:
      #cmd("DSID_INJCL"), #cmd("DSID_OUHJL") (the job log),
      #cmd("DSID_OUJCI") (the JCL images), #cmd("DSID_OUMSG") (the system
      messages), #cmd("DSID_INTXT"), #cmd("DSID_INJNL"). Higher values are
      SYSOUT data sets of the job.],
    [#cmd("mttr")], [The spool address of the first block, 0 when nothing
      has been written.],
  )
] <mvs-subsys-jesdd-tab>

@mvs-subsys-ex-joblist lists the jobs whose names match a pattern and
prints the job log of each.

#fig(caption: [Listing jobs and printing their job logs])[
  #code(read("../ex/mvs-subsys/joblist.c"))
] <mvs-subsys-ex-joblist>

== jesopen <mvs-subsys-jesopen>

#idx("jesopen")
=== Format

```
#include <mvs/jes2.h>

JES *jesopen(void);
```

=== Description

#cmd("jesopen()") opens the checkpoint data set through the DD statement
#cmd("HASPCKPT") and the spool data set through #cmd("HASPACE1"), reads the
job queue of the checkpoint into storage, and returns a handle for
#cmd("jesjob()") and #cmd("jesprint()"). The open runs under
#cmd("try()"), so an abend while the checkpoint is read ends the call
instead of the program.

=== Returns

The handle, or #cmd("NULL") when storage could not be obtained or one of the
data sets could not be opened.

=== Notes

When it fails, #cmd("jesopen()") writes the reason to the operator
console, for example:

```
Unable to open checkpoint dataset DD:HASPCKPT
```

The handle owns a copy of the job queue, which can be several tens of
kilobytes; close it when it is no longer needed.

=== Related

@mvs-subsys-jesclose, @mvs-subsys-jesjob, @mvs-subsys-cpopen

== jesclose <mvs-subsys-jesclose>

#idx("jesclose")
=== Format

```
#include <mvs/jes2.h>

int jesclose(JES **jes);
```

=== Description

#cmd("jesclose()") closes the checkpoint and spool data sets of the handle
#var("*jes"), frees the handle, and sets #var("*jes") to #cmd("NULL").
#cmd("NULL") for #var("jes") or #var("*jes") is ignored.

The handle also keeps track of the storage a #cmd("jesjob()") call is
building. When that call ends in an abend, for example under the
recovery of the caller, #cmd("jesclose()") frees what it had built.

=== Returns

0.

=== Notes

#cmd("jesclose()") does not free the job lists that #cmd("jesjob()")
returned; free them with #cmd("jesjobfr()").

=== Related

@mvs-subsys-jesopen, @mvs-subsys-jesjobfr

== jesjob <mvs-subsys-jesjob>

#idx("jesjob")
#idx("JESJOB")
=== Format

```
#include <mvs/jes2.h>

JESJOB **jesjob(JES *jes, const char *filter, JESFILT type, int dd);
```

=== Description

#cmd("jesjob()") returns the jobs of the JES2 queue in the copy that
#cmd("jesopen()") read. Free and purged queue elements are skipped.

#var("type") selects the jobs:

#deflist(width: 1.3in,
  [#cmd("FILTER_NONE")], [every job; #var("filter") is ignored.],
  [#cmd("FILTER_JOBNAME")], [the jobs whose name matches #var("filter").],
  [#cmd("FILTER_JOBID")], [the jobs whose identifier matches
    #var("filter").],
)

#var("filter") is a pattern of up to 11 characters, changed to uppercase
before the comparison. A question mark matches any one character, an
asterisk any sequence of characters: at the end of the pattern, also no
character at all, elsewhere at least one. A #cmd("NULL") #var("filter")
selects every job.

When #var("dd") is not zero, each #cmd("JESJOB") also receives the list of
its data sets in #cmd("jesdd"). This requires reading the input/output
tables and the internal text of every selected job from the spool, which
takes longer. Without it, the internal text is read only for batch jobs, to
find the #cmd("USER=") of the JOB statement.

=== Returns

A dynamic array of #cmd("JESJOB") pointers; #cmd("arraycount()") gives
their number. #cmd("NULL") when no job was selected, when #var("jes") is
not a usable handle, or when storage ran out before the first job. When
storage runs out later, the jobs found so far are returned.

=== Notes

A job whose control table cannot be read from the spool is left out
without a message. The caller owns the array and frees it with
#cmd("jesjobfr()").

A filter of 12 or more characters is not cut to 11: the uppercase copy is
written past its 12-byte buffer. Keep filters to 11 characters.

=== Related

@mvs-subsys-jesjobfr, @mvs-subsys-jesprint, @mvs-subsys-jes

== jesjobfr, jesjobf1 <mvs-subsys-jesjobfr>

#idx("jesjobfr")
#idx("jesjobf1")
=== Format

```
#include <mvs/jes2.h>

int jesjobfr(JESJOB ***jobs);
int jesjobf1(JESJOB **job);
```

=== Description

#cmd("jesjobfr()") frees an array returned by #cmd("jesjob()"), with every
#cmd("JESJOB") and #cmd("JESDD") in it, and sets #var("*jobs") to
#cmd("NULL"). #cmd("jesjobf1()") frees a single #cmd("JESJOB") with its
data sets and sets #var("*job") to #cmd("NULL"). #cmd("NULL") arguments
are ignored.

=== Returns

0.

=== Notes

To keep one job of an array and free the rest, take its pointer out of the
array and set the element to #cmd("NULL") before calling
#cmd("jesjobfr()").

=== Related

@mvs-subsys-jesjob

== jesprint <mvs-subsys-jesprint>

#idx("jesprint")
#idx("JESPRST")
#idx("JES2", "printing a SYSOUT data set")
=== Format

```
#include <mvs/jes2.h>

int jesprint(JES *jes, JESJOB *job, unsigned dsid,
             int (*prt)(const char *line, unsigned linelen, void *arg),
             void *arg, JESPRST *st);
```

=== Description

#cmd("jesprint()") reads the data set with ID #var("dsid") of #var("job")
from the spool and calls #var("prt") for each line, passing #var("arg")
unchanged. The job must come from #cmd("jesjob()") with data set
information (#var("dd") not zero), since the data set is looked up in its
#cmd("jesdd") list.

Each line is passed as a pointer and a length, without a null character
and without its carriage-control character. Trailing blanks are removed,
the line is cut to 255 characters, and characters that cannot be printed
(below #cmd("X'40'"), and #cmd("X'FF'")) are replaced by blanks. A record
that JES2 wrote in several parts is passed as one line. The callback
returns 0 to go on, or a negative value to stop.

The data set is read block by block along the chain of spool addresses.
Every block is checked against the job key and the data set ID, and the
reading stops at the first block that does not belong to the data set. If
#var("st") is not #cmd("NULL"), #cmd("jesprint()") stores in it how the
reading ended:

#deflist(width: 0.75in,
  [#cmd("blocks")], [the number of blocks read and accepted,],
  [#cmd("lines")], [the number of lines passed to #var("prt"),],
  [#cmd("mttr")], [the spool address at which the reading stopped, 0 when it
    reached the end of the chain,],
  [#cmd("reason")], [why it stopped, one of the values in
    @mvs-subsys-jespr-tab,],
  [#cmd("prtrc")], [the return code of #var("prt") when it stopped the
    reading.],
)

#tab(caption: [jesprint() reasons])[
  #table(columns: (1.3in, 1fr),
    [Reason], [Meaning],
    [#cmd("JESPR_END")], [The end of the chain was reached; the data set
      was read in full.],
    [#cmd("JESPR_OPENEND")], [A block after the first belongs to another
      job or data set. The data set is still being written: its last block
      points to a track that has not been written yet. Everything written so
      far was read. This is the normal end for the output of a running
      job.],
    [#cmd("JESPR_EMPTY")], [Nothing has been written to the data set yet.
      Also the value when #cmd("jesprint()") returns 404 or 503.],
    [#cmd("JESPR_FOREIGN")], [The first block belongs to another job.
      Nothing of the data set was read: JES2 has purged it since the
      checkpoint was read and has given its tracks to another job.],
    [#cmd("JESPR_DSID")], [The first block belongs to another data set of
      the same job.],
    [#cmd("JESPR_IOERR")], [A spool block could not be read.],
    [#cmd("JESPR_LOOP")], [A block points to itself.],
    [#cmd("JESPR_CAP")], [#cmd("JESPR_MAXBLK") (65536) blocks were read; the
      rest of the chain was not followed. A spool volume holds fewer blocks
      than that, so this means a damaged chain.],
    [#cmd("JESPR_STOPPED")], [#var("prt") returned a negative value, which
      is in #cmd("prtrc").],
    [#cmd("JESPR_NOBUF")], [A record in several parts could not be put
      together. What had been assembled was passed as a shortened line, the
      rest of that block was skipped, and the following blocks were read.],
    [#cmd("JESPR_TRUNC")], [A record runs past the end of its block. The
      rest of that block was skipped, and the following blocks were
      read.],
    [#cmd("JESPR_NOMEM")], [A buffer could not be obtained.],
  )
] <mvs-subsys-jespr-tab>

=== Returns

#deflist(width: 0.6in,
  [0], [The data set exists, and #var("st") tells how far it was read. This
    includes a reading stopped by #var("prt").],
  [404], [The job has no data set with ID #var("dsid").],
  [503], [The request could not be served: #var("jes") or #var("job") is
    #cmd("NULL"), #var("dsid") is 0, or the handle has no checkpoint or
    spool.],
)

A buffer shortage before the data set is found also returns 503, with the
reason #cmd("JESPR_NOMEM").

=== Notes

The return value does not tell an empty data set from one that was lost, or
a complete reading from a stopped one. A program that has to distinguish
them must pass #var("st") and examine #cmd("reason").

The line passed to #var("prt") is in a buffer of the library, which is
reused for the next line; copy what is to be kept.

=== Example

See @mvs-subsys-ex-joblist.

=== Related

@mvs-subsys-jesjob, @mvs-subsys-jsopen

== The JES2 Checkpoint Data Set <mvs-subsys-cp>

#idx("checkpoint data set", "reading")
#idx("HASPCP")
The functions of #cmd("<mvs/jes2ckpt.h>") open and read the JES2
checkpoint data set. #cmd("jesopen()") is built on them; a program uses them
directly to examine checkpoint records that #cmd("jesjob()") does not
interpret. Each function has a second name, a macro defined in the header,
listed in @mvs-subsys-cp-tab.

#tab(caption: [Checkpoint functions and their macro names])[
  #table(columns: (1.3in, 1.5in),
    [Function], [Macro],
    [#cmd("__cpopen()")], [#cmd("checkpoint_open")],
    [#cmd("__cpclos()")], [#cmd("checkpoint_close")],
    [#cmd("__cppoin()")], [#cmd("checkpoint_point")],
    [#cmd("__cpread()")], [#cmd("checkpoint_read")],
  )
] <mvs-subsys-cp-tab>

== \_\_cpopen, \_\_cpclos <mvs-subsys-cpopen>

#idx("__cpopen")
#idx("__cpclos")
#idx("checkpoint_open")
#idx("checkpoint_close")
=== Format

```
#include <mvs/jes2ckpt.h>

HASPCP *__cpopen(const char *dataset);
int     __cpclos(HASPCP *cp);
```

=== Description

#cmd("__cpopen()") opens the checkpoint data set and reads the job queue
into storage. #var("dataset") is either #cmd("DD:")#var("ddname"), for a
data set allocated by a DD statement, or a data set name, which is then
allocated dynamically with #cmd("DISP=SHR"); the data set must be
cataloged.

After the open, the handle holds the checkpoint control table of JES2
(record 3 of the data set) in #cmd("hct"), the job queue elements from
#cmd("jqe") up to #cmd("jqeend"), and the job output table from
#cmd("jot") up to #cmd("jotend"). These are mapped by
#cmd("<ibm/jes2/hct.h>"), #cmd("<ibm/jes2/jqe.h>") and
#cmd("<ibm/jes2/jot.h>"). #cmd("pddb1") is the offset of the first
peripheral data definition block in an input/output table of the spool.

#cmd("__cpclos()") closes the data set, frees a dynamic allocation, and
frees the handle. #cmd("NULL") is ignored.

=== Returns

#cmd("__cpopen()") returns the handle, or #cmd("NULL") when storage could
not be obtained, the allocation failed, or the data set could not be
opened. #cmd("__cpclos()") returns 0.

=== Notes

When the buffer for the job queue cannot be obtained, #cmd("__cpopen()")
still returns the handle, with #cmd("buf") and #cmd("jqe") set to
#cmd("NULL"). Check #cmd("buf") before using the queue.

=== Related

@mvs-subsys-cppoin, @mvs-subsys-jesopen

== \_\_cppoin, \_\_cpread <mvs-subsys-cppoin>

#idx("__cppoin")
#idx("__cpread")
#idx("checkpoint_point")
#idx("checkpoint_read")
=== Format

```
#include <mvs/jes2ckpt.h>

int __cppoin(HASPCP *cp, unsigned TTRz);
int __cpread(HASPCP *cp, void *buf4k);
```

=== Description

#cmd("__cppoin()") positions the checkpoint data set at the block with the
relative address #var("TTRz") (#cmd("POINT")). #cmd("__cpread()") reads
the next block, at most 4096 bytes, into #var("buf4k") (#cmd("READ") and
#cmd("CHECK")).

=== Returns

0, or -1 when #var("cp") is #cmd("NULL") or its data set is not open.

=== Notes

The return code does not report I/O errors. An error that #cmd("CHECK")
detects ends the program with an abend.

=== Related

@mvs-subsys-cpopen

== The JES2 Spool Data Set <mvs-subsys-js>

#idx("spool", "reading")
#idx("HASPJS")
The functions of #cmd("<mvs/jes2spool.h>") open the JES2 spool data set and
read its blocks by spool address. As with the checkpoint, each function has
a second name, a macro defined in the header: #cmd("spool_open") for
#cmd("__jsopen()"), #cmd("spool_close") for #cmd("__jsclos()"), and
#cmd("spool_read") for #cmd("__jsrd4()").

== \_\_jsopen, \_\_jsclos <mvs-subsys-jsopen>

#idx("__jsopen")
#idx("__jsclos")
#idx("spool_open")
#idx("spool_close")
=== Format

```
#include <mvs/jes2spool.h>

HASPJS *__jsopen(const char *dataset);
int     __jsclos(HASPJS *js);
```

=== Description

#cmd("__jsopen()") opens a spool data set for direct access (BDAM).
#var("dataset") is #cmd("DD:")#var("ddname") or a cataloged data set name,
allocated dynamically with #cmd("DISP=SHR"). The handle records the number
of tracks per cylinder of the spool device in #cmd("trkcyl"), which
#cmd("__jsrd4()") needs to convert a spool address.

#cmd("__jsclos()") closes the data set, frees a dynamic allocation and frees
the handle. #cmd("NULL") is ignored.

=== Returns

#cmd("__jsopen()") returns the handle, or #cmd("NULL") when storage could
not be obtained or the allocation failed. #cmd("__jsclos()") returns 0.

=== Notes

When the data set is allocated but cannot be opened, #cmd("__jsopen()")
stores through a null pointer after it has closed the handle, which
abends. Make sure the DD statement names a spool data set before the call.

=== Related

@mvs-subsys-jsrd4, @mvs-subsys-jesopen

== \_\_jsrd4 <mvs-subsys-jsrd4>

#idx("__jsrd4")
#idx("spool_read")
#idx("MTTR")
=== Format

```
#include <mvs/jes2spool.h>

int __jsrd4(HASPJS *js, unsigned mttr, void *buf4k, unsigned buflen);
```

=== Description

#cmd("__jsrd4()") reads the spool block at address #var("mttr") into
#var("buf4k"). A spool address is the JES2 form #var("MTTR"): one byte of
extent, two bytes of relative track, one byte of record number. The
relative track is converted into cylinder and head with the
#cmd("trkcyl") of the handle. #var("buflen") is the length of the block,
the #cmd("BUFSIZE") of JES2, which the checkpoint control table holds in
#cmd("_BUFSIZE").

=== Returns

0 when the block was read. When the read fails, the completion code that
the error routine found in the event control block of the read. -1 when
#var("js") is #cmd("NULL").

=== Notes

The extent byte of #var("mttr") is not used: only the first spool volume
can be read.

=== Related

@mvs-subsys-jsopen, @mvs-subsys-jesprint

== Requests to JES2 <mvs-subsys-req>

#idx("JES2", "requests")
#idx("SYSOUT", "requests to JES2")
The functions below ask JES2 to act on a job or its output through the
subsystem interface (#cmd("IEFSSREQ")): to cancel a job, to delete or
release its output, and to select output for a program that writes it
elsewhere. They build an SSOB with #cmd("initssob()") and call
#cmd("iefssreq()"), so they require APF authorization.

Each returns the return code that JES2 placed in #cmd("SSOBRETN"). The
return code of the subsystem interface itself is not examined: when JES2 is
not active or does not take the request, #cmd("SSOBRETN") stays 0 and the
function returns 0 as if the request had been carried out.

== jescanj <mvs-subsys-jescanj>

#idx("jescanj")
#idx("cancel a job")
=== Format

```
#include <mvs/jes2.h>

int jescanj(const char *jobname, const char *jobid, int purge_output);
```

=== Description

#cmd("jescanj()") asks JES2 to cancel the job #var("jobname") with
identifier #var("jobid"). Either may be #cmd("NULL"). When
#var("purge_output") is not zero, the output of the job is purged as
well.

=== Returns

The JES2 return code, one of the values in @mvs-subsys-canj-tab.

#tab(caption: [jescanj() return codes])[
  #table(columns: (0.4in, 1.1in, 1fr),
    [Code], [Name], [Meaning],
    [0], [#cmd("CANJ_OK")], [The job was cancelled.],
    [4], [#cmd("CANJ_NOJB")], [The job name was not found.],
    [8], [#cmd("CANJ_BADI")], [The job name and identifier do not belong
      together.],
    [12], [#cmd("CANJ_NCAN")], [Not cancelled: several jobs have this name
      and no identifier was given.],
    [16], [#cmd("CANJ_SMALL")], [The status array was too small.],
    [20], [#cmd("CANJ_OUTP")], [Not cancelled: the job is on the output
      queue.],
    [24], [#cmd("CANJ_SYNTX")], [The identifier is not valid for the
      subsystem.],
    [28], [#cmd("CANJ_ICAN")], [The request is not allowed: an active TSO
      user or started task cannot be cancelled this way.],
  )
] <mvs-subsys-canj-tab>

=== Notes

Requires APF authorization. The status JES2 returns for the job is not
passed back to the caller.

=== Related

@mvs-subsys-jesdelj

== jesdelj <mvs-subsys-jesdelj>

#idx("jesdelj")
=== Format

```
#include <mvs/jes2.h>

int jesdelj(const char *jobname, const char *jobid);
```

=== Description

#cmd("jesdelj()") asks JES2 to delete the SYSOUT data sets of the jobs
selected by #var("jobname"), #var("jobid") or both, held output included.

=== Returns

-1 when #var("jobname") and #var("jobid") are both #cmd("NULL").
Otherwise the JES2 return code, one of the values in
@mvs-subsys-delj-tab.

#tab(caption: [jesdelj() and jesreque() return codes])[
  #table(columns: (0.4in, 1.6in, 1fr),
    [Code], [Name], [Meaning],
    [0], [#cmd("DELJ_OK"), #cmd("REQUE_OK")], [The request was carried
      out.],
    [4], [#cmd("DELJ_EODS"), #cmd("REQUE_EODS")], [No more data sets to
      select.],
    [8], [#cmd("DELJ_NJOB"), #cmd("REQUE_NJOB")], [The job was not
      found.],
    [12], [#cmd("DELJ_INVA"), #cmd("REQUE_INVA")], [The search arguments
      are not valid.],
    [16], [#cmd("DELJ_UNAV"), #cmd("REQUE_UNAV")], [JES2 cannot process the
      request now.],
    [20], [#cmd("DELJ_DUPJ"), #cmd("REQUE_DUPJ")], [Several jobs have this
      name.],
    [24], [#cmd("DELJ_INVJ"), #cmd("REQUE_INVJ")], [The job name and
      identifier do not belong together.],
    [28], [#cmd("DELJ_IDST"), #cmd("REQUE_IDST")], [The destination is not
      valid.],
  )
] <mvs-subsys-delj-tab>

=== Notes

Requires APF authorization.

=== Related

@mvs-subsys-jesreque, @mvs-subsys-jescanj

== jesreque <mvs-subsys-jesreque>

#idx("jesreque")
=== Format

```
#include <mvs/jes2.h>

int jesreque(const char *jobname, const char *jobid, const char *oclass);
```

=== Description

#cmd("jesreque()") asks JES2 to release the SYSOUT data sets of the
selected jobs, held output included, and to move them to the output class
whose letter is the first character of #var("oclass"). When #var("oclass")
is #cmd("NULL") or empty, class #cmd("C") is used.

=== Returns

-1 when #var("jobname") and #var("jobid") are both #cmd("NULL").
Otherwise the JES2 return code, as in @mvs-subsys-delj-tab.

=== Notes

Requires APF authorization.

=== Related

@mvs-subsys-jesdelj

== jesxwrtr, jesxdone <mvs-subsys-jesxwrtr>

#idx("jesxwrtr")
#idx("jesxdone")
#idx("external writer")
=== Format

```
#include <mvs/jes2.h>

int jesxwrtr(SSSO *ssso, const char *class_list, const char *dest,
             const char *form);
int jesxdone(SSSO *ssso);
```

=== Description

These two functions let a program take SYSOUT data sets from JES2, as an
external writer does. #cmd("jesxwrtr()") clears the SYSOUT extension
#var("ssso"), selects data sets by the classes in #var("class_list") (up to
8 class letters), the destination #var("dest") and the form number
#var("form"), and asks JES2 for the next data set that matches. Any of the
three may be #cmd("NULL") or empty, but not all of them. On return the
#cmd("SSSO") contains JES2's answer, among it the name of the selected data
set in #cmd("SSSODSN"); the SSSO is mapped by
#cmd("<ibm/mvs/iefssso.h>").

#cmd("jesxdone()") tells JES2, with the same #var("ssso"), that the program
has finished with SYSOUT processing.

=== Returns

The JES2 return code from #cmd("SSOBRETN"). #cmd("jesxwrtr()") returns -1
when #var("ssso") is #cmd("NULL") or no selection was given.

=== Notes

Requires APF authorization. #cmd("jesxdone()") does not check
#var("ssso"). The selection by writer name that the SSSO supports is not
offered.

=== Related

@mvs-subsys-iefssreq

== \_\_getpso <mvs-subsys-getpso>

#idx("__getpso")
#idx("PSO")
=== Format

```
#include <mvs/jes2.h>

__PSO *__getpso(void);
```

=== Description

#cmd("__getpso()") returns the process SYSOUT work area (#cmd("$PSO")) of
the job the program runs in, found through the TCB, the JSCB, the SSIB and
the JES2 subsystem job block, whose field #cmd("SJBPSOP") addresses it.
The area is mapped by #cmd("<ibm/jes2/pso.h>").

=== Returns

The value of #cmd("SJBPSOP"): the address of the PSO, or 0 when the job has
none.

== The Internal Reader <mvs-subsys-intrdr>

#idx("internal reader")
#idx("INTRDR")
#idx("submit a job")
The internal reader passes a job to JES2 as if it had been read from a card
reader. #cmd("jesiropn()") allocates a SYSOUT data set with the writer name
#cmd("INTRDR") and opens it, #cmd("jesirput()") writes one 80-byte
statement, and #cmd("jesircl2()") ends the job and returns the identifier
JES2 gave it. No authorization is required.
@mvs-subsys-ex-submit submits a job.

#fig(caption: [Submitting a job through the internal reader])[
  #code(read("../ex/mvs-subsys/submit.c"))
] <mvs-subsys-ex-submit>

== jesiropn <mvs-subsys-jesiropn>

#idx("jesiropn")
=== Format

```
#include <mvs/jes2.h>

int jesiropn(VSFILE **vsfile);
```

=== Description

#cmd("jesiropn()") allocates the internal reader dynamically, with
#cmd("SYSOUT") and writer name #cmd("INTRDR") and with deallocation at
close, opens it, and stores the handle in #var("*vsfile").

=== Returns

0 when the reader is open. Otherwise the error code of the dynamic
allocation, or #cmd("ENOMEM") or #cmd("EVSOPEN") when the open failed.

=== Errors

#deflist(width: 1in,
  [#cmd("ENOMEM")], [Storage for the handle could not be obtained.],
  [#cmd("EVSOPEN")], [The internal reader could not be opened.],
)

=== Notes

When the open fails, #var("*vsfile") is set to #cmd("NULL") and the
allocation is released. When the allocation fails, #var("*vsfile") is not
changed and #cmd("errno") is not set.

=== Related

@mvs-subsys-jesirput, @mvs-subsys-jesircls

== jesirput <mvs-subsys-jesirput>

#idx("jesirput")
=== Format

```
#include <mvs/jes2.h>

int jesirput(VSFILE *vsfile, char card[80]);
```

=== Description

#cmd("jesirput()") writes one statement to the internal reader. #var("card")
is a string; it is padded with blanks to 80 characters, or cut to 80.

=== Returns

#deflist(width: 0.6in,
  [0], [The statement was written.],
  [-1], [#var("card") is #cmd("NULL") or empty. Nothing is written.],
  [-2], [#var("vsfile") is #cmd("NULL").],
  [other], [The return code of the VSAM #cmd("PUT").],
)

=== Notes

An empty string is refused; to write a blank statement pass a string of
one blank. The function does not change #var("card"), although the header
declares the parameter without #cmd("const").

=== Related

@mvs-subsys-jesiropn, @mvs-subsys-jesircls

== jesircls, jesircl2 <mvs-subsys-jesircls>

#idx("jesircls")
#idx("jesircl2")
=== Format

```
#include <mvs/jes2.h>

int jesircls(VSFILE *vsfile);
int jesircl2(VSFILE *vsfile, unsigned char jobid[8]);
```

=== Description

#cmd("jesircl2()") ends the job written to the internal reader (VSAM
#cmd("ENDREQ")), copies the job identifier that JES2 returns, for example
#cmd("JOB01234"), to #var("jobid"), closes the reader and frees the handle.
#var("jobid") may be #cmd("NULL"). #cmd("jesircls()") is
#cmd("jesircl2()") without the job identifier.

=== Returns

The return code of the #cmd("ENDREQ"); 0 when #var("vsfile") is
#cmd("NULL").

=== Notes

The job identifier is eight characters without a null character. It is
copied out before the handle is freed: do not try to read it through the
handle after #cmd("jesircls()"). When #var("vsfile") is #cmd("NULL"),
#var("jobid") is cleared to zeros.

=== Related

@mvs-subsys-jesiropn

== ISPF Services <mvs-subsys-ispf>

#idx("ISPF", "dialog services")
#idx("ISPLINK")
#idx("ISPEXEC")
The header #cmd("<mvs/ispf.h>") gives a C program access to the dialog
services of ISPF. ISPF provides two interfaces: #cmd("ISPLINK"), called
with one parameter per operand, and #cmd("ISPEXEC"), which takes the
service request as a character string. Both are routines of ISPF itself and
are not part of libc370; the header declares them, together with
#cmd("ISPQRY"), under their own names. #cmd("ISPLINK") and #cmd("ISPEXEC")
need a parameter list in which the last address has its high-order bit set,
which a C call does not build; call the libc370 functions
#cmd("isplink()"), #cmd("ispexec()") and #cmd("isp_select()") instead.

The program must run as a TSO command processor under ISPF.
#cmd("isplink()") and #cmd("isp_select()") call #cmd("ISPLINK") directly,
which the linkage editor has to resolve: link the program with the ISPF
load library that contains #cmd("ISPLINK"), or link it with unresolved
references allowed so that the reference is resolved on MVS.

The header defines the service names as macros, padded with blanks to the
eight characters #cmd("ISPLINK") expects: #cmd("ISP_DISPLAY") is
#cmd("\"DISPLAY \""), #cmd("ISP_TBCREATE") is #cmd("\"TBCREATE\""), and so
on for the display, file tailoring, library access, PDF, table, variable
and miscellaneous services. #cmd("ISP_DEFAULT") is eight blanks, for an
operand that takes its default value. #cmd("ISP_LAST(a)") sets the
high-order bit of the pointer #var("a"), to mark the last parameter.

#note[#cmd("ISP_LMMDEL") is defined as #cmd("\"LMDDEL  \""). The ISPF
service that deletes a member is #cmd("LMMDEL"); write the name as a string
instead of using the macro.]

== ispf\_available <mvs-subsys-ispf-available>

#idx("ispf_available")
#idx("ispqry")
=== Format

```
#include <mvs/ispf.h>

int ispqry(void);
#define ispf_available() (ispqry() == 0 ? 1 : 0)
```

=== Description

#cmd("ispf_available()") is true when ISPF services are available to the
program. It calls #cmd("ISPQRY"), the ISPF routine that answers 0 when the
services exist and 20 when they do not.

=== Notes

#cmd("ISPQRY") is not part of libc370. A program that uses this macro must
be linked with the ISPF load library that contains it.

== isplink <mvs-subsys-isplink>

#idx("isplink")
=== Format

```
#include <mvs/ispf.h>

int isplink(const char *service_name, ...);
```

=== Description

#cmd("isplink()") calls #cmd("ISPLINK") with its own parameter list: the
address of #var("service_name") followed by the further arguments. Each
argument is therefore a pointer, as ISPLINK expects, and the last must be
marked with #cmd("ISP_LAST()").

=== Returns

The return code of the ISPF service, or 20 when #var("service_name") is
#cmd("NULL") or no argument among the first twenty carries the end mark.

=== Notes

A missing end mark is reported to the operator console.

=== Example

```
int rc = isplink(ISP_VGET, ISP_LAST("(ZUSER)"));
```

=== Related

@mvs-subsys-isp-select, @mvs-subsys-ispexec

== ispexec <mvs-subsys-ispexec>

#idx("ispexec")
=== Format

```
#include <mvs/ispf.h>

int ispexec(const char *fmt, ...);
```

=== Description

#cmd("ispexec()") formats a service request as #cmd("printf()") does and
runs the TSO command #cmd("ISPEXEC") with it as the operand, as a command
procedure would. For example:

```
rc = ispexec("DISPLAY PANEL(%s)", name);
```

=== Returns

The return code of #cmd("ISPEXEC"). 8 when the program does not run as a
TSO command processor.

=== Notes

The formatted request must be shorter than 256 characters; a longer one
overruns the buffer. The formatted request is formatted a second time
before it is passed on, so a #cmd("%") in it, for example from a
substituted value, is interpreted again: double it, or use
#cmd("isplink()").

=== Related

@mvs-subsys-isplink, @mvs-subsys-isp-select

== isp\_select <mvs-subsys-isp-select>

#idx("isp_select")
=== Format

```
#include <mvs/ispf.h>

int isp_select(const char *fmt, ...);
```

=== Description

#cmd("isp_select()") formats the operand of a #cmd("SELECT") service as
#cmd("printf()") does and calls #cmd("ISPLINK") with #cmd("SELECT"), the
length of the operand and the operand. For example:

```
rc = isp_select("PGM(%s) PARM(%s)", pgm, parm);
```

=== Returns

The return code of the #cmd("SELECT") service.

=== Notes

The formatted operand must be shorter than 256 characters.

=== Related

@mvs-subsys-isplink
