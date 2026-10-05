#import "../bookmaster/bookmaster.typ": *

= Using MVS Services <pg-services>

// Narrow cells: ragged right, no hyphenation of the names.
#show table: set par(justify: false)
#show table: set text(hyphenate: false)

#idx("MVS services")
A C program on MVS does much of its work through services that have no
counterpart in standard C: it allocates data sets while it runs, asks the
catalog what exists, reads VSAM clusters, talks to the operator, writes
accounting records and hands jobs to JES2. libc370 gives each of these a C
interface. This chapter takes one typical task for each and walks through
it:

#deflist(width: 1.6in,
  [Authorization], [which services need an APF-authorized program, and how
    a program becomes one (@pg-services-auth).],
  [Dynamic allocation], [creating and freeing DD statements, and running
    IDCAMS (@pg-services-dynalloc).],
  [Finding data sets], [the catalog, the disk volumes and the directory of
    a partitioned data set (@pg-services-find).],
  [VSAM], [reading and updating a key-sequenced cluster
    (@pg-services-vsam).],
  [The operator], [messages, replies, and the MODIFY and STOP commands
    (@pg-services-console).],
  [SMF], [writing a record of your own (@pg-services-smf).],
  [JES2], [submitting a job and following it through the queue
    (@pg-services-jes2).],
)

Every function used here is described in full in the _libc370 Library
Reference_. That book also lists, in the notes of each entry, the cases in
which a function does not do what its name suggests\; the examples in this
chapter keep clear of them, and the text says why where it matters.

#idx("return code", "of a library service")
Two conventions hold throughout. A library function reports a failure
through its return value and, where the entry says so, #cmd("errno")\; it
does not, as a rule, write a message to the operator. If a failure belongs
on the console, the program writes it there itself (@pg-services-console).
And the return code of a service says whether the request was made, not
always whether the work was done: where a function returns a list or a
count, look at the result as well.

#idx("-Wno-comment")
#note[Several headers of this chapter include mappings of IBM control
blocks that contain #cmd("/*") inside a comment. When the headers are installed in the cc370 sysroot,
as they normally are, the compiler treats them as system headers and does
not report these warnings. When you compile against a copy of the headers
named with #cmd("-I"), it does, and #cmd("-Werror") turns them into errors
(@pg-services-warn-fig). Add #cmd("-Wno-comment") in that case.]

#fig(caption: [The same source against the sysroot and against a copy of
  the headers])[
  #screen(raw(read("../ex/pg-services/compile.txt")))
] <pg-services-warn-fig>

// -------------------------------------------------------------------------
== What Needs Authorization <pg-services-auth>

#idx("APF authorization", "services that need it")
#idx("authorization", "of a program")
Most of the services in this chapter are open to any program.
@pg-services-auth-tab lists those that are not. The requirement comes from
MVS: a service that MVS restricts to authorized callers is reached through
#cmd("MODESET"), and #cmd("MODESET") itself is open only to an
APF-authorized program.

#tab(caption: [Services of this chapter that need authorization])[
  #table(columns: (2.2in, 1fr),
    [*Function*], [*Requirement*],
    [#cmd("smf_write()")], [APF authorization (SVC 83 is restricted).
      #cmd("smf_init()") and #cmd("smf_active()") need none.],
    [#cmd("jescanj()"), #cmd("jesdelj()"), #cmd("jesreque()"),
     #cmd("jesxwrtr()"), #cmd("jesxdone()"), #cmd("iefssreq()")],
      [APF authorization. Reading the JES2 queue and using the internal
      reader need none.],
    [#cmd("ssct_...()"), #cmd("ssvt_...()")], [Supervisor state.],
    [#cmd("racf_login()"), #cmd("racf_logout()"),
     #cmd("racf_set_acee()")], [APF authorization or supervisor state.
      #cmd("racf_auth()") needs none.],
    [#cmd("cmtt_new()") (master trace table)], [APF authorization, or the
      authorization SVC 244 if the installation provides one.],
    [#cmd("__xmpost()")], [Supervisor state and PSW key 0.],
    [Dynamic allocation, IDCAMS, catalog, VTOC and PDS lists, VSAM, WTO,
     WTOR, MODIFY and STOP], [None.],
  )
] <pg-services-auth-tab>

The program management functions that need authorization -- changing the
PSW key or the state, #cmd("GETMAIN") from a protected subpool -- are listed
in the _libc370 Library Reference_, Chapter 29, “Program Management and
Storage”.

#idx("S047 abend")
A program that calls one of these functions without being authorized ends
with abend S047. To run authorized:

+ Link the program with authorization code 1. With the cc370 driver, pass
  #cmd("-Wl,--ac,1")\; with ld370, #cmd("--ac 1")\; in an mbt project, set
  #cmd("ac = 1") for the module in #cmd("project.toml").
+ Put the load module into a library that is APF-authorized, that is, named
  in the APF list of the system (#cmd("IEAAPF00") in #cmd("SYS1.PARMLIB")).
  On MVS 3.8j the list is read at IPL.
+ Run it from a step in which every library of the #cmd("STEPLIB")
  concatenation is authorized. One library that is not makes the whole
  step unauthorized.

@pg-services-link-fig links the SMF example of @pg-services-smf with
authorization code 1, and shows with #cmd("file370") that the code is in
the directory entry.

#fig(caption: [Linking an authorized program and checking the result])[
  #screen(raw(read("../ex/pg-services/link-smf.txt")))
] <pg-services-link-fig>

#idx("__isauth")
A program that can run either way tests its authorization before it calls a
restricted function, with #cmd("__isauth()") from #cmd("<mvs/apf.h>"), as
the SMF example does. On the console you can tell an authorized program by
its messages: MVS puts a #cmd("+") in front of a message written by a
program that is not authorized, and none in front of one from an authorized
program.

Before you link a program with authorization code 1, read @pg-rent: where
MVS loads a module decides whether the program may change its own static
data.

// -------------------------------------------------------------------------
== Allocating Data Sets Dynamically <pg-services-dynalloc>

#idx("dynamic allocation")
#idx("__dsalc")#idx("__dsalcf")#idx("__dsfree")
A program reaches a data set through a DD statement. Most programs get
their DD statements from the JCL, and #cmd("fopen()") can also open a data
set by its name (@pg-io). Allocate a DD yourself when the program must
control the allocation: create a data set with attributes taken from
another, keep a data set allocated across several opens, or hand a DD name
to a utility it calls. The function for this is #cmd("__dsalc()"), or
#cmd("__dsalcf()"), which formats its keyword string as #cmd("printf()")
does. Both are in #cmd("<mvs/dynalloc.h>").

The keywords are written much as on a DD statement, separated by
semicolons:

```
__dsalcf(NULL, "DD=INPUT;DSN=%s;DISP=SHR", dsn);
```

To copy a data set into a new one with the same attributes, as
@pg-services-copyds-fig does:

+ Allocate the input with #cmd("DISP=SHR") under a DD name of your choice
  (line 18). When the call returns 0, the DD exists exactly as if the JCL
  had contained it.
+ Allocate the output with #cmd("DISP=(NEW,CATLG,DELETE)") and take its
  DCB attributes from the input with #cmd("DCBDSN=") (line 25), as
  #cmd("DCB=(")#var("dsn")#cmd(")") does in JCL. Give the space as
  #cmd("SPACE=TRK(5,5)"): the JCL form #cmd("SPACE=(TRK,(5,5))"), with
  nested parentheses, is not understood and makes the call return 1.
+ Open both through their DD names, #cmd("DD:INPUT") and #cmd("DD:OUTPUT")
  (lines 36 and 37), and copy. Check the result of #cmd("fclose()") for the
  output: the last block is written when the data set is closed, and an
  out-of-space condition on it is reported there and nowhere else.
+ Free both DDs with #cmd("__dsfree()") (lines 49 and 50). This is when the
  normal disposition takes effect\; the new data set is cataloged now. Pass
  the DD name in uppercase, exactly as it was allocated: #cmd("__dsfree()")
  does not fold it.

#fig(caption: [Copying a data set into a new one through dynamic
  allocation])[
  #code(read("../ex/pg-services/copyds.c"), numbers: true)
] <pg-services-copyds-fig>

The program needs no DD statement for the data sets, only the parameter
with their names (@pg-services-copyds-jcl).

#fig(caption: [Running the copy program])[
  #code(read("../ex/pg-services/copyds.jcl"))
] <pg-services-copyds-jcl>

Points to watch:

- *The return code.* #cmd("__dsalc()") returns 0 for success, 1 when it
  could not build the request from the keywords, and otherwise the return
  code of SVC 99. It does not return the error and information reason codes
  that explain a failure. When you need them, build the request with the
  text unit functions and issue it with #cmd("__svc99()"), as the _libc370
  Library Reference_ shows in Chapter 25, “Dynamic Allocation and IDCAMS”.
- *DD names.* An allocation under a DD name that the step already has
  fails. To let MVS choose a free name, leave out #cmd("DD=") and pass a
  buffer of 9 characters as the first argument\; the name is returned
  there.
- *Mount requests.* A request with #cmd("UNIT=") or #cmd("VOLSER=") fails at
  once when the volume is not mounted, rather than asking the operator to
  mount it and waiting. Add the keyword #cmd("MOUNT") only where an operator
  is expected to mount the volume.
- *Uncataloging.* Do not use #cmd("UNCATLG") as a disposition with
  #cmd("__dsalc()"): it is taken as #cmd("CATLG").
- *Other keywords.* A keyword that the reference does not list is ignored,
  without an error. MVS 3.8j has no #cmd("LIKE=").

#idx("fopen", "creating a data set")
For a new data set that needs nothing beyond what #cmd("fopen()") can say,
there is a shorter way: #cmd("fopen()") creates a data set opened by name
for writing, and accepts the unit and volume in its mode string
(#cmd("\"w,unit=SYSDA,volser=WORK01\"")). See @pg-io.

=== Deleting a Data Set with IDCAMS <pg-services-idcams>

#idx("IDCAMS", "from a C program")
#idx("idcams_sysprint")
Deleting, renaming and cataloging are IDCAMS commands. #cmd("idcams()") and
#cmd("idcams_sysprint()"), in #cmd("<mvs/idcams.h>"), run one command
without any SYSIN or SYSPRINT DD statement. The condition code they return
does not say why a command failed: a #cmd("DELETE") of a data set that does
not exist ends with 8, and so does one that IDCAMS could not process. The
messages say which, and #cmd("idcams_sysprint()") passes each line IDCAMS
prints to a function of yours, with its message number.

@pg-services-delds-fig deletes a data set that may or may not exist, the
usual first step of a program that creates it again:

+ Format the command with a leading blank (line 24): IDCAMS does not read
  column 1 of its input.
+ In the callback, keep the first message that is not one of the summary
  messages IDC0001I and IDC0002I (line 14). Copy what you keep: the text is
  valid only during the call.
+ Treat condition code 8 with message IDC3012I, "entry not found", as
  success (line 27). Anything else is an error.

#fig(caption: [Deleting a data set that may not exist])[
  #code(read("../ex/pg-services/delds.c"), numbers: true)
] <pg-services-delds-fig>

The callback runs inside an exit of IDCAMS, on a small stack of its own.
Keep it short, and do not call IDCAMS again from it. Use #cmd("idcams()")
and #cmd("idcams_sysprint()") rather than the older #cmd("__idcams()"),
which is not reentrant and cannot run in two tasks at once.

// -------------------------------------------------------------------------
== Finding Data Sets <pg-services-find>

#idx("catalog", "listing")#idx("VTOC")#idx("PDS", "directory")
#cmd("<mvs/dslist.h>") returns what a program would otherwise read from a
LISTCAT, a VTOC listing or a member list. The list functions share one
convention: they return a dynamic array of pointers to records, whose
number #cmd("arraycount()") gives, and #cmd("NULL") both for an empty
result and for a failure. Tell the two apart by #cmd("errno"): the
functions set it to 0 on entry, so 0 afterwards means "nothing found". Each
list has a function that frees it.

=== Listing a Catalog Level <pg-services-catalog>

#idx("__listds")
#cmd("__listds()") lists the data sets cataloged under a high-level
qualifier or a longer level, and reads the format-1 DSCB of each to fill
in its organization, record format and dates. The second argument is passed
to LISTCAT and must make it show the volume of each entry:
#cmd("\"NONVSAM VOLUME\"") for non-VSAM data sets only, #cmd("\"VOLUME\"")
for all. The third is a pattern for the names, in which #cmd("*") stands for
any characters and #cmd("?") for one, or #cmd("NULL").

#fig(caption: [Listing the data sets under a level])[
  #code(read("../ex/pg-services/datasets.c"), numbers: true)
] <pg-services-datasets-fig>

When the DSCB of a data set cannot be read, for example because its volume
is not mounted, the record is still returned with the name and the volume,
and the other fields are zero. @pg-services-datasets-fig tests
#cmd("dsorg") for that (line 28). Every data set costs one OBTAIN, so a
large level takes a while.

=== Looking at Disk Volumes <pg-services-volumes>

#idx("__listvl")#idx("LSPACE")
#cmd("__listvl()") lists the disk volumes that are online and, when its
second argument is not 0, the free space of each, with LSPACE.
@pg-services-volumes-fig prints a table of the free space.

#fig(caption: [Showing the free space of the online volumes])[
  #code(read("../ex/pg-services/volumes.c"), numbers: true)
] <pg-services-volumes-fig>

When LSPACE fails for a volume, its free-space fields stay 0 and the
library writes a message to the console.

#idx("__dscbdv")
To read the VTOC entry of one data set directly, use #cmd("__dscbdv()")
from #cmd("<mvs/dscb.h>"): it reads the format-1 DSCB of a data set on a
named volume. Its companions #cmd("__dscbav()"), which reads a DSCB by its
disk address, and #cmd("__dscbv()"), which reads the format-4 DSCB, have
restrictions that the _libc370 Library Reference_ describes in Chapter 24,
“Data Sets and DD Statements”\; read them before you use either.

=== Reading a PDS Directory <pg-services-pds>

#idx("__walkpd")#idx("__fmtloa")
#cmd("__walkpd()") reads the directory of a partitioned data set and calls a
function of yours for each member, in the order of the names. It allocates
nothing for the members, so a directory of any size costs one directory
block of storage. Prefer it to #cmd("__listpd()"), which allocates a record
for every member and can exhaust the region on a large library.

The data set is named as #cmd("fopen()") names it: by data set name, or by
DD name as in @pg-services-loadlib-fig. The directory entry passed to the
function points into a buffer that the next block overwrites, so the
function formats or copies what it needs before it returns. For a load
library, #cmd("__fmtloa()") turns the user data of the entry into strings:
the size, the authorization code and the attributes, among them
#cmd("RN") (reentrant) and #cmd("RU") (reusable), which @pg-rent explains.
For a source library, #cmd("__fmtisp()") does the same for the ISPF
statistics.

#fig(caption: [Listing the modules of a load library])[
  #code(read("../ex/pg-services/loadlib.c"), numbers: true)
] <pg-services-loadlib-fig>

The function returns 0 to go on and any other value to stop the walk.
#cmd("__walkpd()") returns the number of members it passed, or -1 with
#cmd("errno") set when the directory could not be read. Run the program
with a DD statement #cmd("LOADLIB") for the library.

// -------------------------------------------------------------------------
== Reading and Updating a VSAM Cluster <pg-services-vsam>

#idx("VSAM", "updating a record")
#idx("vsopen")#idx("vsread")#idx("vsupdate")
#cmd("<mvs/vsam.h>") reads and writes key-sequenced, entry-sequenced and
relative-record clusters through a handle, a #cmd("VSFILE"). There is no
stream interface to VSAM: #cmd("fopen()") does not open a cluster.

#note[The VSAM functions are built into the library but are not covered by
its test suites. Try your program against a copy of your own cluster before
you let it change data that matters.]

The cluster is named by a DD statement. A typical task is to read one
record by its key and rewrite it:

+ Open the handle with #cmd("vsopen()") for the DD name, the type of the
  cluster, the access and the mode. For a lookup by key followed by more
  work on the same record, use #cmd("VSACCESS_DYNAM")\; to rewrite records,
  #cmd("VSMODE_UPD"). On failure #cmd("vsopen()") returns the reason, also
  in #cmd("errno"), and leaves nothing allocated.
+ Read the record with #cmd("vsread()"), giving the key and its length.
  The key must be the full key as the cluster holds it, here blank-padded to
  8 characters. A key that is not found is reported as an error, -2 with
  reason code 16 in #cmd("vs->rsn"), not as end of data.
+ Change the record and rewrite it with #cmd("vsupdate()"). The update
  applies to the record the handle read last, so issue no other request on
  the handle between the read and the update.
+ Close the handle with #cmd("vsclose()"), which also frees it.

#fig(caption: [Updating a record of a KSDS])[
  #code(read("../ex/pg-services/vsupd.c"), numbers: true)
] <pg-services-vsupd-fig>

#fig(caption: [Running the update])[
  #code(read("../ex/pg-services/vsupd.jcl"))
] <pg-services-vsupd-jcl>

#idx("vsclear")
*The return value says what this read did.* #cmd("vsread()") returns the
length of the record, -1 at the end of the data -- also for any further
read after the end -- and -2 when its own request failed. The handle also
remembers an end of data or an error, as a stream remembers end of file and
errors: #cmd("vseof()") and #cmd("vserror()") report it until
#cmd("vsclear()") resets it. A remembered error does not make a later read
fail, so a program that handles an error, such as a key not found, can read
on.

Each function exists in two forms: #cmd("vsread()") takes a lock on the
handle around the request, so that several tasks can share one handle, and
#cmd("__vsread()") does not. One handle has one position in the cluster, so
tasks that share a handle must also agree on whose position it is.
Password-protected clusters are not supported.

// -------------------------------------------------------------------------
== Communicating with the Operator <pg-services-console>

#idx("console")#idx("operator", "communicating with")
The operator console is the one place every MVS program can reach. On
MVS 3.8j it is also the system log, so a program should write there only
what an operator needs to see, and say it in one line. The functions are
in #cmd("<mvs/wto.h>") and #cmd("<mvs/console.h>"), and none of them needs
authorization.

=== Writing a Message

#idx("wtof")
#cmd("wtof()") formats a message as #cmd("printf()") does and writes it with
WTO. Begin each message with a message identifier of your own, so that the
operator, and any program that watches the console, can tell your messages
apart:

```
wtof("DEMO001I %u RECORDS COPIED FROM %s", count, dsn);
```

One message holds at most 124 characters\; #cmd("wtof()") splits a longer
text, and writes one message for each line of a text that contains
newlines. Messages written to the console reach the job log at once, which
makes #cmd("wtof()") the right tool for tracing a program that may end
abnormally: output of #cmd("printf()") still in a buffer is lost in an abend
(@pg-errors).

=== Asking for a Reply

#idx("wtorf")
#cmd("wtorf()") writes a message and waits until the operator replies.
@pg-services-confirm-fig asks before a step that cannot be undone.

#fig(caption: [Asking the operator for a reply])[
  #code(read("../ex/pg-services/confirm.c"), numbers: true)
] <pg-services-confirm-fig>

Three rules apply. The reply is not terminated with a null character, so
clear the buffer and pass a maximum one less than its size (lines 9 and
10). End the format string without a newline: a text that ends with one is
written as ordinary messages, and no reply is requested. And the task waits
for as long as the operator takes\; there is no time limit, so do not ask
for a reply in a task that something else is waiting for.

=== Accepting MODIFY and STOP Commands <pg-services-modify>

#idx("MODIFY command")#idx("STOP command")#idx("started task")
#idx("__gtcom")#idx("__cibget")#idx("__cibdel")
A long-running program, usually a started task, takes its orders from the
operator: #cmd("F DEMO,")#var("text") passes it a command, #cmd("P DEMO")
asks it to end. MVS queues each command as a command input buffer (CIB) and
posts an ECB, whose address #cmd("__gtcom()") returns. The program waits on
that ECB, reads the queued CIBs with #cmd("__cibget()") and removes each one
with #cmd("__cibdel()").

Most such programs also have work of their own to do. @pg-services-stc-fig
does some every minute, and also on the command #cmd("F DEMO,RUN"):

+ Call #cmd("__gtcom()") and test the result before anything else (line
  22): #cmd("__cibset()") and #cmd("__cibdel()") do not check it.
+ Remove the CIB of the #cmd("START") command, which a started task finds
  queued when it begins (lines 24 and 25).
+ Build a list of two ECBs: the command ECB, and a timer ECB of your own,
  whose address carries the high-order bit that marks the last entry of the
  list (lines 28 and 29).
+ Wait on the list with #cmd("ecb_timed_waitlist()") (line 33). When the
  interval ends, the library posts the timer ECB with the post code you
  give, which ends the wait. A negative return means that the interval
  could not be set and nothing was waited for\; do not simply wait again.
+ After the wait, look at both: clear the timer ECB if it was posted (lines
  37 and 38), and process every queued CIB (line 41). The last
  #cmd("__cibdel()") clears the command ECB for the next wait.

#fig(caption: [A started task that works on a timer and on commands])[
  #code(read("../ex/pg-services/stc.c"), numbers: true)
] <pg-services-stc-fig>

The timeout is posted into the timer ECB, never into the command ECB: a
timed wait on an ECB that has a meaning of its own would post it and fake
the event. Waits and timers are explained in @pg-tasks.

The command text in #cmd("cibdata") is not terminated, so copy it with its
length, #cmd("cibdatln"), as the example does. A started task needs a
procedure in a procedure library of the system (@pg-services-stc-jcl).
Start it with #cmd("S DEMO"), then try #cmd("F DEMO,RUN") and #cmd("P DEMO").

#fig(caption: [A procedure for the started task])[
  #code(read("../ex/pg-services/stc.jcl"))
] <pg-services-stc-jcl>

#fig(caption: [The console during S DEMO, F DEMO,RUN and P DEMO])[
  _Output to be captured on MVS._
] <pg-services-stc-out>

// -------------------------------------------------------------------------
== Writing SMF Records <pg-services-smf>

#idx("SMF", "writing a record")
#idx("smf_write")#idx("smf_init")
SMF collects accounting records, and record types 128 to 255 are left to
installations. A program that wants its work accounted for writes a record
of such a type with the functions of #cmd("<mvs/smf.h>"):

+ Define the record as a structure that begins with an
  #cmd("SMF_HEADER"), 18 bytes, followed by your data.
+ Clear the whole record, then let #cmd("smf_init()") fill in the header:
  the length, the type, the time, the date and the system identifier.
+ Fill in your data and write the record with #cmd("smf_write()").

#fig(caption: [Writing an SMF record])[
  #code(read("../ex/pg-services/smfrec.c"), numbers: true)
] <pg-services-smf-fig>

#cmd("smf_write()") needs APF authorization (@pg-services-auth), so the
example tests for it first with #cmd("__isauth()") (line 21): an
unauthorized call would end the program with abend S047. It also tests
#cmd("smf_active()"), although #cmd("smf_write()") makes the same test and
returns -1. A result of 1 says only that SMF records something\; whether
user records are written is decided by the SMF options of the system, set
at IPL by the member #cmd("SMFPRMxx") of #cmd("SYS1.PARMLIB").

#idx("SMF", "record layout")
*Lay out the record deliberately.* A program that reads the record later,
from the SMF data set, sees the bytes, not your structure. The compiler
aligns an #cmd("unsigned") on a fullword boundary: after the 18-byte header
and an 8-byte name it leaves a gap of two bytes before #cmd("records"),
whether you declare the gap or not. Declare it (#cmd("rsvd") in the
example), and write the offsets beside the fields, so that the layout a
reader works from is the layout that is written.

// -------------------------------------------------------------------------
== Working with JES2 <pg-services-jes2>

#idx("JES2", "submitting a job")#idx("internal reader")
#idx("JES2", "job queue")
Two tasks with JES2 need no authorization: submitting a job through the
internal reader, and reading the job queue to see what became of it. Both
are in #cmd("<mvs/jes2.h>"). Cancelling a job or deleting its output goes
through the subsystem interface and needs an authorized program
(@pg-services-auth). @pg-services-submit-fig submits a job and waits until
it has run.

*Submitting.* #cmd("jesiropn()") allocates and opens the internal reader,
#cmd("jesirput()") writes one JCL statement, padded to 80 columns, and
#cmd("jesircl2()") ends the job and returns its job identifier, such as
#cmd("JOB01234"). The identifier has eight characters and no null
character: the function #cmd("submit()") copies it into a string (lines 25
and 26). #cmd("jesirput()") refuses an empty string\; write a blank
statement as one blank.

*Following the job.* #cmd("jesopen()") reads the JES2 checkpoint data set
into storage and returns a handle\; #cmd("jesjob()") then selects jobs from
that copy. The copy is not updated: a job submitted after the open is not in
it, and a job in it does not move on. To see a change, open a new handle,
as #cmd("ended()") does on every call (line 33), and close it again: the
copy of the queue takes tens of kilobytes. A job has run when it is on the
output queue (#cmd("_OUTPUT")) or being printed (#cmd("_HARDCPY"))\; its
#cmd("completion") then holds X'77' in the high-order byte, the abend code
in the next twelve bits and the highest condition code in the low-order
twelve.

#fig(caption: [Submitting a job and waiting for it to end])[
  #code(read("../ex/pg-services/submit.c"), numbers: true)
] <pg-services-submit-fig>

The job queue is read from the checkpoint and spool data sets directly,
and the step needs DD statements for both (@pg-services-jes2-jcl).

#fig(caption: [DD statements for reading the job queue])[
  #code(read("../ex/pg-services/jes2.jcl"))
] <pg-services-jes2-jcl>

Points to watch:

- *Filters.* A job name or job identifier pattern for #cmd("jesjob()") may
  have at most 11 characters. A longer one is not cut but overwrites
  storage, so check the length of a pattern that comes from outside the
  program.
- *A job that disappears.* #cmd("jesjob()") returns #cmd("NULL") when no job
  matches, and also when it fails. A job whose output was purged right after
  it ran is no longer in the queue at all, so a program that waits for a job
  must also give up after a while, as the example does.
- *Reading the output.* #cmd("jesprint()") passes the lines of one output
  data set of the job to a function of yours. Its return value does not say
  whether the data set was read in full: pass a #cmd("JESPRST") and look at
  its #cmd("reason"). The _libc370 Library Reference_ lists the reasons in
  Chapter 33, “Subsystems, JES2 and ISPF”.
- *The pause.* The example waits two seconds between looks, with a timed
  wait on an ECB that nothing else posts (line 67), and stops waiting if
  the timer cannot be set.

#fig(caption: [Output of the submit program])[
  _Output to be captured on MVS._
] <pg-services-submit-out>
