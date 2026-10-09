#import "../bookmaster/bookmaster.typ": *

= \<stdio.h\> — Input and Output <std-stdio>

#idx("stdio.h")
#idx("stream")
The header #cmd("<stdio.h>") declares the functions that read and write
streams, the type #cmd("FILE") that describes a stream, and the functions
that format and scan text. On MVS a stream is a data set, a member of a
partitioned data set, a SYSOUT data set or the TSO terminal, reached
through a DD statement. This introduction describes how a file name
becomes a DD and a data set, what the open modes mean for the record
formats of MVS, and which conversions the formatted input and output
functions support. The function entries follow it.

== Streams, DDs and Data Sets <std-stdio-streams>

#idx("DD statement", "and streams")
#idx("BSAM")
Every open stream is a DCB opened on one DD. The DD either exists already
-- it was coded in the JCL of the step or allocated by the program -- or
#cmd("fopen()") allocates it by dynamic allocation (SVC 99) and frees it
again when the stream is closed. The data set is read and written with
BSAM, one record at a time, on tape as well\; only a tape stream opened
with the option #cmd("bsam") and a block size above 32760 is read and
written with EXCP. VSAM data sets cannot be opened as streams.

The library converts between records and the byte stream that a C
program sees. How it does so depends on the record format of the data set
and on whether the stream is a text stream, a binary stream or a record
stream; see @std-stdio-records.

Each stream carries a lock. Every function that reads or writes a stream
holds it for the length of the call, so a line written by one
#cmd("printf()") or #cmd("puts()") is not split by a write from another task
on the same stream. The lock is skipped only when no other task can reach
the stream: when the calling task has no subtask, and no task above it in
the job step runs C code. A program without threads therefore takes no
lock, and neither does one whose threads have all ended and been deleted.
A thread, a subtask that the program attaches itself, and a C module with a
start-up of its own that is linked to on a thread all take it. (For a
subtask that the program attaches itself this follows from the library
source and has not been verified on MVS.)

At normal program end -- return from #cmd("main()"), #cmd("exit()") or
#cmd("_Exit()") -- every stream that is still open is closed. When the
program abends, nothing is closed: the records still in the buffers, among
them the last block of every output data set, are lost, whether or not
#cmd("fflush()") was called. A program that may abend should report
its progress through the console functions (@mvs-console) rather than
through #cmd("stdout").

== File Names <std-stdio-names>

#idx("file name")
#idx("DD:", "file name")
#idx("data set name", "in fopen")
The #var("filename") argument of #cmd("fopen()") and #cmd("freopen()")
has one of the forms in @std-stdio-names-tab. Letters are converted to
uppercase in all of them. A DD name and a member name are cut to 8
characters, a data set name to 44.

#tab(caption: [File names accepted by fopen()])[
  #table(columns: (1.65in, 1fr),
    [Form], [Meaning],
    [#cmd("DD:")#var("ddname")], [The DD #var("ddname") of the step. The
      prefix may be written in lowercase, #cmd("dd:"). The DD must exist.],
    [#cmd("DD:")#var("ddname")#cmd("(")#var("member")#cmd(")")], [Member
      #var("member") of the partitioned data set allocated to
      #var("ddname").],
    [#cmd("'")#var("dsname")#cmd("'")], [The cataloged data set
      #var("dsname"), fully qualified. The closing quote may be left out.],
    [#cmd("'")#var("dsname")#cmd("(")#var("member")#cmd(")'")], [Member
      #var("member") of the partitioned data set #var("dsname").],
    [#var("dsname")], [Without quotes. When the program runs as a TSO
      command processor, the TSO prefix of the user and a period are put in
      front\; in batch, and under TSO by #cmd("CALL"), the name is used as
      it is written.],
    [#cmd("&&")#var("name")], [A temporary data set, allocated on VIO when
      it is opened for output. #cmd("tmpnam()") returns names of this
      form.],
    [#cmd("*")], [A SYSOUT data set, or under TSO the terminal, allocated
      when the stream is opened.],
    [#cmd("*")#var("ddname")], [The DD #var("ddname") if it exists;
      otherwise the same as #cmd("*").],
    [#cmd("*PUTLINE")], [The output of the TSO terminal monitor program,
      written with PUTLINE: under #cmd("IKJEFT01") in batch into
      #cmd("SYSTSPRT"), in a TSO session to the terminal\; whether TSO/E
      #cmd("OUTTRAP") traps it there has not been measured. For writing\;
      #cmd("*GETLINE") reads. The name is reserved and never opens a DD
      called #cmd("PUTLINE").],
    [#cmd("*GETLINE")], [The input of the TSO terminal monitor program,
      read with GETLINE: under #cmd("IKJEFT01") in batch the next line of
      #cmd("SYSTSIN"), in a TSO session the terminal.
      For reading only. The name is reserved and never opens a DD called
      #cmd("GETLINE").],
  )
] <std-stdio-names-tab>

The name #cmd("'NULLFILE'") is a dummy data set: reading it returns end
of file at once, and what is written to it is discarded.

The names that #cmd("remove()") and #cmd("rename()") accept are different;
see their entries.

=== How a Data Set Name Is Allocated <std-stdio-alloc>

#idx("dynamic allocation", "by fopen")
#idx("DISP", "chosen by fopen")
A data set name is allocated by dynamic allocation with a disposition
that depends on the mode:

- For reading (#cmd("\"r\"")), the data set is allocated
  #cmd("DISP=SHR"). It must exist and be cataloged.
- For writing (#cmd("\"w\""), #cmd("\"a\"") and the three
  #cmd("+") modes), the library tries, in this order, until one of them
  succeeds: a temporary data set (#cmd("&&") names only) with
  #cmd("DISP=(NEW,CATLG)") and #cmd("UNIT=VIO"); #cmd("DISP=SHR") when a member is named;
  #cmd("DISP=OLD")\; and finally #cmd("DISP=(NEW,CATLG)"), which creates
  the data set. #cmd("\"r+\"") never creates a data set.

A data set that the library creates is sequential (#cmd("DSORG=PS")).
Its attributes come from the options of the mode string (see
@std-stdio-options), then from environment variables, then from the
defaults of @std-stdio-defaults. They are used only when the data set is
created; an existing data set keeps its own. A member can be written only
into a partitioned data set that already exists.

#tab(caption: [Attributes of data sets that fopen() allocates])[
  #table(columns: (1.15in, 1.9in, 1fr),
    [Data set], [Environment variables], [Defaults],
    [new data set], [#cmd("DATASET_RECFM"), #cmd("DATASET_LRECL"),
      #cmd("DATASET_BLKSIZE"), #cmd("DATASET_SPACE"), #cmd("DATASET_UNIT"),
      #cmd("DATASET_VOLSER")], [#cmd("RECFM=V"), #cmd("LRECL=255"), the
      installation's default space, unit and volume],
    [#cmd("&&") temporary], [#cmd("TEMP_RECFM"), #cmd("TEMP_LRECL"),
      #cmd("TEMP_SPACE")], [#cmd("RECFM=V"),
      #cmd("LRECL=255"), #cmd("UNIT=VIO")],
    [SYSOUT (#cmd("*"))], [#cmd("SYSOUT_CLASS"), #cmd("SYSOUT_RECFM"),
      #cmd("SYSOUT_LRECL"), #cmd("SYSOUT_BLKSIZE")], [the job's message
      class, #cmd("RECFM=V"), #cmd("LRECL=255")],
    [TSO terminal (#cmd("*"))], [#cmd("TERMINAL_RECFM"),
      #cmd("TERMINAL_LRECL"), #cmd("TERMINAL_BLKSIZE")], [#cmd("RECFM=V"),
      #cmd("LRECL=4000")],
  )
] <std-stdio-defaults>

A block size is passed only when the record format contains #cmd("B").
The environment variables are read with #cmd("getenv()"), so they can be
set in the #cmd("SYSENV") DD of the step (see @std-stdlib). The standard
streams are opened before #cmd("SYSENV") is read, and do not see them.

#note[A DD named in #cmd("DD:") form is not allocated by the library and
is not freed when the stream is closed. A DD that #cmd("fopen()")
allocated is freed by #cmd("fclose()").]

== Open Modes <std-stdio-modes>

#idx("open mode")
#idx("mode string")
The #var("mode") argument of #cmd("fopen()") and #cmd("freopen()") is one
of #cmd("r"), #cmd("w") and #cmd("a"), optionally followed by #cmd("b"),
#cmd("+") or both, in either order, and then by options, each introduced
by a comma: #cmd("\"r\""), #cmd("\"wb+\""), #cmd("\"w,recfm=fb,lrecl=80\"").
Uppercase and lowercase are the same.

#tab(caption: [Open modes])[
  #table(columns: (0.65in, 1fr),
    [Mode], [Meaning],
    [#cmd("r")], [Read. The data set must exist.],
    [#cmd("w")], [Write. The data set is written from the beginning;
      what it held is replaced. It is created if it does not exist.],
    [#cmd("a")], [Append. The data set is opened #cmd("EXTEND") and written
      after its last record; it is created if it does not exist. On
      SYSOUT and on unit record devices, which cannot be extended,
      #cmd("a") is the same as #cmd("w"). A member that exists cannot be
      appended to: #cmd("fopen()") fails with #cmd("EOPNOTSUPP") and the
      member is not changed. A member that does not exist is created.],
    [#cmd("r+")], [Read and write. The data set must exist; reading starts
      at the beginning.],
    [#cmd("w+")], [Read and write. The data set is emptied, or created.],
    [#cmd("a+")], [Read and write. Writing always takes place at the end;
      the position after #cmd("fopen()") is the end.],
    [#cmd("b")], [Binary stream; see @std-stdio-records.],
  )
] <std-stdio-modes-tab>

#idx("update mode")
The #cmd("+") modes need a direct access data set; on SYSOUT, a terminal
or tape #cmd("fopen()") fails with #cmd("EINVAL"). The DCB is open in one
direction at a time, and the library closes it and opens it again the
other way when the program changes from reading to writing or back. It
does so without a call to #cmd("fseek()") or #cmd("fflush()"), which C
requires between the two and which a portable program writes anyway.
Changing direction costs a close and an open, and changing from writing
to reading reads the data set again up to the position.

A write in the middle of a data set opened #cmd("r+") or #cmd("w+")
replaces data in place, and a record never changes its length. A
#cmd("'\\n'") written where the record ends moves on to the next record; on
a text stream with fixed-length records, a #cmd("'\\n'") inside the record
fills the rest of it with blanks. Any write that would make a record longer
or, on variable-length and undefined records, shorter, fails with
#cmd("EOPNOTSUPP"). A member of a partitioned data set cannot be updated in
place: it can only be written at the end of what was written since it was
opened.

=== Options <std-stdio-options>

#idx("open mode", "options")
A value that is a list is written in parentheses, for example
#cmd("volser=(pub001,pub002)").

#tab(caption: [Options of the mode string])[
  #table(columns: (1.35in, 1fr),
    [Option], [Meaning],
    [#cmd("record")], [Record stream: each #cmd("fread()") and
      #cmd("fwrite()") transfers one record. See @std-stdio-records.],
    [#cmd("rlse")], [Release the unused space of the data set when it is
      closed (#cmd("SPACE=(,,RLSE)")). It applies when the library
      allocates the data set by name, and not to a member.],
    [#cmd("recfm=")#var("f")], [Record format of a data set that is
      created, for example #cmd("fb") or #cmd("vb").],
    [#cmd("lrecl=")#var("n")], [Record length of a data set that is
      created.],
    [#cmd("blksize=")#var("n")], [Block size of a data set that is created;
      used only with a blocked record format.],
    [#cmd("space=")#var("unit")#cmd("(")#var("p")#cmd(",")#var("s")#cmd(")")],
      [Primary and secondary space of a data set that is created.
      #var("unit") is #cmd("trk"), #cmd("cyl") or a block length.],
    [#cmd("unit=")#var("name")], [Unit of a new data set.],
    [#cmd("volser=")#var("vol")], [Volume or volumes of a new data set.],
    [#cmd("mount")], [Wait for the volume to be mounted. Without it, a new
      data set for which #cmd("unit=") or #cmd("volser=") names a volume that
      is not mounted is not allocated, and #cmd("fopen()") fails instead of
      waiting for the operator.],
  )
] <std-stdio-options-tab>

#cmd("unit="), #cmd("volser=") and #cmd("mount") apply to a new cataloged
data set only. A SYSOUT data set (#cmd("*")) takes #cmd("recfm="),
#cmd("lrecl=") and #cmd("blksize=")\; a temporary data set takes
#cmd("recfm="), #cmd("lrecl=") and #cmd("space="), and no block size. Other
text in the mode string is ignored.

Two of these do not work as written, because the mode string is converted
to lowercase before it is examined (a defect, libc370 issue 474, read
from the library source): #cmd("space=cyl(...)") and #cmd("space=trk(...)") for a temporary
data set are taken as a space in blocks, and #cmd("blksize=") for a SYSOUT
data set is never used, since its record format is tested for an uppercase
#cmd("B"). The environment variables #cmd("TEMP_SPACE") and
#cmd("SYSOUT_RECFM"), in uppercase, work.

== Records and Lines <std-stdio-records>

#idx("record format")
#idx("text stream")
#idx("binary stream")
#idx("record stream")
A stream opened without #cmd("b") is a *text stream*, one opened with
#cmd("b") a *binary stream*, and one opened with the option #cmd("record")
a *record stream*. The record format is the one of the data set, of the DD
or of the defaults above. For a DD that gives no record format and no
record length, such as #cmd("SYSOUT=*") without DCB parameters, an output
stream is opened with variable-length blocked records.

=== Text streams

On a text stream a record is a line.

- *Reading.* Each record is returned followed by a #cmd("'\\n'"). The record
  is returned as it is: a fixed-length record of length 80 is read as 80
  characters and #cmd("'\\n'"), trailing blanks included, and a program that
  does not want them removes them itself. Bytes X'00' at the end of a
  record are dropped\; a byte X'00' inside a record is data and is
  returned with the rest of the line.
  The record descriptor word of a variable-length record is not part of
  the line.
- *Writing.* #cmd("'\\n'") ends the record. An empty line is a record of its
  own: on fixed-length records a record of blanks, on variable-length
  records a record with no data, on undefined records one blank. A
  fixed-length record is filled with blanks to its length. A line that is
  longer than a record continues in the next record, without an error.
  #cmd("fflush()") ends the record as well, so a line written in two parts
  with #cmd("fflush()") between them becomes two records.

#idx("newline", "character")
#idx("carriage control")
The newline character #cmd("'\\n'") is X'15'. A byte X'15' in the data of
a record is read as the end of a line. The library neither adds nor
interprets carriage control characters: on a data set with
#cmd("RECFM=FBA") or #cmd("VBA") the first character of each line is the
control character, and the program writes it.

=== Binary streams

A binary stream transfers the data of the records with nothing added and
nothing removed. Reading returns the records one after another, without
their record descriptor words; #cmd("'\\n'") has no special meaning. When
writing, a record is written whenever the buffer holds a full record --
#cmd("LRECL") bytes on fixed-length records, #cmd("LRECL") minus 4 on
variable-length records, the block size on undefined records. At
#cmd("fclose()") the rest is written as a short record; on fixed-length
records it is filled with X'00'.

=== Record streams

On a stream opened with the option #cmd("record"), one #cmd("fread()")
returns one record and one #cmd("fwrite()") writes one. On variable-length
records the record descriptor word is part of the data in both directions:
bytes 0 and 1 hold the length of the record including the four bytes of
the descriptor, bytes 2 and 3 are zero. The character functions --
#cmd("fgetc()"), #cmd("fgets()"), #cmd("fputs()") and the formatted
functions -- do not work on a record stream. A record stream cannot be
opened with #cmd("+").

== The Standard Streams <std-stdio-std>

#idx("stdin")#idx("stdout")#idx("stderr")
#idx("SYSPRINT")#idx("SYSTERM")#idx("SYSIN")
The C start-up opens three streams before it calls #cmd("main()"):

#tab(caption: [The standard streams])[
  #table(columns: (0.75in, 1.6in, 1fr),
    [Stream], [Opened as], [Result],
    [#cmd("stdout")], [#cmd("fopen(\"*SYSPRINT\", \"w\")")], [The
      #cmd("SYSPRINT") DD; if the step has none, a SYSOUT data set of the
      job's message class, or under TSO the terminal.],
    [#cmd("stderr")], [#cmd("fopen(\"*SYSTERM\", \"w\")")], [The
      #cmd("SYSTERM") DD; otherwise SYSOUT or the terminal, as for
      #cmd("stdout").],
    [#cmd("stdin")], [#cmd("fopen(\"dd:SYSIN\", \"r\")")], [The
      #cmd("SYSIN") DD; if the step has none, #cmd("'NULLFILE'"), which is
      at end of file at once.],
  )
] <std-stdio-std-tab>

If one of them cannot be opened, the program ends before #cmd("main()")
with return code 12 and a console message that names the stream and the
#cmd("errno") value.

The three are macros that call a function, not variables. A program can
replace one with #cmd("freopen()").

#idx("TSO", "terminal I/O")
On the TSO terminal a line is written with #cmd("TPUT") and read with
#cmd("TGET"). Leading and trailing blanks of an input line are removed.
TSO has no end-of-file key, so an input line that consists only of
#cmd("/*"), #cmd("//"), #cmd("<eof>") or #cmd("<EOF>") is end of file.

== Buffering <std-stdio-buffer>

#idx("buffering")
#idx("block", "written at close")
A stream has a buffer of one record. The record goes to the access method
when the line ends, when the buffer is full, or at #cmd("fflush()")\; the
access method collects records into blocks and writes a block when it is
full. The last block, which is usually short, is written when the data set
is closed. #cmd("setbuf()") and #cmd("setvbuf()") have no effect, and the
modes #cmd("_IOFBF"), #cmd("_IOLBF") and #cmd("_IONBF") are not
distinguished.

#idx("ENOSPC")#idx("x37 abend")
A write error ends the stream's ability to write. A data set that runs
out of space does not end the program with an x37 abend: the write fails,
the error indicator is set and #cmd("errno") is #cmd("ENOSPC")\; any
other permanent I/O error sets #cmd("EIO"). Every later write on the
stream fails at once with the same #cmd("errno"), every later read with
#cmd("EIO"), and nothing more is written, until #cmd("clearerr()") is called. Because the last block is
written at close, running out of space on it is reported by
#cmd("fclose()") and by nothing earlier.

== Positioning <std-stdio-position>

#idx("file position")
The position that #cmd("ftell()") returns and #cmd("fseek()") accepts is a
byte count from the beginning of the data set, counted the way the data
would be read back. On a text stream each record counts its data and one
byte for the #cmd("'\\n'")\; a record of a fixed-length data set counts its
full length: after #cmd("\"L1\\n\"") is written to a data set with
#cmd("RECFM=FB") and #cmd("LRECL=80"), #cmd("ftell()") returns 81. On a
record stream the position is the number of records.

BSAM cannot position a sequential data set at a byte, so the library moves
by reading:

- Forward, it reads up to the position.
- Backward on an input stream, it closes and opens the data set again and
  reads from the beginning. The time this takes grows with the position.
- A target inside the record in the buffer, forward or backward, is
  reached without reading or reopening. Before 2.6.2 that shortcut placed
  the stream wrongly within the record and left #cmd("ftell()") at the old
  position (libc370 issue 473).
- An output stream (#cmd("\"w\""), #cmd("\"a\"")) is always at its end and
  cannot move. #cmd("fseek()") to the current position succeeds; any other
  position fails with #cmd("ESPIPE").

== Formatted Output <std-stdio-printf-conv>

#idx("printf", "conversions")
#idx("conversion specification")
The functions of the #cmd("printf()") family accept the conversion
specifications of @std-stdio-printf-tab. A specification is #cmd("%"),
then optional flags, width, a period and precision, and a length
modifier, and then the conversion character. The flags are #cmd("-"),
#cmd("+"), space, #cmd("#") and #cmd("0")\; width and precision may be
#cmd("*").

#tab(caption: [Conversions of the printf() functions])[
  #table(columns: (0.95in, 1fr),
    [Conversion], [Result],
    [#cmd("d"), #cmd("i")], [A signed decimal integer.],
    [#cmd("u"), #cmd("o"), #cmd("x"), #cmd("X")], [An unsigned integer in
      decimal, octal or hexadecimal. #cmd("x") uses lowercase digits,
      #cmd("X") uppercase. The flag #cmd("#") puts #cmd("0x") in front of
      #cmd("x") and of #cmd("X") alike, and has no effect on #cmd("o").],
    [#cmd("c")], [One character. A width pads it with blanks, on the
      left or, with the flag #cmd("-"), on the right.],
    [#cmd("s")], [A string. A null pointer is printed as #cmd("(null)") by
      #cmd("%s") without width and precision only.],
    [#cmd("p")], [A pointer, as eight uppercase hexadecimal digits, without
      #cmd("0x").],
    [#cmd("f"), #cmd("F")], [A floating-point value in fixed notation, 6
      decimal places by default.],
    [#cmd("e"), #cmd("E")], [A floating-point value in exponent notation.
      The exponent is introduced by an uppercase #cmd("E") for both, and
      has a sign and two digits: #cmd("1.500000E+00").],
    [#cmd("g"), #cmd("G")], [Exponent notation if the exponent is below
      -4 or not below the precision, otherwise fixed notation; see
      below.],
    [#cmd("n")], [Stores the number of characters written so far in the
      object the argument points to: an #cmd("int"), or with #cmd("hh"),
      #cmd("h"), #cmd("l"), #cmd("ll"), #cmd("j"), #cmd("z") or #cmd("t")
      the type that modifier names.],
    [#cmd("%")], [A #cmd("%") character. A flag or a width is ignored.],
    [#cmd("a"), #cmd("A")], [Not supported. The argument, a #cmd("double"),
      is consumed, and the conversion specification is printed as it is
      written: #cmd("printf(\"%a\", 1.0)") prints #cmd("%a").],
  )
] <std-stdio-printf-tab>

The length modifiers are #cmd("l") (#cmd("long")), #cmd("ll") and #cmd("j")
(64-bit values), #cmd("z") and #cmd("t") (32-bit #cmd("size_t") and
#cmd("ptrdiff_t")), #cmd("h") and #cmd("hh"), and #cmd("L"). #cmd("long")
and #cmd("int") are both 32 bits. #cmd("long double") is the same type as
#cmd("double"), so #cmd("L") with a floating-point conversion reads a
#cmd("double").

These rules differ from C99:

- On the integer conversions #cmd("h") and #cmd("hh") are accepted and
  ignored: the value is printed as the #cmd("int") it was passed as, and #cmd("printf(\"%hhd\", 300)")
  prints #cmd("300").
- #cmd("%a") and #cmd("%A") are not implemented; see the table.
- A conversion character that has no meaning is printed as written,
  together with its flags, width and precision, and consumes no argument:
  #cmd("printf(\"%y|%d\", 5)") prints #cmd("%y|5"). A #cmd("%") at the
  end of the format is printed as #cmd("%").
- The flag #cmd("0") on a negative number, or together with #cmd("+") or
  space, puts the zeros in front of the sign: #cmd("printf(\"%05d\", -3)")
  prints #cmd("000-3"). The width does not include the #cmd("0x") of the
  flag #cmd("#"), which is placed after the padding:
  #cmd("printf(\"%#08x\", 1)") prints #cmd("00000000x1"). The flag
  #cmd("0") is not ignored when a precision is given.
- #cmd("%e") with precision 0 keeps the decimal point:
  #cmd("printf(\"%.0e\", 2.5)") prints #cmd("3.E+00"). The flag #cmd("#")
  has no effect on floating-point conversions.
- In fixed notation #cmd("%g") takes the precision as the number of
  decimal places, as #cmd("%f") does, not as the number of significant
  digits, and then removes trailing zeros. In exponent notation it keeps
  them. @std-stdio-printf-ex shows the effect.
- The precision of an integer conversion must not exceed 126.
- An output error is reported only when it is already known: the return
  value is negative when the stream is not open for writing or its error
  indicator is set. Output that is still in the buffer fails only when the
  buffer is written\; use #cmd("ferror()") to find out.

Floating-point values are System/370 hexadecimal floating point, which has
about 15 significant decimal digits and no infinity and no NaN.

#tab(caption: [Output of some conversions])[
  #table(columns: (2.2in, 1.15in, 1fr),
    [Call], [Output], [C99 would print],
    [#cmd("printf(\"%e\", 1.5)")], [#cmd("1.500000E+00")], [#cmd("1.500000e+00")],
    [#cmd("printf(\"%g\", 1.5)")], [#cmd("1.5")], [#cmd("1.5")],
    [#cmd("printf(\"%g\", 123.456789)")], [#cmd("123.456789")], [#cmd("123.457")],
    [#cmd("printf(\"%g\", 1234567.0)")], [#cmd("1.234567E+06")], [#cmd("1.23457e+06")],
    [#cmd("printf(\"%g\", 1e10)")], [#cmd("1.000000E+10")], [#cmd("1e+10")],
    [#cmd("printf(\"%g\", 0.000123456)")], [#cmd("0.000123")], [#cmd("0.000123456")],
    [#cmd("printf(\"%010.2f\", -3.14159)")], [#cmd("-000003.14")], [#cmd("-000003.14")],
    [#cmd("printf(\"%05d\", -3)")], [#cmd("000-3")], [#cmd("-0003")],
    [#cmd("printf(\"%a\", 1.0)")], [#cmd("%a")], [#cmd("0x1p+0")],
    [#cmd("printf(\"%p\", p)")], [#cmd("00001234")], [an implementation-defined form],
  )
] <std-stdio-printf-ex>

== Formatted Input <std-stdio-scanf-conv>

#idx("scanf", "conversions")
The functions of the #cmd("scanf()") family accept the conversions of
@std-stdio-scanf-tab. A specification is #cmd("%"), an optional #cmd("*")
that suppresses the assignment, an optional length modifier and the
conversion character. White space in the format skips any white space in
the input, and any other character must match the input.

#tab(caption: [Conversions of the scanf() functions])[
  #table(columns: (1.2in, 1fr),
    [Conversion], [Input and argument],
    [#cmd("d")], [A decimal integer, with an optional sign. #cmd("int *").],
    [#cmd("i")], [An integer in the base its prefix gives: #cmd("0x") or
      #cmd("0X") hexadecimal, #cmd("0") octal, else decimal. #cmd("int *").],
    [#cmd("u"), #cmd("o"), #cmd("x")], [An unsigned integer in decimal, octal
      or hexadecimal; #cmd("x") accepts a #cmd("0x") prefix. A minus sign is
      accepted and negates the value, as in #cmd("strtoul()").
      #cmd("unsigned int *").],
    [#cmd("p")], [A hexadecimal pointer value. #cmd("void **").],
    [#cmd("e"), #cmd("f"), #cmd("g"), #cmd("E"), #cmd("G")], [A
      floating-point number in decimal, with optional exponent.
      #cmd("float *"), or #cmd("double *") with #cmd("l") or #cmd("L").],
    [#cmd("s")], [A sequence of characters that are not white space.
      #cmd("char *")\; the string is terminated.],
    [#cmd("[")#var("set")#cmd("]"), #cmd("[^")#var("set")#cmd("]")], [A
      sequence of characters in #var("set"), or not in it. #cmd("char *").],
    [#cmd("c")], [One character. #cmd("char *")\; no terminator.],
    [#cmd("n")], [Stores the number of characters read so far. Not counted
      in the return value.],
    [#cmd("%")], [Matches a #cmd("%").],
  )
] <std-stdio-scanf-tab>

The length modifiers #cmd("hh"), #cmd("h"), #cmd("l"), #cmd("ll"),
#cmd("j"), #cmd("z"), #cmd("t") and #cmd("L") select the size of the
integer the argument points to; #cmd("L") with an integer conversion means
#cmd("long long").

These rules differ from C99:

- *There is no field width.* A digit after the #cmd("%") is not a width;
  the conversion fails and with it the rest of the format. #cmd("%s") and
  #cmd("%[") therefore store as many characters as the input holds, and the
  array they write to must be large enough for the longest possible input.
  #cmd("%c") always reads one character.
- #cmd("%X"), #cmd("%F"), #cmd("%a") and #cmd("%A") are not supported.
- A conversion suppressed with #cmd("*") is counted in the return value.
- In #cmd("%[") a #cmd("-") is an ordinary character, not a range:
  #cmd("%[a-z]") matches #cmd("a"), #cmd("-") and #cmd("z").
- The character that follows the closing #cmd("]") of #cmd("%[") in the
  format is skipped and has no effect. Follow #cmd("]") with a blank:
  #cmd("\"%[^,] ,%d\"") reads #cmd("abc,12") as intended, where
  #cmd("\"%[^,],%d\"") does not. A format must not end with #cmd("%[...]").
- When the input ends before the first conversion, the functions return
  0, not #cmd("EOF").

== Macros and Types <std-stdio-macros>

#idx("FILE")#idx("fpos_t")#idx("EOF")#idx("BUFSIZ")#idx("FOPEN_MAX")
#tab(caption: [Macros and types of \<stdio.h\>])[
  #table(columns: (1.2in, 0.9in, 1fr),
    [Name], [Value], [Meaning],
    [#cmd("FILE")], [192 bytes], [Describes a stream. Its members belong to
      the library; a program uses only pointers to it.],
    [#cmd("fpos_t")], [#cmd("unsigned long")], [A position, as
      #cmd("ftell()") returns it.],
    [#cmd("EOF")], [-1], [End of file or error.],
    [#cmd("BUFSIZ")], [32768], [Size of a buffer for #cmd("setbuf()"),
      which uses none.],
    [#cmd("FOPEN_MAX")], [256], [Not a limit: the library does not count
      open streams. Each open stream occupies a DD, and MVS limits the
      number of DDs of a step.],
    [#cmd("FILENAME_MAX")], [260], [Size of an array for a file name.],
    [#cmd("L_tmpnam")], [260], [Size of an array for #cmd("tmpnam()").],
    [#cmd("TMP_MAX")], [25], [Number of names #cmd("tmpnam()") is
      guaranteed to make; it makes 99999 before it repeats.],
    [#cmd("_IOFBF"), #cmd("_IOLBF"), #cmd("_IONBF")], [1, 2, 3], [Buffering
      modes for #cmd("setvbuf()"), which ignores them.],
    [#cmd("SEEK_SET"), #cmd("SEEK_CUR"), #cmd("SEEK_END")], [0, 1, 2],
      [Origins for #cmd("fseek()").],
    [#cmd("NULL")], [#cmd("((void *)0)")], [The null pointer.],
    [#cmd("stdin"), #cmd("stdout"), #cmd("stderr")], [], [The standard
      streams; see @std-stdio-std.],
  )
] <std-stdio-macros-tab>

#cmd("getc()"), #cmd("getchar()"), #cmd("putc()"), #cmd("putchar()"),
#cmd("feof()") and #cmd("ferror()") are defined as macros and also exist
as functions. Each macro evaluates its arguments once.

== clearerr, feof, ferror <std-stdio-clearerr>

#idx("clearerr")#idx("feof")#idx("ferror")#idx("error indicator")
#idx("end-of-file indicator")

=== Format

```
#include <stdio.h>

void clearerr(FILE *stream);
int  feof(FILE *stream);
int  ferror(FILE *stream);
```

=== Description

#cmd("feof()") tests the end-of-file indicator of #var("stream"),
#cmd("ferror()") its error indicator. #cmd("clearerr()") resets both.

=== Returns

#cmd("feof()") and #cmd("ferror()") return 1 if the indicator is set and 0
if it is not, as functions and as macros. #cmd("clearerr()") returns no
value.

=== Notes

- After a write error, every read and write on the stream fails until
  #cmd("clearerr()") is called (see @std-stdio-buffer). A program that
  has freed space for a data set that ran out of it calls
  #cmd("clearerr()") and writes again. The records lost in the failed write
  are not written again.
- #cmd("fseek()") and #cmd("rewind()") also clear both indicators.

=== Related

@std-stdio-fseek, @std-stdio-perror.

== fclose <std-stdio-fclose>

#idx("fclose")

=== Format

```
#include <stdio.h>

int fclose(FILE *stream);
```

=== Description

#cmd("fclose()") writes what is still in the buffer of #var("stream"),
closes the data set, frees the DD if #cmd("fopen()") allocated it, and
frees the stream. The stream must not be used afterwards.

=== Returns

0 if the stream was closed without error, #cmd("EOF") if writing the
buffer or closing the data set failed. The stream is closed and freed in
both cases.

=== Errors

#deflist(width: 1in,
  [#cmd("ENOSPC")], [The data set ran out of space; at least the last
    block was not written.],
  [#cmd("EIO")], [A permanent I/O error occurred while writing or
    closing.],
)

=== Notes

- The last block of an output data set is written by the close (see
  @std-stdio-buffer), so #cmd("fclose()") is the only call that can report
  that it was not. Check its return value whenever the end of the data
  matters.
- A null pointer, or a pointer that does not point to a stream, is
  ignored, and #cmd("fclose()") returns 0.

=== Related

@std-stdio-fopen, @std-stdio-fflush.

== fflush <std-stdio-fflush>

#idx("fflush")

=== Format

```
#include <stdio.h>

int fflush(FILE *stream);
```

=== Description

On an output stream, #cmd("fflush()") hands the record in the buffer to the
access method, which ends the record. With nothing in the buffer it does
nothing. On an input stream it has no effect, except on a stream opened
with #cmd("+"), where a record changed in place is written back.

=== Returns

0 on success. On failure a nonzero value instead of #cmd("EOF"): 12 when
the data set is out of space, another positive value for an I/O error.

=== Errors

#deflist(width: 1in,
  [#cmd("ENOSPC")], [The data set ran out of space.],
  [#cmd("EIO")], [A permanent I/O error.],
)

=== Notes

- #cmd("fflush(NULL)") is not supported: #var("stream") must point to an
  open stream. To flush all streams, call #cmd("fflush()") for each.
- The record reaches the access method, not the data set: the block that
  holds it is written when it is full or at #cmd("fclose()"). After an
  abend, what #cmd("fflush()") wrote may still be lost.
- On a text stream #cmd("fflush()") in the middle of a line ends the record
  there; the rest of the line becomes a record of its own.

=== Related

@std-stdio-fclose, @std-stdio-buffer.

== fgetc, getc, getchar <std-stdio-fgetc>

#idx("fgetc")#idx("getc")#idx("getchar")

=== Format

```
#include <stdio.h>

int fgetc(FILE *stream);
int getc(FILE *stream);
int getchar(void);
```

=== Description

#cmd("fgetc()") reads the next character from #var("stream").
#cmd("getc()") is the same, and #cmd("getchar()") reads from
#cmd("stdin"). On a text stream the end of each record is read as
#cmd("'\\n'").

=== Returns

The character, as an #cmd("unsigned char") converted to #cmd("int"), or
#cmd("EOF") at the end of the data set or on an error. #cmd("feof()") and
#cmd("ferror()") tell the two apart.

=== Errors

#deflist(width: 1in,
  [#cmd("EBADF")], [The stream is not open for reading. The error indicator
    is not set.],
  [#cmd("EIO")], [A permanent I/O error, or an earlier error on the
    stream.],
)

=== Notes

- On a record stream the functions return #cmd("EOF").
- When the end-of-file indicator is set, the functions return #cmd("EOF")
  even if a character was pushed back with #cmd("ungetc()"). The character
  is kept, and the first read after #cmd("clearerr()") returns it.

=== Related

@std-stdio-fgets, @std-stdio-ungetc, @std-stdio-fread.

== fgets, gets <std-stdio-fgets>

#idx("fgets")#idx("gets")

=== Format

```
#include <stdio.h>

char *fgets(char *s, int n, FILE *stream);
char *gets(char *s);
```

=== Description

#cmd("fgets()") reads characters from #var("stream") into #var("s") until
it has read a #cmd("'\\n'"), which it stores, or #var("n") - 1 characters,
or reaches the end of the data set, and terminates #var("s") with a null
character. On a text stream it reads one record per call when #var("n") is
large enough.

#cmd("gets()") reads a line from #cmd("stdin") into #var("s") with no limit
on its length and replaces the #cmd("'\\n'") with a null character.

=== Returns

#var("s"), or #cmd("NULL") if no character was read because the end of the
data set was reached, if an error occurred, or if #var("n") is less than
1.

=== Errors

#cmd("EBADF") and #cmd("EIO"), as for #cmd("fgetc()").

=== Notes

- A record is returned with its trailing blanks. For a data set with
  #cmd("LRECL=80") and fixed-length records, #var("n") must be at least 82
  to read a whole record, its #cmd("'\\n'") and the terminator in one
  call; with a smaller #var("n") the rest of the record comes with the next
  call.
- #cmd("gets()") cannot be used safely, because it writes as many
  characters as the line has. It is not part of C11. Use #cmd("fgets()").
- On a record stream both functions return #cmd("NULL").

=== Example

@std-stdio-trim-ex copies the records of a data set to #cmd("stdout")
without the blanks that fill them.

#fig(caption: [Reading lines and removing the trailing blanks])[
  #code(read("../ex/std-stdio/trim.c"), numbers: true)
] <std-stdio-trim-ex>

=== Related

@std-stdio-fgetc, @std-stdio-fputs, @std-stdio-records.

== fopen <std-stdio-fopen>

#idx("fopen")

=== Format

```
#include <stdio.h>

FILE *fopen(const char *filename, const char *mode);
```

=== Description

#cmd("fopen()") opens the data set, member, SYSOUT data set or terminal
that #var("filename") names, in the way #var("mode") gives, and returns a
stream for it. @std-stdio-names describes the file names,
@std-stdio-modes the modes and their options. A file name that is not a
DD is allocated as @std-stdio-alloc describes.

=== Returns

A pointer to the stream, or #cmd("NULL") if the stream could not be
opened.

=== Errors

#deflist(width: 1.1in,
  [#cmd("EBUSY")], [The DD is a JES2 input stream (#cmd("SYSIN")
    #cmd("DD *")) that is already open. JES2 allows one open at a time, and
    a second one would end the program with an abend.],
  [#cmd("EINVAL")], [A #cmd("+") mode on a device other than direct access,
    #cmd("*PUTLINE") opened for reading: with #cmd("\"r\"") or a
    #cmd("+") mode, or #cmd("*GETLINE") opened for writing: with
    #cmd("\"w\""), #cmd("\"a\"") or a #cmd("+") mode.],
  [#cmd("ENODEV")], [#cmd("*PUTLINE") or #cmd("*GETLINE") in a program that
    does not run under the TSO terminal monitor program.],
  [#cmd("ENOMEM")], [Not enough storage for the buffers. Raise the
    #cmd("REGION").],
  [#cmd("EOPNOTSUPP")], [#cmd("\"a\"") for a member that exists.],
)

For other failures -- a DD that does not exist, a data set that is not
cataloged, a mode that is not valid, a refused allocation -- the value of
#cmd("errno") has no meaning.

=== Notes

- Each stream that #cmd("fopen()") allocates occupies a DD until
  #cmd("fclose()").
- For a data set name, an existing data set is allocated
  #cmd("DISP=OLD") for writing, which holds it exclusively until the stream
  is closed. A member is allocated #cmd("DISP=SHR").
- A DD with #cmd("DUMMY") or #cmd("'NULLFILE'") opened for reading
  returns end of file at once.
- A stream on #cmd("*PUTLINE") writes each line as one PUTLINE. A line of
  more than 252 characters goes out as several lines. The return code of
  PUTLINE is not checked, so a failed write is not reported: neither
  #cmd("ferror()") nor a negative return value shows it.
- A stream on #cmd("*GETLINE") reads one line per GETLINE. Leading blanks
  are kept\; TSO removes the trailing ones, so a line is not padded to the
  record length of #cmd("SYSTSIN"). A line of more than 1020 characters is
  cut to 1020 (from the library source, not measured). At the end of
  #cmd("SYSTSIN") the stream reaches end of file.

=== Example

@std-stdio-newds-ex writes ten lines into a new data set with fixed-length
records.

#fig(caption: [Creating a data set with fopen()])[
  #code(read("../ex/std-stdio/newds.c"), numbers: true)
] <std-stdio-newds-ex>

A program that reads a DD and writes a member of a library could be run
with this JCL:

```
//STEP1    EXEC PGM=MYPROG
//STEPLIB  DD DSN=MYUSER.LOADLIB,DISP=SHR
//INPUT    DD DSN=MYUSER.DATA,DISP=SHR
//OUTLIB   DD DSN=MYUSER.REPORTS,DISP=SHR
//SYSPRINT DD SYSOUT=*
```

and open its streams with #cmd("fopen(\"DD:INPUT\", \"r\")") and
#cmd("fopen(\"DD:OUTLIB(JAN)\", \"w\")").

=== Related

@std-stdio-fclose, @std-stdio-freopen, @mvs-datasets, @mvs-dynalloc.

== fprintf, printf, snprintf, sprintf <std-stdio-fprintf>

#idx("fprintf")#idx("printf")#idx("snprintf")#idx("sprintf")

=== Format

```
#include <stdio.h>

int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int snprintf(char *s, size_t n, const char *format, ...);
int sprintf(char *s, const char *format, ...);
```

=== Description

The functions format their arguments under control of #var("format") as
@std-stdio-printf-conv describes. #cmd("fprintf()") writes the result to
#var("stream"), #cmd("printf()") to #cmd("stdout"). #cmd("sprintf()")
stores it in #var("s") and terminates it with a null character.
#cmd("snprintf()") stores at most #var("n") - 1 characters and the null
character; with #var("n") 0 it stores nothing, and #var("s") may then be a
null pointer.

=== Returns

#cmd("printf()"), #cmd("sprintf()") and #cmd("snprintf()") return the
number of characters formatted, without the null character. For
#cmd("snprintf()") this is the length the whole result has, also when it
was cut to fit #var("n"). #cmd("fprintf()") and #cmd("printf()") return a
negative value on an output error: when the stream is not open for writing
(#cmd("errno") #cmd("EBADF")), or when its error indicator is set.

=== Notes

- The output has no limit on its length.
- Output still in the buffer fails only when the buffer is written, at a
  later call or at #cmd("fclose()")\; a return value of 0 or more does not
  prove that the data has reached the data set. Test #cmd("ferror()")
  before the stream is closed, and the return value of #cmd("fclose()").
- #cmd("sprintf()") does not know the size of #var("s"). Use
  #cmd("snprintf()").
- On a record stream nothing is written.

=== Example

```
char  name[9];
int   rc = 4;

snprintf(name, sizeof name, "STEP%04d", 17);
printf("%-8s ended, return code %d\n", name, rc);
```

prints #cmd("STEP0017 ended, return code 4").

=== Related

@std-stdio-vfprintf, @std-stdio-printf-conv.

== fputc, putc, putchar <std-stdio-fputc>

#idx("fputc")#idx("putc")#idx("putchar")

=== Format

```
#include <stdio.h>

int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);
int putchar(int c);
```

=== Description

#cmd("fputc()") writes #var("c"), converted to #cmd("unsigned char"), to
#var("stream"). #cmd("putc()") is the same; #cmd("putchar()") writes to
#cmd("stdout"). On a text stream #cmd("'\\n'") ends the record.

=== Returns

#var("c") on success, #cmd("EOF") on failure.

=== Errors

#deflist(width: 1.1in,
  [#cmd("EBADF")], [The stream is not open for writing.],
  [#cmd("ENOSPC")], [The data set ran out of space, now or in an earlier
    write.],
  [#cmd("EIO")], [A permanent I/O error, now or in an earlier write.],
  [#cmd("EOPNOTSUPP")], [On a #cmd("+") stream, the write would change the
    length of a record (see @std-stdio-modes).],
)

=== Notes

On a record stream the functions write nothing and return #var("c").

=== Related

@std-stdio-fputs, @std-stdio-fwrite.

== fputs, puts <std-stdio-fputs>

#idx("fputs")#idx("puts")

=== Format

```
#include <stdio.h>

int fputs(const char *s, FILE *stream);
int puts(const char *s);
```

=== Description

#cmd("fputs()") writes the string #var("s") to #var("stream") without its
null character. #cmd("puts()") writes #var("s") and a #cmd("'\\n'") to
#cmd("stdout")\; the string and its newline are one write, which no other
task's output can split.

=== Returns

On success, #cmd("fputs()") returns the number of characters written and
#cmd("puts()") a nonnegative value. On failure both return #cmd("EOF").

=== Errors

As for #cmd("fputc()").

=== Notes

On a record stream #cmd("fputs()") returns #cmd("EOF") and writes nothing.

=== Related

@std-stdio-fputc, @std-stdio-fgets.

== fread <std-stdio-fread>

#idx("fread")

=== Format

```
#include <stdio.h>

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
```

=== Description

On a text or binary stream, #cmd("fread()") reads up to #var("nmemb")
elements of #var("size") bytes each into the array #var("ptr") points to,
as if by repeated calls of #cmd("fgetc()").

On a record stream it reads one record. It stores at most
#var("size") #sym.times #var("nmemb") bytes of it and discards the rest.

=== Returns

The number of complete elements read; less than #var("nmemb") at the end
of the data set or on an error. On a record stream #var("nmemb") if a
record was read, 0 at the end of the data set or on an error.

=== Errors

#deflist(width: 1in,
  [#cmd("EBADF")], [The stream is not open for reading.],
  [#cmd("EIO")], [A permanent I/O error, now or earlier.],
)

=== Notes

- On a record stream the return value does not tell the length of the
  record. On variable-length records the record descriptor word in bytes 0
  and 1 holds it; on fixed-length records it is the #cmd("LRECL"). On
  undefined records the length cannot be learned: clear the buffer before
  each call if it matters.
- On a text stream every record ends with #cmd("'\\n'"), which is counted
  among the bytes read.

=== Example

@std-stdio-vbrec-ex reads a data set with variable-length records as a
record stream.

#fig(caption: [Reading variable-length records])[
  #code(read("../ex/std-stdio/vbrec.c"), numbers: true)
] <std-stdio-vbrec-ex>

=== Related

@std-stdio-fwrite, @std-stdio-records.

== freopen <std-stdio-freopen>

#idx("freopen")

=== Format

```
#include <stdio.h>

FILE *freopen(const char *filename, const char *mode, FILE *stream);
```

=== Description

#cmd("freopen()") opens #var("filename") with #var("mode"), as
#cmd("fopen()") does, and then closes the data set of #var("stream") and
makes #var("stream") refer to the new one. The usual use is to send a
standard stream elsewhere, as in
#cmd("freopen(\"DD:REPORT\", \"w\", stdout)").

=== Returns

#var("stream"), or #cmd("NULL") on failure.

=== Errors

Those of #cmd("fopen()"). See also the notes.

=== Notes

- The new data set is opened before the old one is closed. If the open
  fails, #var("stream") stays open on its old data set, with its error
  indicator set; C99 requires it to be closed.
- #var("filename") must not be a null pointer: a change of mode on the
  same data set is not supported, and #cmd("freopen()") returns
  #cmd("NULL").
- #var("stream") must be open.
- A character pushed back with #cmd("ungetc()") before the call is not
  discarded.
- Closing the old data set writes its last block. A failure there does
  not make #cmd("freopen()") fail; it sets #cmd("errno") to
  #cmd("ENOSPC") or #cmd("EIO"). Set #cmd("errno") to 0 before the call
  and test it afterwards when the end of the old data matters, or use
  #cmd("fclose()") and #cmd("fopen()").

=== Related

@std-stdio-fopen, @std-stdio-std.

== fscanf, scanf, sscanf <std-stdio-fscanf>

#idx("fscanf")#idx("scanf")#idx("sscanf")

=== Format

```
#include <stdio.h>

int fscanf(FILE *stream, const char *format, ...);
int scanf(const char *format, ...);
int sscanf(const char *s, const char *format, ...);
```

=== Description

The functions read characters, convert them as #var("format") directs and
store the results in the objects the further arguments point to.
#cmd("fscanf()") reads from #var("stream"), #cmd("scanf()") from
#cmd("stdin") and #cmd("sscanf()") from the string #var("s").
@std-stdio-scanf-conv lists the conversions and how they differ from C99.

=== Returns

The number of conversions that stored a value, which may be fewer than
requested if the input does not match. Conversions suppressed with
#cmd("*") are counted. At the end of the input the functions return 0;
they do not return #cmd("EOF").

=== Notes

- There is no field width, so #cmd("%s") and #cmd("%[") cannot be
  limited. Read a line with #cmd("fgets()") and convert it with
  #cmd("sscanf()"), into arrays as long as the line buffer, so that no
  input can overrun them.
- #cmd("fscanf()") and #cmd("scanf()") read one character beyond the last
  one they convert and push it back with #cmd("ungetc()") -- but only when
  the format runs to its end. When they stop early, at a character that
  does not match the format or at an input failure, that character is
  consumed (a defect, libc370 issue 474).

=== Example

```
char  line[82];
char  name[82];
int   count;

if (fgets(line, sizeof line, stdin) != NULL
    && sscanf(line, "%s %d", name, &count) == 2)
    printf("%s: %d\n", name, count);
```

=== Related

@std-stdio-vfscanf, @std-stdio-fgets.

== fseek, fsetpos, rewind <std-stdio-fseek>

#idx("fseek")#idx("fsetpos")#idx("rewind")

=== Format

```
#include <stdio.h>

int  fseek(FILE *stream, long int offset, int whence);
int  fsetpos(FILE *stream, const fpos_t *pos);
void rewind(FILE *stream);
```

=== Description

#cmd("fseek()") sets the position of #var("stream") to #var("offset")
bytes from the beginning (#cmd("SEEK_SET")), from the current position
(#cmd("SEEK_CUR")) or from the end (#cmd("SEEK_END")). @std-stdio-position
describes the positions and how the library reaches them.
#cmd("fsetpos()") is #cmd("fseek(stream, *pos, SEEK_SET)"), and
#cmd("rewind()") is #cmd("fseek(stream, 0L, SEEK_SET)") without a return
value. A character pushed back with #cmd("ungetc()") is discarded, except
on a stream opened with a #cmd("+") mode, where it survives the move (a
defect, libc370 issue 474).

=== Returns

#cmd("fseek()") and #cmd("fsetpos()") return 0 on success and -1 on
failure.

=== Errors

#deflist(width: 1in,
  [#cmd("ESPIPE")], [The stream is open for writing only, and the position
    is not the current one.],
  [#cmd("EINVAL")], [On a #cmd("+") stream: #var("whence") is not valid,
    or the position is negative or beyond the end of the data set.],
)

=== Notes

- All three functions clear the error indicator as well as the end-of-file
  indicator. C99 lets #cmd("fseek()") clear only the end-of-file indicator.
- On a stream opened #cmd("\"w\"") or #cmd("\"a\"") that has been written
  to, #cmd("rewind()") does not move the position: it cannot return to the
  beginning without truncating the data set.
- On a stream opened #cmd("\"r\""), #cmd("SEEK_END") moves to the end of
  the data set and ignores #var("offset").
- A position beyond the end of a data set opened #cmd("\"r\"") fails, sets
  the error and end-of-file indicators and leaves the stream at the end.
  On a record stream it returns 0 and sets no indicator (a defect,
  libc370 issue 474).
- Moving backward, or forward over a long distance, reads the data set and
  takes time in proportion; a move within the current record does not
  (@std-stdio-position).
- On a record stream the position is a record number.

=== Related

@std-stdio-ftell, @std-stdio-position.

== ftell, fgetpos <std-stdio-ftell>

#idx("ftell")#idx("fgetpos")

=== Format

```
#include <stdio.h>

long int ftell(FILE *stream);
int      fgetpos(FILE *stream, fpos_t *pos);
```

=== Description

#cmd("ftell()") returns the current position of #var("stream") as
@std-stdio-position defines it. #cmd("fgetpos()") stores the same value
in #var("pos").

=== Returns

#cmd("ftell()") returns the position. #cmd("fgetpos()") returns 0.

=== Notes

- A character pushed back with #cmd("ungetc()") does not change the
  position that #cmd("ftell()") reports.
- On a stream opened #cmd("\"a+\""), the position is the size of the data
  set, which #cmd("fopen()") counts by reading it.

=== Related

@std-stdio-fseek.

== fwrite <std-stdio-fwrite>

#idx("fwrite")

=== Format

```
#include <stdio.h>

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
```

=== Description

On a text or binary stream, #cmd("fwrite()") writes #var("nmemb") elements
of #var("size") bytes from the array #var("ptr") points to, as if by
repeated calls of #cmd("fputc()").

On a record stream, it writes the #var("size") #sym.times #var("nmemb")
bytes as one record. On variable-length records they must begin with the
record descriptor word: bytes 0 and 1 hold the length of the record,
which is #var("size") #sym.times #var("nmemb"), and bytes 2 and 3 are zero.

=== Returns

The number of complete elements written; less than #var("nmemb") on an
error. On a record stream 1 when the record was written and 0 when it was
not, whatever #var("nmemb") is. When #var("size") or #var("nmemb") is 0,
nothing is written\; on a record stream 0 is returned, but on a text or
binary stream #var("nmemb") is returned (#cmd("fwrite(p, 0, 5, f)")
returns 5), which is a defect (libc370 issue 474).

=== Errors

#deflist(width: 1in,
  [#cmd("EBADF")], [The stream is not open for writing.],
  [#cmd("EINVAL")], [Record stream only: the record is longer than
    #cmd("LRECL") (#cmd("LRECL") - 4 on spanned records), or on
    variable-length records the descriptor word is shorter than four
    bytes, does not match the length or has bytes 2 and 3 not zero. Nothing
    is written, and the stream stays usable.],
  [#cmd("ENOSPC")], [The data set ran out of space, now or earlier.],
  [#cmd("EIO")], [A permanent I/O error, now or earlier.],
)

=== Related

@std-stdio-fread, @std-stdio-records.

== perror <std-stdio-perror>

#idx("perror")

=== Format

```
#include <stdio.h>

void perror(const char *s);
```

=== Description

#cmd("perror()") writes to #cmd("stderr") the string #var("s"), a colon and
a blank, and then the message for the current value of #cmd("errno") and
a #cmd("'\\n'"). If #var("s") is a null pointer or empty, only the message
is written. For a value that #cmd("strerror()") has no message for, the
message is #cmd("unknown error:") followed by the number.

=== Notes

The #cmd("unknown error:") message is formatted into a static buffer of 24
bytes, which holds a number of up to nine digits, or eight and a minus
sign. A value of #cmd("errno") of 1000000000 or more, or of -100000000 or
less, overruns the buffer by up to two bytes.

=== Example

```
if (fclose(f) == EOF)
    perror("DD:REPORT");
```

writes, for example, #cmd("DD:REPORT: No space left on device").

=== Related

#cmd("strerror()") in @std-string, @std-errno.

== remove <std-stdio-remove>

#idx("remove")

=== Format

```
#include <stdio.h>

int remove(const char *filename);
```

=== Description

#cmd("remove()") deletes the data set or member that #var("filename")
names. A name of the form #var("dsname")#cmd("(")#var("member")#cmd(")"),
without quotes, deletes the member from its directory, with the data set
allocated #cmd("DISP=SHR"). Any other name is deleted by the IDCAMS command
#cmd("DELETE") #var("filename"), with #var("filename") changed to
uppercase.

=== Returns

0 on success. For a member, the return code of #cmd("STOW") -- 8 when the
member does not exist -- or a negative value when the data set could not
be allocated or opened. For anything else, the highest condition code of
IDCAMS -- 8 when the entry does not exist -- or a negative value when
IDCAMS could not be called.

=== Notes

- The name is a data set name as IDCAMS reads it, not a file name of
  #cmd("fopen()"): there is no #cmd("DD:") form, and no TSO prefix is
  added. A name without quotes is taken as fully qualified.
- A name in parentheses that is not a member name, such as the relative
  generation #cmd("GDG.BASE(0)"), is passed to IDCAMS.
- IDCAMS needs the data set exclusively. A member deleted in the
  #var("dsname")#cmd("(")#var("member")#cmd(")") form does not, so it can be
  deleted while other jobs use the library.

=== Related

@std-stdio-rename, @mvs-dynalloc.

== rename <std-stdio-rename>

#idx("rename")

=== Format

```
#include <stdio.h>

int rename(const char *old, const char *newnam);
```

=== Description

#cmd("rename()") gives the data set or member #var("old") the name
#var("newnam"). When both are of the form
#var("dsname")#cmd("(")#var("member")#cmd(")") with the same
#var("dsname"), the member is renamed in its directory, with the data set
allocated #cmd("DISP=SHR")\; the member keeps its location and its user
data. Any other pair of names is renamed by the IDCAMS command
#cmd("ALTER") #var("old") #cmd("NEWNAME(")#var("newnam")#cmd(")"), with
both changed to uppercase.

=== Returns

0 on success. For a member, the return code of #cmd("STOW") -- 4 when
#var("newnam") already exists, 8 when #var("old") does not -- or a
negative value when the data set could not be allocated or opened. For
anything else, the highest condition code of IDCAMS, or a negative value
when IDCAMS could not be called.

=== Notes

The names are read as for #cmd("remove()").

=== Related

@std-stdio-remove.

== setbuf, setvbuf <std-stdio-setbuf>

#idx("setbuf")#idx("setvbuf")

=== Format

```
#include <stdio.h>

void setbuf(FILE *stream, char *buf);
int  setvbuf(FILE *stream, char *buf, int mode, size_t size);
```

=== Description

The functions have no effect. The buffer of a stream is always one record,
supplied by the library (see @std-stdio-buffer).

=== Returns

#cmd("setvbuf()") returns 0.

=== Related

@std-stdio-fflush.

== tmpfile <std-stdio-tmpfile>

#idx("tmpfile")#idx("temporary data set")

=== Format

```
#include <stdio.h>

FILE *tmpfile(void);
```

=== Description

#cmd("tmpfile()") creates a temporary data set with a name from
#cmd("tmpnam()") and opens it with mode #cmd("\"wb+\""): what is written
can be read back after #cmd("rewind()") or #cmd("fseek()"). The data set is
allocated on VIO, with the attributes of @std-stdio-defaults. As a
temporary data set, it is deleted by the system at the end of the step at
the latest.

=== Returns

A pointer to the stream, or #cmd("NULL").

=== Notes

Up to LIBC/370 2.4 the stream was opened #cmd("\"wb\"") and could only be
written.

=== Related

@std-stdio-tmpnam.

== tmpnam <std-stdio-tmpnam>

#idx("tmpnam")

=== Format

```
#include <stdio.h>

char *tmpnam(char *s);
```

=== Description

#cmd("tmpnam()") makes a name for a temporary data set,
#cmd("&&TMP")#var("nnnnn"), where #var("nnnnn") is a number counted from
00001 for the whole program. If #var("s") is not a null pointer, the name
is stored there; #var("s") must have room for 11 characters. Otherwise it
is stored in an area of the calling task, which the next call
overwrites.

=== Returns

A pointer to the name, or #cmd("NULL") if the C run-time environment is
not set up.

=== Notes

After 99999 names the numbers start again at 1.

=== Related

@std-stdio-tmpfile, @std-stdio-names.

== ungetc <std-stdio-ungetc>

#idx("ungetc")

=== Format

```
#include <stdio.h>

int ungetc(int c, FILE *stream);
```

=== Description

#cmd("ungetc()") pushes #var("c"), converted to #cmd("unsigned char"),
back onto #var("stream"), so that the next read returns it. One character
can be pushed back at a time.

=== Returns

#var("c"), or #cmd("EOF") if #var("c") is #cmd("EOF") or a character has
already been pushed back.

=== Notes

- #cmd("ungetc()") does not clear the end-of-file indicator, although C99
  requires a successful call to clear it, and a read does not return a pushed-back character while the
  indicator is set: after the end of the data set the next read returns
  #cmd("EOF") again. The character is held back, not lost. Call
  #cmd("clearerr()"), before or after #cmd("ungetc()"), and the next read
  returns it.
- The position that #cmd("ftell()") reports does not change.
- #cmd("fseek()"), #cmd("fsetpos()") and #cmd("rewind()") discard the
  character, except on a stream opened with a #cmd("+") mode (a defect, libc370
  issue 474).

=== Related

@std-stdio-fgetc.

== vfprintf, vprintf, vsnprintf, vsprintf <std-stdio-vfprintf>

#idx("vfprintf")#idx("vprintf")#idx("vsnprintf")#idx("vsprintf")

=== Format

```
#include <stdarg.h>
#include <stdio.h>

int vfprintf(FILE *stream, const char *format, va_list arg);
int vprintf(const char *format, va_list arg);
int vsnprintf(char *s, size_t n, const char *format, va_list arg);
int vsprintf(char *s, const char *format, va_list arg);
```

=== Description

The functions are #cmd("fprintf()"), #cmd("printf()"), #cmd("snprintf()")
and #cmd("sprintf()") with the arguments in a #cmd("va_list") that
#cmd("va_start()") has initialized. They do not call #cmd("va_end()").

=== Returns

As for the functions without #cmd("v").

=== Notes

#cmd("vfprintf()") writes each character to the stream as it is formatted
and has no limit on the length of the output.

=== Example

```
#include <stdarg.h>
#include <stdio.h>

void msg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fputs("MYPROG: ", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
```

=== Related

@std-stdio-fprintf.

== vfscanf, vscanf, vsscanf <std-stdio-vfscanf>

#idx("vfscanf")#idx("vscanf")#idx("vsscanf")

=== Format

```
#include <stdarg.h>
#include <stdio.h>

int vfscanf(FILE *stream, const char *format, va_list arg);
int vscanf(const char *format, va_list arg);
int vsscanf(const char *s, const char *format, va_list arg);
```

=== Description

The functions are #cmd("fscanf()"), #cmd("scanf()") and #cmd("sscanf()")
with the arguments in a #cmd("va_list") that #cmd("va_start()") has
initialized.

=== Returns

As for the functions without #cmd("v").

=== Related

@std-stdio-fscanf.
