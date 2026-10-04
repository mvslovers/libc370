#import "../bookmaster/bookmaster.typ": *

// Narrow cells: ragged right.
#show table: set par(justify: false)

= Files and I/O <pg-io>

#idx("input and output")#idx("stream")
A C program reads and writes files through the functions of
#cmd("<stdio.h>"), on MVS as anywhere else. What is different is the file:
not a sequence of bytes in a directory, but a data set made of records,
reached through a DD statement, with a record format and a record length
that the C program does not choose. This chapter shows how to read and
write data sets, members of partitioned data sets and SYSOUT with the
standard functions, how lines become records and records become lines, how
to create a new data set, and what to do when a data set runs out of space.
The _libc370 Library Reference_ describes each function and lists every
option; this chapter shows which ones to use for a task.

== How a Stream Reaches a Data Set <pg-io-model>

#idx("DD statement", "and streams")#idx("BSAM")
Every stream is a data control block (DCB) opened on one DD statement. The
DD either exists before the stream is opened -- it was coded in the JCL of
the step, or the program allocated it -- or #cmd("fopen()") allocates it
itself by dynamic allocation and frees it again at #cmd("fclose()"). The
data set is read and written with BSAM, one record at a time; tape is
read and written with EXCP. VSAM data sets cannot be opened as streams.

The first argument of #cmd("fopen()") says which DD to use or how to
allocate one. @pg-io-names-tab shows the forms; letters are converted to
uppercase in all of them.

#tab(caption: [File names for fopen()])[
  #table(columns: (1.75in, 1fr),
    [Name], [Opens],
    [#cmd("DD:")#var("ddname")], [the DD #var("ddname") of the step, which
      must exist.],
    [#cmd("DD:")#var("ddname")#cmd("(")#var("member")#cmd(")")], [a member
      of the library allocated to #var("ddname").],
    [#cmd("'")#var("dsname")#cmd("'")], [the cataloged data set
      #var("dsname"), allocated by #cmd("fopen()").],
    [#cmd("'")#var("dsname")#cmd("(")#var("member")#cmd(")'")], [a member
      of the cataloged library #var("dsname").],
    [#var("dsname")], [as above; under TSO the user's prefix is put in
      front of the name.],
    [#cmd("&&")#var("name")], [a temporary data set, on VIO.],
    [#cmd("*")], [a new SYSOUT data set, or under TSO the terminal.],
    [#cmd("*")#var("ddname")], [the DD #var("ddname") if it exists,
      otherwise as #cmd("*").],
  )
] <pg-io-names-tab>

#idx("file name", "DD or data set name")
Which form to use is a question of who decides where the data lives:

- With #cmd("DD:") names, the JCL decides. The program can be run against
  another data set, a member, SYSOUT, a #cmd("DUMMY") DD or in-stream data
  without a change, and the job's #cmd("DISP") parameters say what happens
  to the data set at the end of the step. This is the normal choice for a
  batch program.
- With data set names, the program decides. Use them when the name is only
  known while the program runs, such as a name given as a parameter or
  built from the date, or under TSO, where there is no JCL.

A data set that #cmd("fopen()") allocates by name is allocated with a
disposition that follows from the mode: #cmd("DISP=SHR") for reading,
#cmd("DISP=OLD") for writing an existing data set -- which holds it
exclusively until #cmd("fclose()") -- and #cmd("DISP=(NEW,CATLG)") for a
new one. A member is always allocated #cmd("DISP=SHR"). Each stream
occupies a DD while it is open, and the number of DDs of a step is limited
by MVS, not by the library: close streams that are no longer needed.

== Reading a Data Set <pg-io-read>

#idx("data set", "reading")#idx("fgets", "buffer size")
To read a data set line by line:

+ Code a DD statement for it in the JCL, here #cmd("INPUT").
+ Open it with #cmd("fopen(\"DD:INPUT\", \"r\")"). Test the result for
  #cmd("NULL").
+ Read with #cmd("fgets()") into a buffer of at least the record length
  plus 2 bytes: one for the #cmd("'\\n'") that ends each line, one for the
  null character. With a smaller buffer, a record comes back in pieces.
+ Remove the #cmd("'\\n'") and, for fixed-length records, the trailing
  blanks, if the program does not want them.
+ When #cmd("fgets()") returns #cmd("NULL"), test #cmd("ferror()") to tell
  the end of the data from an error.

@pg-io-count-fig follows these steps for records of up to 256 bytes, and
@pg-io-count-jcl runs it against a member of a source library.

#fig(caption: [Counting the lines of a data set])[
  #code(read("../ex/pg-io/count.c"), numbers: true)
] <pg-io-count-fig>

#fig(caption: [Reading a member through a DD statement])[
  #code(read("../ex/pg-io/count.jcl"))
] <pg-io-count-jcl>

#idx("scanf", "field width")
To take values out of a line, convert it with #cmd("sscanf()") after
#cmd("fgets()"). The #cmd("scanf()") functions of the library accept no
field width, so a #cmd("%s") stores as many characters as the input has:
make every array that #cmd("sscanf()") fills as long as the line buffer,
and no line can overrun it.

== Records and Lines <pg-io-records>

#idx("record format")#idx("text stream")#idx("binary stream")
#idx("record stream")
A stream is opened in one of three ways, and the way decides how the
records of the data set are turned into the bytes the program sees:

- A *text stream*, mode #cmd("\"r\""), #cmd("\"w\"") or #cmd("\"a\""),
  treats each record as a line.
- A *binary stream*, the same modes with #cmd("b"), passes the data of the
  records with nothing added and nothing removed.
- A *record stream*, with the option #cmd("record") in the mode, as in
  #cmd("\"rb,record\""), transfers one record with each #cmd("fread()") or
  #cmd("fwrite()").

The record format comes from the data set, from the DD statement, or for
a new data set from the mode string (@pg-io-new). @pg-io-records-tab
summarizes what each combination does.

#tab(caption: [How records become bytes])[
  #table(columns: (0.7in, 1fr, 1fr),
    [Stream], [Reading a record], [Writing],
    [text], [Its data and a #cmd("'\\n'"). A fixed-length record keeps its
      trailing blanks. A variable-length record comes without its record
      descriptor word (RDW).],
    [#cmd("'\\n'") ends the record. A fixed-length record is filled with
      blanks to its length. A line longer than a record continues in the
      next record. An empty line is a record of blanks (F), a record
      without data (V) or one blank (U).],
    [binary], [Its data, without the RDW, directly after the data of the
      record before.], [A record is written each time the buffer holds a full
      record: #cmd("LRECL") bytes (F), #cmd("LRECL") - 4 bytes (V), the
      block size (U). The rest goes out at #cmd("fclose()") as a short
      record; a fixed-length one is filled with X'00'.],
    [record], [One #cmd("fread()") returns one record. A variable-length
      record begins with its RDW.], [One #cmd("fwrite()") writes one
      record. A variable-length record must begin with a correct RDW.],
  )
] <pg-io-records-tab>

Some details of text streams catch a program that was written for a byte
stream:

- *Trailing blanks are data.* A fixed-length record of 80 bytes is read as
  80 characters and a #cmd("'\\n'"), however short the text in it. A
  program that compares lines, or writes them somewhere else, removes the
  blanks first, as @pg-io-count-fig does.
- *Two bytes end a line early.* A byte X'00' in a record ends the line at
  that point and the rest of the record is not returned. A byte X'15', the
  value of #cmd("'\\n'") (@pg-charset), is read as the end of a line. Data
  that may contain either is read as a binary or record stream.
- *Carriage control is the program's business.* On a data set with
  #cmd("RECFM=FBA") or #cmd("VBA"), the first character of every line is
  the control character, and the program writes it: #cmd("'1'") for a new
  page, a blank for the next line. The library neither adds nor removes it.
- *#cmd("fflush()") ends the record.* A line written in two parts with a
  #cmd("fflush()") between them becomes two records.

#idx("RDW")#idx("record descriptor word")
The record descriptor word of a variable-length record is four bytes:
bytes 0 and 1 give the length of the record, the four bytes included, and
bytes 2 and 3 are zero. On a record stream it is part of the data in both
directions. @pg-io-vbcopy-fig copies a data set with variable-length
records, record by record, using the RDW to find the length of each one;
@pg-io-vbcopy-jcl gives the output data set its attributes in the DD
statement.

#fig(caption: [Copying variable-length records with a record stream])[
  #code(read("../ex/pg-io/vbcopy.c"), numbers: true)
] <pg-io-vbcopy-fig>

#fig(caption: [JCL for the record copy])[
  #code(read("../ex/pg-io/vbcopy.jcl"))
] <pg-io-vbcopy-jcl>

The character functions -- #cmd("fgets()"), #cmd("fputs()"),
#cmd("fprintf()") and the others -- do nothing on a record stream, and a
record stream cannot be opened with #cmd("+").

== Creating a Data Set <pg-io-new>

#idx("data set", "creating")#idx("open mode", "options")
#cmd("fopen()") creates a data set when a name given as a data set name
does not exist and the mode writes: #cmd("\"w\""), #cmd("\"a\""),
#cmd("\"w+\"") or #cmd("\"a+\""). The new data set is sequential and
cataloged. Its attributes come from options written after the mode,
each introduced by a comma:

#tab(caption: [Mode options for a new data set])[
  #table(columns: (1.6in, 1fr),
    [Option], [Gives],
    [#cmd("recfm=")#var("f")], [the record format: #cmd("f"), #cmd("fb"),
      #cmd("v"), #cmd("vb"), #cmd("u") and so on.],
    [#cmd("lrecl=")#var("n")], [the record length.],
    [#cmd("blksize=")#var("n")], [the block size; used only with a blocked
      record format.],
    [#cmd("space=")#var("unit")#cmd("(")#var("p")#cmd(",")#var("s")#cmd(")")],
      [primary and secondary space, in #cmd("trk"), #cmd("cyl") or blocks
      of a given length.],
    [#cmd("rlse")], [release the unused space at #cmd("fclose()").],
    [#cmd("unit=")#var("name"), #cmd("volser=")#var("vol")], [the unit and
      the volume.],
  )
] <pg-io-options-tab>

An option that is not given is taken from an environment variable --
#cmd("DATASET_RECFM"), #cmd("DATASET_LRECL"), #cmd("DATASET_BLKSIZE"),
#cmd("DATASET_SPACE"), #cmd("DATASET_UNIT"), #cmd("DATASET_VOLSER") --
which can be set in the #cmd("SYSENV") DD (@pg-startup-env), and without
one from the defaults of the library: #cmd("RECFM=V") with
#cmd("LRECL=255"), and the installation's default space, unit and volume.
The attributes apply only when the data set is created. An existing data
set keeps its own, whatever the mode string says.

To create a data set:

+ Choose the record format and length for what the data set will hold.
  Records that other MVS programs read, such as control statements or
  source, are usually #cmd("RECFM=FB") with #cmd("LRECL=80").
+ Choose a block size that is a multiple of the record length, and a
  space that holds the data with room to spare.
+ Open the data set by name, with the options in the mode string.
+ Write the data.
+ Close it with #cmd("fclose()") and test the result: the last block is
  written at that point, and a data set that runs out of space on it says
  so there and nowhere else (@pg-io-errors).

@pg-io-newds-fig copies its #cmd("SYSIN") into a new data set whose name
it takes from the parameter. Run as in @pg-io-newds-jcl it creates
#cmd("MYUSER.CONTROL.CARDS") with two records.

#fig(caption: [Creating a data set with fixed-length records])[
  #code(read("../ex/pg-io/newds.c"), numbers: true)
] <pg-io-newds-fig>

#fig(caption: [JCL for NEWDS])[
  #code(read("../ex/pg-io/newds.jcl"))
] <pg-io-newds-jcl>

The name is put between apostrophes, so that it is taken as it is. Without
them, under TSO, the user's prefix would be put in front of it. If the data
set exists already, #cmd("fopen()") opens it #cmd("DISP=OLD") and
overwrites it, with its own attributes.

#idx("temporary data set")#idx("tmpnam")
A temporary data set, for data that lives only while the program runs, is
opened with a name from #cmd("tmpnam()"), which returns names of the form
#cmd("&&TMP")#var("nnnnn"). Open it #cmd("\"wb+\"") to write it and read
it back. #cmd("tmpfile()") is no substitute: the stream it returns can
only be written. MVS deletes a temporary data set at the end of the step at
the latest.

== Writing a Member of a Library <pg-io-member>

#idx("partitioned data set", "member")#idx("member", "writing")
A member of a partitioned data set is opened by naming it:
#cmd("DD:")#var("ddname")#cmd("(")#var("member")#cmd(")") with a DD for the
library, or #cmd("'")#var("dsname")#cmd("(")#var("member")#cmd(")'"). The
library must exist; #cmd("fopen()") does not create one. The member is
written with the attributes of the library.

- Mode #cmd("\"w\"") creates the member, or replaces it when it exists.
- Mode #cmd("\"a\"") creates a member that does not exist, but cannot add
  to one that does: #cmd("fopen()") fails with #cmd("errno")
  #cmd("EOPNOTSUPP") and leaves the member unchanged. To add records to a
  member, read it and write it again.
- A member cannot be updated in place with #cmd("\"r+\"").

#fig(caption: [Writing a member])[
  #code(read("../ex/pg-io/member.c"), numbers: true)
] <pg-io-member-fig>

#fig(caption: [JCL for MEMBER])[
  #code(read("../ex/pg-io/member.jcl"))
] <pg-io-member-jcl>

@pg-io-member-fig, run as in @pg-io-member-jcl, writes the member
#cmd("JAN") of #cmd("MYUSER.REPORTS"). The library is allocated
#cmd("DISP=SHR"), which lets other jobs read it while the member is
written.

#idx("remove")#idx("rename")
#cmd("remove()") and #cmd("rename()") delete and rename members as well as
data sets. Their names are written differently from those of
#cmd("fopen()"): there is no #cmd("DD:") form, no prefix is added, and a
name without apostrophes is taken as fully qualified.
#cmd("remove(\"MYUSER.REPORTS(JAN)\")") deletes the member. They return 0
on success; otherwise the return code of the service they used -- #cmd("STOW")
for a member, IDCAMS for a data set -- or a negative value when the data set
could not be allocated or opened.

== The Standard Streams <pg-io-std>

#idx("stdout")#idx("stderr")#idx("stdin")
#idx("SYSPRINT")#idx("SYSTERM")#idx("SYSIN")
The start-up opens three streams before #cmd("main()") is called:

#tab(caption: [The standard streams])[
  #table(columns: (0.75in, 0.95in, 1fr),
    [Stream], [DD], [Without the DD],
    [#cmd("stdout")], [#cmd("SYSPRINT")], [a SYSOUT data set of the job's
      message class; under TSO, the terminal.],
    [#cmd("stderr")], [#cmd("SYSTERM")], [as for #cmd("stdout").],
    [#cmd("stdin")], [#cmd("SYSIN")], [an empty input: the first read
      returns end of file.],
  )
] <pg-io-std-tab>

The streams are text streams. A #cmd("SYSPRINT") or #cmd("SYSTERM") DD
without DCB parameters, such as #cmd("SYSOUT=*"), is written with
variable-length blocked records, so a line is a record of its own length.
An in-stream #cmd("SYSIN DD *") has fixed-length records of 80 bytes, which
#cmd("stdin") returns with their trailing blanks.

#idx("freopen")
To send a standard stream somewhere else for the rest of the run, use
#cmd("freopen()"). It opens the new file first and closes the old one only
when that succeeded. The close writes the last block of the old file, and
a failure there does not make #cmd("freopen()") fail: it only sets
#cmd("errno"). Set #cmd("errno") to 0 before the call and test it after, as
in @pg-io-redir-fig, when the end of the old output matters.

#fig(caption: [Redirecting stdout])[
  #code(read("../ex/pg-io/redir.c"), numbers: true)
] <pg-io-redir-fig>

#fig(caption: [JCL for REDIR])[
  #code(read("../ex/pg-io/redir.jcl"))
] <pg-io-redir-jcl>

#idx("TSO", "terminal I/O")
Under TSO, #cmd("stdout") and #cmd("stderr") are the terminal unless
#cmd("SYSPRINT") and #cmd("SYSTERM") are allocated to something else.
#cmd("stdin") is the terminal when #cmd("SYSIN") is allocated to it, as the
logon procedures of many systems do; without a #cmd("SYSIN") DD it is empty,
as in batch. A line is written to the terminal with #cmd("TPUT") and read
with #cmd("TGET"). Leading and trailing blanks of an
input line are removed. TSO has no end-of-file key, so a line that consists
of #cmd("/*"), #cmd("//"), #cmd("<eof>") or #cmd("<EOF>") alone is taken as
the end of the input. @pg-tso describes programs under TSO in more detail.

== Positioning <pg-io-position>

#idx("file position")#idx("fseek")#idx("ftell")
#cmd("ftell()") and #cmd("fseek()") count the bytes the program would read
from the beginning of the data set: on a text stream the data of each
record and one byte for its #cmd("'\\n'"). A record of a fixed-length data
set counts its full length, blanks included. On a record stream the
position is a record number.

BSAM cannot go to a byte of a sequential data set, so the library gets
there by reading:

- Forward, it reads up to the position.
- Backward, on an input stream, it closes the data set, opens it again and
  reads from the beginning. The time this takes grows with the position.
- An output stream opened #cmd("\"w\"") or #cmd("\"a\"") is always at its
  end. #cmd("fseek()") to any other position fails with #cmd("ESPIPE").

So a program that moves around in a data set pays for every move. Read the
data set once, keeping what is needed, rather than going back to it; if it
has to go back, #cmd("rewind()") and read forward.

#idx("update mode")
The #cmd("+") modes open a data set on disk for reading and writing. A
write in the middle replaces data in place, and a record never changes its
length: a write that would make a record longer -- or, on variable-length
and undefined records, shorter -- fails with #cmd("EOPNOTSUPP"). The DCB
is open in one direction at a time, and every change between reading and
writing closes and opens it again. Updating in place is therefore for a
few changes to records of fixed length. To change many records, or their
length, read the data set and write a new one.

== Errors and Full Data Sets <pg-io-errors>

#idx("ENOSPC")#idx("x37 abend")#idx("fclose", "testing the result")
#idx("block", "written at close")
A stream has a buffer of one record. A record goes to BSAM when the line
ends; BSAM collects records into blocks and writes a block when it is full.
The last block of a data set, usually a short one, is written when the data
set is closed. Three rules follow:

+ *Test the result of #cmd("fclose()")* on every output stream whose
  contents matter. A data set that runs out of space on its last block
  reports it there and nowhere else: #cmd("fclose()") returns #cmd("EOF")
  with #cmd("errno") #cmd("ENOSPC"). #cmd("fflush()") does not write the
  block and cannot find out earlier.
+ *A full data set is an error, not an abend.* When a write runs out of
  space, the library does not let MVS end the program with an x37 abend:
  the write fails, the error indicator of the stream is set and
  #cmd("errno") is #cmd("ENOSPC")\; any other permanent I/O error gives
  #cmd("EIO"). Every later read or write on the stream fails at once in the
  same way until the program calls #cmd("clearerr()").
+ *The return value of #cmd("printf()") does not report a write error.* It
  counts the characters formatted. Test #cmd("ferror()") instead, at the
  latest before #cmd("fclose()").

#idx("errno", "after fopen")
When #cmd("fopen()") fails, #cmd("errno") tells the reason only in a few
cases: #cmd("ENOMEM") when there is not enough storage for the buffers,
#cmd("EINVAL") for a #cmd("+") mode on a device other than disk,
#cmd("EOPNOTSUPP") for #cmd("\"a\"") on an existing member, #cmd("EBUSY")
for an in-stream data set that is already open. For every other failure --
a missing DD, a data set that is not cataloged, a refused allocation --
#cmd("errno") has no meaning and may hold a value from an earlier call.
Report the name that could not be opened rather than #cmd("errno"), as the
examples in this chapter do.

#idx("abend", "and open streams")
When a program abends, no stream is closed. The records still in the
buffers, among them the last block of every output data set, are lost,
whether or not #cmd("fflush()") was called. A program that may abend and
needs a trace of how far it came writes it to the console (@pg-errors).

#idx("__fabandon")
A program that catches an x37 abend with its own recovery routine is left
with a stream that looks healthy to the library, and #cmd("fclose()") would
drive the failing write again and abend inside the close. Close such a
stream with #cmd("__fabandon()") from #cmd("<mvs/file.h>"), which discards
the buffer and closes it under protection. It is needed only after an
abend; a stream that reported #cmd("ENOSPC") is closed with
#cmd("fclose()").

== Beyond Streams <pg-io-beyond>

#idx("record input and output")#idx("ropen")
Streams cover most work with sequential data. For the rest, the library has
interfaces of its own, described in the _libc370 Library Reference_:

#deflist(width: 1.6in,
  [Record I/O], [#cmd("ropen()"), #cmd("rread()"), #cmd("rwrite()") and
    #cmd("rclose()") in #cmd("<mvs/rfile.h>") read and write one logical
    record at a time, with nothing added, removed or translated, and report
    the length of each record read. Chapter “Data Sets and DD Statements”.],
  [BSAM, BDAM and EXCP], [#cmd("<mvs/osio.h>") reads and writes one block
    at a time with these access methods, without the buffering of the
    stream functions. Chapter “Record I/O”.],
  [VSAM], [#cmd("<mvs/vsam.h>") opens key-sequenced, entry-sequenced and
    relative-record clusters. Chapter “VSAM”.],
  [Dynamic allocation], [#cmd("__dsalc()") and #cmd("__dsfree()") in
    #cmd("<mvs/dynalloc.h>") allocate a DD from JCL-like keywords, such as
    #cmd("DSN=A.B;DISP=SHR"), which a stream can then open in #cmd("DD:")
    form. Chapter “Dynamic Allocation
    and IDCAMS”.],
  [Catalog and directory], [#cmd("<mvs/dslist.h>") lists the data sets of a
    catalog level and the members of a library; #cmd("<mvs/pds.h>") reads
    and changes the directory of a library. Chapter “Data Sets and DD
    Statements”.],
)

@pg-services describes the other MVS services of the library.
