#import "../bookmaster/bookmaster.typ": *

= Console Communication <mvs-console>

#idx("console")
#idx("operator", "communicating with")
This chapter describes the functions that let a program talk to the MVS
operator. Three headers provide them:

#deflist(width: 1.35in,
  [#cmd("<mvs/wto.h>")], [writes messages to the operator (WTO, SVC 35),
    asks the operator for a reply (WTOR), and writes storage dumps and save
    area trace-backs as console messages.],
  [#cmd("<mvs/console.h>")], [receives the operator commands MODIFY and STOP
    addressed to the job or started task (EXTRACT and QEDIT).],
  [#cmd("<mvs/mtt.h>")], [reads a copy of the master trace table, the
    in-storage record of recent console traffic.],
)

#idx("authorization", "console functions")
The WTO, WTOR and command functions need no authorization. MVS shows a
message written by a program that is not APF-authorized with a #cmd("+")
in front of it, and a message from an authorized program without it. The
master trace table functions need APF authorization, or the authorization
SVC 244 that #cmd("__autask()") uses (see @mvs-program)\; see
@mvs-console-cmtt_new.

#idx("WTO", "message length")
A single WTO message holds at most 124 characters, and the text of a WTOR
message at most 122. The functions issue the messages without routing or
descriptor codes, so the installation defaults apply. The text is EBCDIC,
as every string in a libc370 program is.

== wto <mvs-console-wto>
#idx("wto")
#idx("WTO", "wto function")

=== Format
```
#include <mvs/wto.h>

void wto(char *buf);
```

=== Description
#cmd("wto()") writes the string #var("buf") to the operator with one or
more WTO macro calls (SVC 35). A string of up to 124 characters is written
as one message. A longer string is split: each message ends after the last
blank at or before column 125 -- so a message can be 125 characters long
when the blank is in column 125 -- and a run of 124 characters that holds
no blank is cut at column 124. The next message continues with the rest.

A newline in #var("buf") is not interpreted\; use #cmd("wtof()") to write
several lines.

=== Notes
#cmd("wto()") changes #var("buf") when the string is longer than 124
characters: after each message it moves the rest of the text to the
beginning of the buffer, so the buffer holds only the last part when the
function returns. That is why the parameter is not #cmd("const")\; do not
pass a string literal that may be longer than 124 characters. A string of
up to 124 characters is left as it is.

An empty string writes an empty message.

=== Related
@mvs-console-wtof, @mvs-console-wtor

== wtof, vwtof <mvs-console-wtof>
#idx("wtof")
#idx("vwtof")

=== Format
```
#include <mvs/wto.h>

void wtof(const char *text, ...);
void vwtof(const char *text, va_list tList);
```

=== Description
#cmd("wtof()") formats its arguments under the control of the format
string #var("text"), as #cmd("printf()") does, and writes the result to
the operator. #cmd("vwtof()") does the same with the arguments in the
#cmd("va_list") #var("tList").

The formatted text is split at every newline character, and each part is
written with #cmd("wto()"), so one call can write several messages. A part
longer than 124 characters is split further, as @mvs-console-wto
describes. A newline at the end of the text does not produce an empty
message\; two newlines in a row do.

=== Notes
The formatted text is built in a buffer of 4096 bytes on the stack and is
truncated to 4095 characters.

=== Example
```
wtof("DEMO001I %d RECORDS READ FROM %s", count, ddname);
```

=== Related
@mvs-console-wto, @mvs-console-wtor

== wtor, wtorf, vwtorf <mvs-console-wtor>
#idx("wtor")
#idx("wtorf")
#idx("vwtorf")
#idx("WTOR")
#idx("operator", "reply")

=== Format
```
#include <mvs/wto.h>

void wtor(char *reply, unsigned replymax, const char *text);
void wtorf(char *reply, unsigned replymax, const char *text, ...);
void vwtorf(char *reply, unsigned replymax, const char *text,
            va_list tList);
```

=== Description
#cmd("wtor()") writes the message #var("text") to the operator with a WTOR
macro call (SVC 35) and waits until the operator has replied. The reply is
stored in the buffer #var("reply"), at most #var("replymax") characters of
it.

#cmd("wtorf()") and #cmd("vwtorf()") format the message first, as
#cmd("wtof()") and #cmd("vwtof()") do. When the formatted text contains
newline characters, every line but the last is written as an ordinary
message with #cmd("wto()"), and the last line is the WTOR message.

=== Notes
- The reply is not terminated with a null character. Clear the buffer
  before the call, and make it at least one byte longer than
  #var("replymax"), as @mvs-console-reply-ex does.
- #var("replymax") is limited to 119, the longest reply that MVS accepts\;
  a larger value is reduced to 119.
- The text of the WTOR message is limited to 122 characters\; a longer
  text is truncated. #cmd("wtor()") does not split it.
- If #var("reply") is #cmd("NULL") or #var("replymax") is 0, no reply is
  requested and #cmd("wtor()") does not wait.
- If the text given to #cmd("wtorf()") or #cmd("vwtorf()") ends with a
  newline character, no WTOR is issued: the lines are written as ordinary
  messages, the function returns at once, and #var("reply") is not
  changed. End the format string without a newline.
- The calling task waits until the reply arrives\; there is no time limit.

=== Example
#fig(caption: [Asking the operator for a reply])[
  #code(read("../ex/mvs-console/reply.c"), numbers: true)
] <mvs-console-reply-ex>

=== Related
@mvs-console-wtof

== wtodump, wtodumpf <mvs-console-wtodump>
#idx("wtodump")
#idx("wtodumpf")
#idx("storage", "displaying on the console")

=== Format
```
#include <mvs/wto.h>

void wtodump(const char *title, void *varea, int size, int chunk);
void wtodumpf(void *buf, int len, const char *fmt, ...);
```

=== Description
#cmd("wtodump()") writes #var("size") bytes of storage, starting at
#var("varea"), to the operator in hexadecimal and as characters. The first
message gives the address, the #var("title") and the length:

```
Dump of 0004A2F0 "INPUT RECORD" (32 bytes)
```

It is followed by one message for each #var("chunk") bytes. A message
gives the offset from #var("varea"), the bytes in hexadecimal in groups of
four, and the same bytes as characters between colons\; a byte that is not
a printable character is shown as a period:

```
+00000 C8C5D3D3 D640E6D6 D9D3C440 40404040 :HELLO WORLD     :
+00010 F0F0F1F2 00000000 00000000 00000000 :0012............:
```

#cmd("wtodumpf()") writes the same dump with 16 bytes to a message. It
builds the title from the format string #var("fmt") and the arguments that
follow it, as #cmd("printf()") does.

=== Notes
- #var("chunk") must be between 1 and 35. The hexadecimal and the character
  part of a message are built in 80-byte buffers on the stack, and a larger
  value overruns them. Use a multiple of 4: with any other value the last
  hexadecimal digit of each line is cut off (a defect, libc370 issue 404).
- The title formatted by #cmd("wtodumpf()") must not be longer than 79
  characters. It is formatted with #cmd("vsprintf()") into an 80-byte
  buffer, and a longer title overruns it.
- Each line is a separate WTO message. A large area produces many messages
  on the console and in the system log\; dump only what is needed.

=== Example
```
wtodumpf(rec, 32, "RECORD %u OF %s", recno, ddname);
```

=== Related
@mvs-console-wtof, @mvs-console-wto_traceback

== wto\_traceback <mvs-console-wto_traceback>
#idx("wto_traceback")
#idx("save area", "trace-back")

=== Format
```
#include <mvs/wto.h>

void wto_traceback(SAVEAREA *sa);
```

=== Description
#cmd("wto_traceback()") follows the chain of save areas backward and writes
an entry for each function on it to the operator, after the message
#cmd("Save area trace back"). For each save area it writes two messages:
the name of the function, its entry point address and the size of its
stack frame\; then the address it returns to, as an address and as the
name, entry point and offset of the calling function:

```
  "copyrec" ep=01A2C8 frame=192 bytes
    returns to=01A6F4 "main" ep=01A5E0+114
```

The trace starts at the save area #var("sa"). When #var("sa") is
#cmd("NULL"), it starts at #cmd("wto_traceback()") itself: its first line
names the function that called it (#cmd("returns to=")).
It ends at the first save area of the program, or after 256 save areas.

The #cmd("SAVEAREA") type is declared in #cmd("<s370/savearea.h>"), which
#cmd("<mvs/wto.h>") includes.

=== Notes
The walk runs under the protection of #cmd("try()") (see @mvs-program): if
a save area in the chain has been overlaid and the walk causes an abend,
the trace-back ends and the program continues.

=== Related
@mvs-console-wtodump

== \_\_gtcom <mvs-console-gtcom>
#idx("__gtcom")
#idx("EXTRACT macro")
#idx("COM", "communication area")

=== Format
```
#include <mvs/console.h>

COM *__gtcom(void);
```

=== Description
#cmd("__gtcom()") returns the address of the communication area through
which MVS passes the operator commands MODIFY and STOP to the program. The
address is obtained with the EXTRACT macro (#cmd("FIELDS=COMM")) at the
first call and kept for the life of the program.

The #cmd("COM") structure has two fields:

#deflist(width: 1.35in,
  [#cmd("comecbpt")], [points to the event control block that MVS posts
    when a MODIFY or STOP command for this job arrives. Wait on it, for
    example with #cmd("ecb_wait()") (see @mvs-sync), or include it in a
    list of ECBs.],
  [#cmd("comcibpt")], [the first command input buffer (CIB) in the queue,
    or zero when no command is queued. Read it with #cmd("__cibget()").],
)

=== Returns
The address of the communication area, or #cmd("NULL") when the program
has no run-time anchor.

=== Example
See @mvs-console-modify-ex.

=== Related
@mvs-console-cibget, @mvs-console-cibdel, @mvs-console-cibset

== \_\_cibget <mvs-console-cibget>
#idx("__cibget")
#idx("CIB", "command input buffer")
#idx("MODIFY command")
#idx("STOP command")

=== Format
```
#include <mvs/console.h>

CIB *__cibget(void);
```

=== Description
#cmd("__cibget()") returns the first command input buffer (CIB) in the
program's queue. A CIB describes one operator command. Its fields of
interest are:

#deflist(width: 1.35in,
  [#cmd("cibverb")], [the command: #cmd("CIBSTART") (X'04') for the START
    command that started the task, #cmd("CIBMODFY") (X'44') for MODIFY,
    #cmd("CIBSTOP") (X'40') for STOP, and #cmd("CIBMOUNT") (X'0C') for
    MOUNT.],
  [#cmd("cibdata"), #cmd("cibdatln")], [the operand text and its length in
    bytes. For MODIFY it is the text after the comma that follows the job
    name: for #cmd("F DEMO,SHOW STATUS") it is #cmd("SHOW STATUS"). For
    START it is the #cmd("PARM") value. STOP has no operand. The text is
    not terminated with a null character\; use #cmd("cibdatln").],
  [#cmd("cibconid")], [the identifier of the console that entered the
    command.],
  [#cmd("cibasid")], [the address space identifier.],
  [#cmd("cibnext")], [the next CIB in the queue, or #cmd("NULL").],
)

The CIB stays in the queue until it is removed with #cmd("__cibdel()").

=== Returns
The address of the first CIB, or #cmd("NULL") when no command is queued or
the communication area cannot be obtained.

=== Notes
A started task finds the CIB of its START command in the queue when it
begins, whether or not the ECB has been posted. Remove it with
#cmd("__cibdel()") before you wait for the first command, as
@mvs-console-modify-ex does: while it is queued it counts against the limit
set by #cmd("__cibset()").

=== Related
@mvs-console-gtcom, @mvs-console-cibdel

== \_\_cibdel <mvs-console-cibdel>
#idx("__cibdel")
#idx("QEDIT macro")

=== Format
```
#include <mvs/console.h>

int __cibdel(CIB *cib);
```

=== Description
#cmd("__cibdel()") removes the command input buffer #var("cib") from the
program's queue and frees it, with the QEDIT macro (#cmd("BLOCK=")). Call
it for every CIB once the command has been handled. When the last CIB is
removed, the ECB that #cmd("comecbpt") points to is cleared, so the next
wait blocks until another command arrives.

=== Returns
The return code of QEDIT, 0 when the CIB was removed. When #var("cib") is
#cmd("NULL"), #cmd("__cibdel()") does nothing and returns 0.

=== Related
@mvs-console-cibget, @mvs-console-cibset

== \_\_cibset <mvs-console-cibset>
#idx("__cibset")

=== Format
```
#include <mvs/console.h>

int __cibset(unsigned count);
```

=== Description
#cmd("__cibset()") sets the number of MODIFY commands that MVS may queue
for the program at one time to #var("count"), with the QEDIT macro
(#cmd("CIBCTR=")). A MODIFY command that would exceed the limit is
rejected by the system and does not reach the program.

=== Returns
The return code of QEDIT, 0 when the limit was set. When #var("count") is
0, #cmd("__cibset()") does nothing and returns 0.

=== Notes
#cmd("__cibset()") and #cmd("__cibdel()") do not check that the
communication area could be obtained. In a program without a run-time
anchor, where #cmd("__gtcom()") returns #cmd("NULL"), they pass an invalid
address to QEDIT. Call #cmd("__gtcom()") first and test the result, as the
example does.

=== Example
#fig(caption: [A started task that accepts MODIFY and STOP commands])[
  #code(read("../ex/mvs-console/modify.c"), numbers: true)
] <mvs-console-modify-ex>

The task handles every queued command before it waits again.
#cmd("F DEMO,HELLO") writes #cmd("DEMO002I COMMAND 'HELLO' ACCEPTED"), and
#cmd("P DEMO") ends the task.

=== Related
@mvs-console-gtcom, @mvs-console-cibget, @mvs-console-cibdel

== cmtt\_new <mvs-console-cmtt_new>
#idx("cmtt_new")
#idx("master trace table")
#idx("MTT", "master trace table")

=== Format
```
#include <mvs/mtt.h>

CMTT *cmtt_new(void);
```

=== Description
#cmd("cmtt_new()") makes a copy of the master trace table and returns a
handle to it. The master trace table is the wrap-around table in which MVS
records the messages and commands that pass through the consoles\; it is
found through the CVT and the master scheduler resident data area.

The copy is taken in key 0, without serialization: the size is read before
the copy, and the pointers of the copy are adjusted from the live table
afterwards, so a table that MVS changes meanwhile can give an inconsistent
copy (a defect, libc370 issue 476). It is made in the program's own storage, in key 8, and the
pointers in its header are adjusted to point into the copy. The table on
the system is not changed.

=== Returns
A pointer to the new handle, or #cmd("NULL") when the program could not be
authorized or there is not enough storage.

=== Notes
- The program must be APF-authorized. If it is not, #cmd("cmtt_new()")
  authorizes the task for the duration of the call with
  #cmd("__autask()"), which needs the authorization SVC 244, and removes
  the authorization again before it returns. The caller's state and PSW
  key are restored.
- The copy is as large as the table on the system. Free it with
  #cmd("cmtt_free()") as soon as it has been read.
- #cmd("cmtt_new()") does not check that the system has a master trace
  table. On a system without one the result is unpredictable.

=== Related
@mvs-console-cmtt_get_array, @mvs-console-cmtt_free

== cmtt\_get\_array <mvs-console-cmtt_get_array>
#idx("cmtt_get_array")

=== Format
```
#include <mvs/mtt.h>

MTENTRY **cmtt_get_array(CMTT *cmtt);
```

=== Description
#cmd("cmtt_get_array()") returns the entries of the copied master trace
table as a dynamic array of pointers to #cmd("MTENTRY") structures, oldest
entry first. The number of entries is
#cmd("array_count(&")#var("array")#cmd(")") (see @ext). The fields of an
entry are:

#deflist(width: 1.35in,
  [#cmd("mtentflg"), #cmd("mtenttag")], [two bytes of flags and two bytes
    that identify the component that made the entry.],
  [#cmd("mtentimm")], [the immediate data of the entry.],
  [#cmd("mtentlen"), #cmd("mtentdat")], [the length of the data and the
    data, usually the text of a console message. The text is not
    terminated with a null character.],
)

The array is built at the first call and kept in the handle\; a second call
returns the same array.

The oldest entry in the table is often the remainder of a record that was
overwritten when the table wrapped, so it is left out. An entry whose
length is negative, or that would extend past the end of the table, ends
the walk, so a damaged table cannot lead the walk out of the copy.

=== Returns
The array, or #cmd("NULL") when #var("cmtt") is #cmd("NULL") or holds no
table.

=== Notes
The array and the entries belong to the handle. Do not free them: they are
freed by #cmd("cmtt_free()"), and the pointers are no longer valid after
it.

=== Example
#fig(caption: [Printing the last 20 entries of the master trace table])[
  #code(read("../ex/mvs-console/mtt.c"), numbers: true)
] <mvs-console-mtt-ex>

=== Related
@mvs-console-cmtt_new, @mvs-console-cmtt_free

== cmtt\_free <mvs-console-cmtt_free>
#idx("cmtt_free")

=== Format
```
#include <mvs/mtt.h>

void cmtt_free(CMTT **pcmtt);
```

=== Description
#cmd("cmtt_free()") frees the handle that #var("pcmtt") points to, together
with the copy of the table and the array of entries, and sets
#cmd("*")#var("pcmtt") to #cmd("NULL"). When #var("pcmtt") is
#cmd("NULL") nothing is done, and a handle that is already #cmd("NULL") is
accepted.

=== Related
@mvs-console-cmtt_new, @mvs-console-cmtt_get_array
