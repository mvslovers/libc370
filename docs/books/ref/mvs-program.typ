#import "../bookmaster/bookmaster.typ": *

= Program Management and Storage <mvs-program>

#idx("program management")
This chapter describes the functions with which a C program finds its own
run-time environment, calls other programs and load modules, obtains storage
from MVS, recovers from abnormal ends, changes its authorization and issues
TSO commands. The headers are:

#deflist(width: 1.35in,
  [#cmd("<mvs/crt.h>")], [the run-time anchors of a program, a task and the
    address space, and the start-up hook #cmd("__premain()")
    (@mvs-program-anchors).],
  [#cmd("<mvs/env.h>")], [environment variables loaded from a data set
    (@mvs-program-env).],
  [#cmd("<mvs/wsa.h>")], [writable static areas (@mvs-program-wsa).],
  [#cmd("<mvs/link.h>")], [LINK, LOAD and DELETE of load modules
    (@mvs-program-link).],
  [#cmd("<mvs/storage.h>")], [GETMAIN and FREEMAIN, and the subpool of the
    heap (@mvs-program-storage).],
  [#cmd("<mvs/recovery.h>")], [ESTAE recovery and the #cmd("try()") function
    (@mvs-program-recovery).],
  [#cmd("<mvs/apf.h>")], [APF authorization, supervisor state and the PSW
    key (@mvs-program-apf).],
  [#cmd("<mvs/tso.h>")], [calling a TSO command processor
    (@mvs-program-tso).],
)

#idx("start-up", "of a C program")
How a C program is started -- the start-up routine #cmd("@@CRT0"), the
start-up module #cmd("crtm"), the stack, and the parameters a program
receives -- is described in the _libc370 Programmer's Guide_. This chapter
describes only the interfaces of #cmd("<mvs/crt.h>") that a program may call.

#idx("authorization", "program management functions")
*Authorization.* Most functions in this chapter can be called by any
program. The exceptions are listed in @mvs-program-auth-tab. A function
that issues #cmd("MODESET") without checking the authorization of its
caller first ends an unauthorized program with abend S047: the program
must be linked with authorization code 1 (#cmd("ld370 --ac 1")) and be
fetched from an APF-authorized library.

#tab(caption: [Functions that need authorization])[
#table(columns: (auto, 1fr),
  [*Function*], [*Requirement*],
  [#cmd("getmain()"), #cmd("freemain()")], [supervisor state or PSW key 0-7
    for a subpool above 127 (the GETMAIN rules of MVS)],
  [#cmd("__setsp()")], [a subpool above 127 makes every later allocation of
    a problem-state caller fail],
  [#cmd("__loadhi()")], [APF-authorized and PSW key 0],
  [#cmd("__super()"), #cmd("__prob()"), #cmd("__pswkey()")], [APF-authorized\;
    otherwise they return #cmd("EPERM")],
  [#cmd("super_do()"), #cmd("super_key_do()")], [APF-authorized\; otherwise
    abend S047],
  [#cmd("__austep()"), #cmd("__uastep()")], [APF-authorized\; otherwise they
    return 4],
  [#cmd("clib_auth_cde()"), #cmd("clib_auth_name()")], [APF-authorized\;
    otherwise abend S047],
  [#cmd("__autask()")], [an authorization SVC 244 on the system (see
    @mvs-program-__autask)],
)
] <mvs-program-auth-tab>

#idx("external name")
Several functions have an external name that differs from the C name, such
as #cmd("@@LOADHI") for #cmd("__loadhi()"). An external name is at most eight
characters. Where the header assigns none, the compiler forms it from the C
name by changing each underscore to #cmd("@"), folding to uppercase and
cutting it to eight characters, so #cmd("__crtget()") and
#cmd("__CRTGET") are the same function. Where the header declares two C
names for one external name, such as #cmd("super_do()") and #cmd("__sudo"),
they are the same function and share one entry below.

// -------------------------------------------------------------------------
== Run-Time Anchors <mvs-program-anchors>

#idx("run-time anchor")
#idx("CLIBPPA")
#idx("CLIBCRT")
#idx("CLIBGRT")
The library keeps its state in three control blocks, which it finds
without any global variable, so that a program is reentrant:

#deflist(width: 1.35in,
  [#cmd("CLIBPPA")], [the _program properties area_, one for each
    invocation of a C program. The start-up module builds it in front of the
    stack and chains it into the first save area of the task, where
    #cmd("__ppaget()") finds it.],
  [#cmd("CLIBCRT")], [the _task_ anchor, one for each task (TCB) that runs
    C code: the value of #cmd("errno"), the state of #cmd("strtok()"),
    #cmd("rand()") and the time functions, and the ESTAE stack.],
  [#cmd("CLIBGRT")], [the _process_ anchor, one for the program and all its
    threads: the open files, the #cmd("atexit()") functions, the environment
    variables and the writable static areas.],
)

The layouts are in #cmd("<mvs/crt.h>"). They are given there for dumps and
for the library itself, and a program should not change them, with these
exceptions, which are reserved for the application and never used by the
library:

#deflist(width: 1.35in,
  [#cmd("grtapp1"), #cmd("grtapp2"), #cmd("grtapp3")], [three pointers in
    #cmd("CLIBGRT"), shared by all tasks of the program.],
  [#cmd("crtapp1"), #cmd("crtapp2")], [two pointers in #cmd("CLIBCRT"),
    one set for each task.],
)

#idx("TSO", "detecting")
A program may also read the environment flags: #cmd("GRTFLAG1_TSO") in
#cmd("grtflag1") is set when the program was started as a TSO command
processor, and #cmd("PPAFLAG_TSOFG") and #cmd("PPAFLAG_TSOBG") in
#cmd("ppaflag") tell a TSO foreground session from a TSO background job.

== \_\_ppaget <mvs-program-__ppaget>
#idx("__ppaget")
#idx("__PPAGET")

=== Format
```
#include <mvs/crt.h>

CLIBPPA *__ppaget(void);
CLIBPPA *__PPAGET(void);
```

=== Description
Returns the program properties area of the C program that runs on the
current task. The two names are the same function, #cmd("@@PPAGET").

The search looks first at the save area chained to the first save area of
the current TCB, then at the same place in each owning task up to the job
step task, and finally runs the save area chain of the caller backwards. A
candidate is accepted only when it lies below 16 MB and carries the
eyecatcher #cmd("@PPA").

=== Returns
The address of the #cmd("CLIBPPA"), or #cmd("NULL") when no C program is
active on the task.

=== Notes
A thread created with #cmd("cthread_create()") has no program properties area
of its own. On a thread #cmd("__ppaget()") returns the area of the program
that created the thread, found through the owning task.

=== Related
@mvs-program-__crtget, @mvs-program-__grtget

== \_\_crtget <mvs-program-__crtget>
#idx("__crtget")

=== Format
```
#include <mvs/crt.h>

CLIBCRT *__crtget(void);
```

=== Description
Returns the task anchor of the current task: the #cmd("CLIBCRT") in the
program properties area whose owning TCB is the current TCB.

=== Returns
The address of the #cmd("CLIBCRT"), or #cmd("NULL") when the program
properties area is not found or holds no anchor for the current task.

=== Notes
When no anchor is found, #cmd("__crtget()") writes a message to the operator
that names the TCB and the program properties area, and a dump of the
anchor array. This is a sign that C code runs on a task the library does not
know, for example in an exit routine entered on a system task.

=== Related
@mvs-program-__ppaget, @mvs-program-__grtget

== \_\_grtget <mvs-program-__grtget>
#idx("__grtget")

=== Format
```
#include <mvs/crt.h>

CLIBGRT *__grtget(void);
```

=== Description
Returns the process anchor, taken from the task anchor of the current
task. All threads of a program share it.

=== Returns
The address of the #cmd("CLIBGRT"), or #cmd("NULL") when there is no task
anchor.

=== Notes
The functions that depend on the process anchor -- the environment
functions, #cmd("__wsaget()") and others -- return their failure value when
it is missing, rather than abending.

=== Related
@mvs-program-__crtget

== \_\_exit <mvs-program-__exit>
#idx("__exit")

=== Format
```
#include <mvs/crt.h>

void __exit(int status);
```

=== Description
Ends the program the way #cmd("exit()") does: it calls the functions
registered with #cmd("atexit()") and #cmd("on_exit"), last registered first,
passing #var("status") to each, closes every open file, frees the
environment variables, the writable static areas and the device table,
and then ends the program with #var("status") as its return code. The
library's termination code releases the anchors and the stack.

=== Returns
Does not return.

=== Notes
#cmd("exit()") calls #cmd("__exit()"). A program has no reason to call it
directly.

The header does not declare #cmd("__exit()") with
#cmd("__attribute__((noreturn))"), so the compiler may warn about a missing
return value in a function that ends with a call to it.

=== Related
#cmd("exit()") in @std-stdlib

== \_\_premain <mvs-program-__premain>
#idx("__premain")
#idx("@@PREMAI")
#idx("start-up", "hook")

=== Format
```
#include <mvs/crt.h>

int __premain(char *parm, char *pgmname, void **pgmr1);
```

=== Description
A function that the program defines, not the library. When a program
defines #cmd("__premain()"), the C start-up calls it first: before it opens
the standard streams, before it reads the environment variables of the
#cmd("SYSENV") DD and before #cmd("main()").

#deflist(width: 0.9in,
  [#var("parm")], [the parameter as the program received it: two bytes of
    length followed by the text, or, for a TSO command, the command buffer
    with its prefix of four bytes.],
  [#var("pgmname")], [the program name, eight characters padded with
    blanks, not terminated by a null character.],
  [#var("pgmr1")], [the parameter list that register 1 addressed when the
    program was called.],
)

A standard stream that the function sets -- #cmd("stdin"), #cmd("stdout")
or #cmd("stderr") -- is kept by the start-up. One that it leaves
#cmd("NULL") is opened as usual.

=== Returns
0 to go on to #cmd("main()"). Any other value ends the program with that
value as its return code, as #cmd("exit()") does: the #cmd("atexit()")
functions are called, the open files are closed, and #cmd("main()") is not
called.

=== Notes
The start-up refers to #cmd("__premain()") with a weak external reference,
under the external name #cmd("@@PREMAI"). A program that does not define it
links nothing for it and starts as before.

The C environment exists when the function is called, but the standard
streams are not open: the function must not write to #cmd("stdout") or
#cmd("stderr") before it has set them itself. #cmd("wtof()") reaches the
console. #cmd("getenv()") does not see the variables of the #cmd("SYSENV")
DD yet.

The function replaces the practice of supplying a private #cmd("@@START")\;
which #cmd("@@START") a program gets then depends on the order of its link
line, and a wrong choice is not reported.

A module started with #cmd("crtm") also calls the function, if it defines
one.

=== Example
```
#include <stdio.h>
#include <mvs/crt.h>
#include <mvs/wto.h>

int __premain(char *parm, char *pgmname, void **pgmr1)
{
    (void)parm;
    (void)pgmr1;
    stdout = fopen("DD:REPORT", "w");
    if (stdout == NULL) {
        wtof("%.8s: DD REPORT is missing", pgmname);
        return 16;
    }
    return 0;
}
```

=== Related
@mvs-program-__exit

== Other Anchor Functions <mvs-program-anchor-internal>
#idx("__crtset")
#idx("__crtres")
#idx("__grtset")
#idx("__grtres")
#idx("__ppahrv")

=== Format
```
#include <mvs/crt.h>

int  __crtset(void);
int  __crtres(void);
int  __grtset(void);
int  __grtres(void);
void __ppahrv(CLIBPPA *ppa);
```

=== Description
These functions build and release the anchors and are called by the
start-up and termination code and by #cmd("try()"). A program must not call
them.

#cmd("__crtset") creates the task anchor for the current TCB and adds it to
the program properties area\; it takes the process anchor from the anchor
of the owning task. #cmd("__crtres") removes and frees it again.
#cmd("__grtset") creates the process anchor, #cmd("__grtres") frees it.
These four return 0 on success and a nonzero value otherwise.

#cmd("__ppahrv") releases what a program that ended abnormally under
#cmd("try()") left behind: it closes the program's open files and frees its
process and task anchors. Unlike #cmd("__exit()") it does not call the
#cmd("atexit()") functions.

// -------------------------------------------------------------------------
== Environment Variables <mvs-program-env>

#idx("environment variable")
MVS has no environment for a program to inherit. At start-up the library
loads the environment from the DD #cmd("SYSENV"), or, if that cannot be
read, from the DD #cmd("ENVIRON"), with #cmd("loadenv()"), which reads
#cmd("name=value") lines from a data set\; without either DD the
environment starts empty. #cmd("setenv()"), #cmd("putenv()") and further
calls of #cmd("loadenv()") change it. The
variables belong to the process anchor, so all threads of a program see the
same set. Names are compared with case, except by #cmd("getenvi()").

#cmd("<mvs/env.h>") also declares #cmd("getenv()"), which is described with
#cmd("setenv()"), #cmd("putenv()") and #cmd("unsetenv()") in @std-stdlib.

== loadenv <mvs-program-loadenv>
#idx("loadenv")

=== Format
```
#include <mvs/env.h>

int loadenv(const char *fn);
```

=== Description
Reads the data set #var("fn") and sets an environment variable for each
line of the form #cmd("name=value"), as #cmd("putenv()") does. #var("fn") is
any name #cmd("fopen()") accepts, for example #cmd("DD:SYSENV") or a data set
name.

Each line is prepared as follows:

- A line that begins with #cmd("*") or #cmd("#") is a comment and is
  skipped.
- When the last eight characters of a line longer than eight characters
  are all digits, they are taken as a sequence number and removed.
- Trailing blanks are removed.
- A line without #cmd("=") is skipped.

Blanks around the name and in front of the value are removed\; an
existing variable of the same name is replaced.

=== Returns
0 when the data set was read, or a nonzero value when it could not be
opened.

=== Notes
A line is read into a buffer of 1024 bytes\; a longer line is processed as
several lines.

A value whose last eight characters are digits loses them when the line is
longer than eight characters, because they cannot be told from a sequence
number. Set such a value with #cmd("setenv()").

A line that cannot be stored, for lack of storage, is skipped without a
message.

=== Example
#code(read("../ex/mvs-program/env.c"))

=== Related
@mvs-program-getenvi, #cmd("getenv()"), #cmd("setenv()") in @std-stdlib

== getenvi <mvs-program-getenvi>
#idx("getenvi")

=== Format
```
#include <mvs/env.h>

char *getenvi(const char *name);
```

=== Description
Returns the value of the environment variable #var("name"), like
#cmd("getenv()"), but compares the names without regard to case.

=== Returns
A pointer to the value, or #cmd("NULL") when no variable has that name or
the program has no process anchor.

=== Notes
The value belongs to the environment. It stays valid until the variable is
set again or removed.

=== Related
@mvs-program-__findenv, #cmd("getenv()") in @std-stdlib

== setenvi <mvs-program-setenvi>
#idx("setenvi")

=== Format
```
#include <mvs/env.h>

int setenvi(const char *name, int value, int rewrite);
```

=== Description
Sets the environment variable #var("name") to the decimal representation of
#var("value"), as #cmd("setenv(name, buf, rewrite)") does. When
#var("rewrite") is 0, an existing variable is left unchanged.

=== Returns
The value #cmd("setenv()") returns: 0 on success, nonzero on failure.

=== Related
#cmd("setenv()") in @std-stdlib

== \_\_findenv <mvs-program-__findenv>
#idx("__findenv")

=== Format
```
#include <mvs/env.h>

char *__findenv(const char *name, int *index, int nocase);
```

=== Description
Searches the environment for #var("name") and is the function on which
#cmd("getenv()"), #cmd("getenvi()") and #cmd("setenv()") are built. When
#var("nocase") is nonzero, the comparison ignores case.

When #var("index") is not #cmd("NULL"), #cmd("__findenv()") stores in it the
position of the variable found, or, when none is found, the position of the
first free slot in the environment array, or -1 when there is none.

=== Returns
A pointer to the value, or #cmd("NULL").

=== Notes
#cmd("__findenv()") does not serialize. #cmd("getenv()") and #cmd("getenvi()")
hold a lock on the environment while they search\; a program that calls
#cmd("__findenv()") directly while other threads change the environment must
do the same.

The header also declares the variables #cmd("__envvar") and
#cmd("__envsiz"). They are left from an earlier version of the library and
are not used\; the environment is kept in the process anchor.

// -------------------------------------------------------------------------
== Writable Static Areas <mvs-program-wsa>

#idx("writable static area")
#idx("reentrant program", "static data")
A load module fetched from a library in the link list cannot store into its
own static data: the first store ends the program with abend S0C4, whatever
its RENT attribute. Fetched from a private library through a STEPLIB, the
same module stores without trouble. The library therefore keeps data that a
function must change in a _writable static area_ on the heap, obtained by
#cmd("__wsaget()") and held in the process anchor.

== \_\_wsaget <mvs-program-__wsaget>
#idx("__wsaget")

=== Format
```
#include <mvs/wsa.h>

void *__wsaget(void *key, unsigned len);
```

=== Description
Returns the writable static area that belongs to #var("key") and has a
length of #var("len") bytes, and creates it on the first call.

#var("key") identifies the area. It is normally the address of a static
initializer in the load module: when a new area is created and
#var("key") is an address between #cmd("X'2000'") and #cmd("X'FF000000'"),
the first #var("len") bytes at #var("key") are copied into it. Otherwise
the new area is cleared to zeros. A later call with the same #var("key") and
#var("len") returns the same area.

=== Returns
The address of the area, or #cmd("NULL") when there is no process anchor
or storage cannot be obtained.

=== Notes
The area is shared by all threads of the program and is freed when the
program ends. The function serializes the creation, but not the use of the
area.

When storage cannot be obtained, #cmd("__wsaget()") writes a message to the
operator.

// -------------------------------------------------------------------------
== Calling Programs and Loading Modules <mvs-program-link>

#idx("LINK macro")
#idx("LOAD macro")
#idx("DELETE macro")
#cmd("<mvs/link.h>") gives a C program the MVS services LINK, LOAD and
DELETE. A program name is up to eight characters\; the functions fold it to
uppercase and pad it with blanks.

The parameter list #var("r1") that #cmd("__link()") passes is the list the
called program receives in register 1, in the form that program expects. To
pass a parameter as #cmd("EXEC PGM=...,PARM=") does, build a list of one
address that points to a halfword length followed by the text, and set the
high-order bit of that address, as in the example under
@mvs-program-__linkt.

A program called this way may itself be a C program with the usual
start-up, #cmd("@@CRT0"). It then builds its own run-time anchors and
releases them before it returns.

== \_\_link <mvs-program-__link>
#idx("__link")

=== Format
```
#include <mvs/link.h>

int __link(const char *pgm, void *dcb, void *r1, int *prc);
```

=== Description
Calls the program #var("pgm") with #cmd("LINK"), passing #var("r1") in
register 1, and waits until it returns. When #var("dcb") is not
#cmd("NULL") it is the address of an open DCB of the library to search\;
otherwise MVS searches the task library, the step or job library and the
link library. When #var("prc") is not #cmd("NULL"), the return code is
also stored in it.

=== Returns
The return code of the program in register 15, or -1 when #var("pgm") is
#cmd("NULL") or empty or the program cannot be called.

=== Notes
The LINK is issued with an error exit, so a program that cannot be found
gives -1 instead of abend S806. A program can of course also return -1
itself.

An abend of the called program is not intercepted: it ends the caller as
well, unless the caller has an ESTAE. Use #cmd("__linkt()") to catch it.

=== Related
@mvs-program-__linkt, @mvs-program-tsocmd

== \_\_linkt, \_\_linkds <mvs-program-__linkt>
#idx("__linkt")
#idx("__linkds")

=== Format
```
#include <mvs/link.h>

int __linkt(const char *pgm, void *dcb, void *r1, int *prc);
int __linkds(const char *pgm, void *dcb, void *r1, int *prc);
```

=== Description
Call the program #var("pgm") as #cmd("__link()") does, under the protection
of #cmd("try()"). When the called program ends abnormally, the caller
continues.

#cmd("__linkds()") also writes an abend report to the operator, as
#cmd("abendrpt(ESTAE_CREATE, DUMP_SUPPRESS)") does, before the abend is
caught.

The return code of the called program, or -1 when it could not be called,
is stored in #var("prc") when that is not #cmd("NULL").

=== Returns
0 when the program returned, the abend code in the form #cmd("X'00sssuuu'")
when it ended abnormally (see @mvs-program-try), or a negative value when
the ESTAE could not be created. No dump is taken.

=== Notes
When a called C program ends abnormally, its own termination code does not
run. The library then closes the files that program opened and frees its
stack and anchors, so that repeated abends of a called program do not use
up the region.

=== Example
#code(read("../ex/mvs-program/link.c"))

=== Related
@mvs-program-__link, @mvs-program-try, @mvs-program-abendrpt

== \_\_call <mvs-program-__call>
#idx("__call")

=== Format
```
#include <mvs/link.h>

int __call(void *func, void *plist);
```

=== Description
Calls the routine at the address #var("func") with #cmd("BALR 14,15"),
with #var("plist") in register 1. #var("func") is typically an entry point
returned by #cmd("__load()").

=== Returns
The value of register 15 when the routine returns.

=== Notes
The routine is called with the standard linkage of MVS: register 13 points
to the caller's save area. To call a C function in the same program, call
it directly.

=== Related
@mvs-program-__load

== \_\_load <mvs-program-__load>
#idx("__load")

=== Format
```
#include <mvs/link.h>

void *__load(void *dcb, const char *module, unsigned *size, char *ac);
```

=== Description
Brings the load module #var("module") into storage with #cmd("LOAD") and
returns its entry point. The member is looked up first with #cmd("BLDL") in
the library whose open DCB is #var("dcb"), or, when #var("dcb") is
#cmd("NULL"), in the task, step or job library and the link library.

When the module is loaded and #var("size") is not #cmd("NULL"), its length
in bytes is stored there\; when #var("ac") is not #cmd("NULL"), its APF
authorization code is stored there.

=== Returns
The entry point address, or #cmd("NULL") when the name is missing, the
member is not found or the LOAD fails.

=== Notes
The external name is #cmd("@@LOAD").

Each successful #cmd("__load()") raises the use count of the module, and each
#cmd("__delete()") lowers it. Call #cmd("__delete()") once for each
#cmd("__load()").

=== Related
@mvs-program-__delete, @mvs-program-__call, @mvs-program-__steplb

== \_\_delete <mvs-program-__delete>
#idx("__delete")

=== Format
```
#include <mvs/link.h>

int __delete(const char *module);
```

=== Description
Gives up a module loaded by #cmd("__load()"), with #cmd("DELETE EPLOC=").

=== Returns
The return code of #cmd("DELETE"): 0 when the module was deleted, 4 when it
was not loaded by this task. #cmd("__delete()") also returns 0 when
#var("module") is #cmd("NULL") or empty.

=== Related
@mvs-program-__load

== \_\_loadhi <mvs-program-__loadhi>
#idx("__loadhi")
#idx("CSA", "loading a module into")

=== Format
```
#include <mvs/link.h>

int __loadhi(const char *module, void **lpa, void **epa, unsigned *size);
```

=== Description
Loads the module #var("module") into the common service area (CSA,
subpool 241), so that it can be used from other address spaces.

#cmd("__loadhi()") loads the module from the STEPLIB of the job step into
private storage, copies it into storage obtained from subpool 241, and
corrects its address constants from the relocation dictionary, which it
reads from the member #cmd("DD:STEPLIB(")#var("module")#cmd(")"). The
private copy is then deleted again.

On success it stores the load point in #var("lpa"), the entry point in
#var("epa") and the length in #var("size"), each when not #cmd("NULL").

=== Returns
0 on success, 4 on failure. Each failure is reported to the operator with a
message.

=== Notes
The caller must be APF-authorized and run in PSW key 0. An authorized
program that runs in another key calls it as
#cmd("super_key_do(PSWKEY0, __loadhi, module, &lpa, &epa, &size)").

The module is read from STEPLIB only. A module with a relocation item that
lies outside the module is refused, and nothing is left in common storage.

The storage remains allocated after the program ends. Free it with
#cmd("freemain()") when the module is no longer used.

The external name is #cmd("@@LOADHI").

=== Related
@mvs-program-super_do, @mvs-program-getmain

== clib\_find\_cde <mvs-program-clib_find_cde>
#idx("clib_find_cde")
#idx("CDE")

=== Format
```
#include <mvs/link.h>

CDE *clib_find_cde(const char *name);
```

=== Description
Searches the job pack queue of the job step task for the contents
directory entry (CDE) of the module #var("name"), and returns it. The
#cmd("CDE") type is mapped by #cmd("<ibm/mvs/ihacde.h>").

=== Returns
The address of the CDE, or #cmd("NULL") when the module is not in the job
pack queue.

=== Notes
The name is padded with blanks but not folded to uppercase\; pass it in
uppercase.

The external name is #cmd("@@FNDCDE").

=== Related
@mvs-program-clib_auth_cde

// -------------------------------------------------------------------------
== Storage <mvs-program-storage>

#idx("storage", "GETMAIN")
#idx("subpool")
#cmd("malloc()") and the other functions of #cmd("<stdlib.h>") obtain their
storage through #cmd("__getm()"), which issues a conditional GETMAIN, rounds
each request with its 8-byte header up to a multiple of 64 bytes, and
returns #cmd("NULL") when the region is exhausted, rather than abending.

The storage of the heap comes from a subpool that the program can choose:
the _ambient_ heap subpool of the current task. It is 0 unless the program
sets another one with #cmd("__setsp()"). Each block records the subpool it
came from, so #cmd("free()") needs no variant. A C program called with
#cmd("__link()") starts with the ambient subpool of its caller.

A program that allocates its heap from a subpool of its own can release
everything in one step with #cmd("FREEMAIN") of the whole subpool, for
example after a called program ended abnormally. That is the purpose of the
ambient subpool, and also its danger: the subpool applies to _every_
allocation on the task, including storage that library functions or
callbacks obtain and that must outlive the release. Such storage must be
taken from a fixed subpool with #cmd("__getmsp()"), or allocated between
#cmd("__setsp(0)") and the restoring #cmd("__setsp()") call.

#cmd("getmain()") and #cmd("freemain()") are a separate interface to GETMAIN
and FREEMAIN with an explicit subpool. Their storage cannot be released with
#cmd("free()"), nor heap storage with #cmd("freemain()").

== getmain, freemain <mvs-program-getmain>
#idx("getmain")
#idx("freemain")

=== Format
```
#include <mvs/storage.h>

void *getmain(unsigned size, unsigned sp);
int   freemain(void *addr);
```

=== Description
#cmd("getmain()") obtains #var("size") bytes from subpool #var("sp") (0 to
255) with a conditional GETMAIN. The request is rounded, together with an
8-byte header, up to a multiple of 64 bytes, and the storage is cleared to
zeros. The header records the subpool, the length and the PSW key of the
caller.

#cmd("freemain()") clears the storage obtained by #cmd("getmain()") and
releases it to the subpool it came from.

=== Returns
#cmd("getmain()") returns the address of the storage, or #cmd("NULL") when
#var("size") is 0 or 16 MB or more, or the GETMAIN fails.

#cmd("freemain()") returns the return code of FREEMAIN, 0 when #var("addr") is
#cmd("NULL"), and -1 when the header in front of #var("addr") is not valid.

=== Notes
A failed GETMAIN and an invalid header are reported to the operator.

A subpool above 127 requires supervisor state or a PSW key of 0 to 7, as
MVS prescribes for GETMAIN. Storage in subpool 241, the common service
area, survives the end of the program and of the job.

=== Related
@mvs-program-__getmsp, @mvs-program-super_do

== \_\_setsp, \_\_getsp <mvs-program-__setsp>
#idx("__setsp")
#idx("__getsp")
#idx("subpool", "of the heap")

=== Format
```
#include <mvs/storage.h>

unsigned char __setsp(unsigned char sp);
unsigned char __getsp(void);
```

=== Description
#cmd("__setsp()") sets the ambient heap subpool of the current task to
#var("sp") and returns the previous value. #cmd("__getsp()") returns the
current value.

=== Returns
The previous or current subpool. Both return 0, and #cmd("__setsp()")
changes nothing, when the task has no program properties area of its own.

=== Notes
A thread created with #cmd("cthread_create()") has no program properties
area. Its ambient subpool is always 0.

A problem-state program may use the subpools 1 to 127. With a subpool above
127, every later allocation of a problem-state program returns
#cmd("NULL") until the subpool is set back\; it does not abend.

Set a nonzero subpool only after every allocation that must outlive it is
pinned to a fixed subpool (see @mvs-program-storage).

=== Related
@mvs-program-__getmsp, @mvs-program-__getm

== \_\_getmsp <mvs-program-__getmsp>
#idx("__getmsp")

=== Format
```
#include <mvs/storage.h>

void *__getmsp(size_t size, unsigned char sp);
```

=== Description
Obtains a heap block of #var("size") bytes from subpool #var("sp"),
regardless of the ambient subpool. The block has the same header as a block
from #cmd("malloc()") and is released with #cmd("free()") or #cmd("__freem()").

=== Returns
The address of the block, or #cmd("NULL") when #var("size") is 0, the
rounded size does not fit in 24 bits, or the GETMAIN fails.

=== Notes
The storage is not cleared.

=== Related
@mvs-program-__setsp

== \_\_getm, \_\_freem <mvs-program-__getm>
#idx("__getm")
#idx("__freem")

=== Format
```
#include <mvs/storage.h>

void *__getm(size_t sz);
void  __freem(void *ptr);
```

=== Description
The allocator under #cmd("malloc()") and #cmd("free()"). #cmd("__getm()")
obtains #var("sz") bytes from the ambient heap subpool with a conditional
GETMAIN\; #cmd("__freem()") releases a block to the subpool recorded in its
header.

=== Returns
#cmd("__getm()") returns the address of the block, or #cmd("NULL") when
#var("sz") is not positive, the rounded size does not fit in 24 bits, or
the GETMAIN fails.

=== Notes
Both are written in assembler. The storage is not cleared.

Use #cmd("malloc()") and #cmd("free()") rather than these functions:
#cmd("malloc()") also sets #cmd("errno") to #cmd("ENOMEM") when storage is
short.

=== Related
#cmd("malloc()"), #cmd("free()") in @std-stdlib

// -------------------------------------------------------------------------
== Recovery <mvs-program-recovery>

#idx("recovery")
#idx("ESTAE macro")
#idx("abend", "recovery from")
#cmd("<mvs/recovery.h>") gives a C program the recovery service ESTAE.
#cmd("estae()") establishes a C function as recovery routine, which MVS calls
with the system diagnostic work area (SDWA) when the task ends abnormally.
#cmd("try()") calls a function under such a routine and turns an abend into
a return code. #cmd("abendrpt()") establishes a routine that only writes an
abend report.

ESTAE is available to every program, in problem state as in supervisor
state. Each task keeps its own stack of recovery routines in its task
anchor, at most ten deep. #cmd("try()") uses one level for the duration of
the call.

The type #cmd("SDWA") and its fields are mapped by
#cmd("<ibm/mvs/ihasdwa.h>"), which #cmd("<mvs/recovery.h>") includes.

== estae, \_\_estae <mvs-program-estae>
#idx("estae")
#idx("__estae")
#idx("SETRP")

=== Format
```
#include <mvs/recovery.h>

int __estae(ESTAE_OP op, void *fp, void *udata);
#define estae(op,fp,udata) __estae((op),(fp),(udata))

#define SETRP(sdwa,rc,retry,regs) ...
```

=== Description
Creates, replaces or removes a recovery routine, according to #var("op"):

#deflist(width: 1.35in,
  [#cmd("ESTAE_CREATE")], [establishes #var("fp") as a new recovery
    routine, with #var("udata") as its parameter.],
  [#cmd("ESTAE_OVERLAY")], [replaces the most recent recovery routine by
    #var("fp") and #var("udata").],
  [#cmd("ESTAE_DELETE")], [removes the most recent recovery routine.
    #var("fp") and #var("udata") are not used.],
)

The recovery routine is called on a stack of its own of 32 KB, as

```
int fp(SDWA *sdwa, void *param);
```

where #var("param") points to two words: the address of the routine
itself, and #var("udata"). To retry, the routine sets the retry address in
the SDWA with #cmd("SETRP") and returns 4\; to let the abend go on, it
returns 0. #cmd("SETRP(sdwa, rc, retry, regs)") stores the return code and
the retry address in the SDWA, and with a nonzero #var("regs") requests
that the registers in the SDWA be loaded for the retry.

When MVS supplies no SDWA, the abend goes on without calling the routine.

=== Returns
The return code of the ESTAE macro: 0 when the routine was established or
removed. -1 when #var("fp") is #cmd("NULL") for #cmd("ESTAE_CREATE") or
#cmd("ESTAE_OVERLAY"), when ten routines are already established, or when
the task has no task anchor.

=== Notes
A missing #var("fp") is reported on #cmd("stderr").

The routine is established with #cmd("TERM=YES"), so it is also entered
when the task is being ended. It must then do no retry\; the bit
#cmd("SDWACLUP") in #cmd("SDWAERRD") is set in that case. An address in
#var("retry") must be the address of code, not of a C function: a C
program normally uses #cmd("try()") instead.

=== Related
@mvs-program-try, @mvs-program-abendrpt

== try, \_\_\_try <mvs-program-try>
#idx("try")
#idx("___try")

=== Format
```
#include <mvs/recovery.h>

int ___try(void *func, ...);
#define try(func,...) ___try((func), __VA_ARGS__)
```

=== Description
Calls the function #var("func") with the arguments that follow it, under a
recovery routine. When #var("func") returns, #cmd("try()") returns 0. When
#var("func") ends abnormally, the recovery routine suppresses the dump,
retries in #cmd("try()"), and #cmd("try()") returns the abend code.

The abend code has the form #cmd("X'00sssuuu'"): #cmd("sss") is the system
completion code, #cmd("uuu") the user completion code. A system abend S0C4
gives #cmd("X'000C4000'"), a user abend U0100 gives #cmd("X'00000064'").
The code is also kept for #cmd("tryrc()").

=== Returns
0 when #var("func") returned normally\; the abend code when it ended
abnormally\; a negative value, the ESTAE return code negated, when the
recovery routine could not be established.

=== Notes
The return value of #var("func") is lost. A function called with
#cmd("try()") passes its results through its arguments.

#cmd("try()") is a macro that needs at least one argument after
#var("func"), because of #cmd("__VA_ARGS__")\; pass 0 when #var("func")
takes none.

When #var("func") has called other C programs, with #cmd("__link()") or
similar, and the abend happened in one of them, #cmd("try()") releases the
stack, the open files and the anchors those programs left behind before it
returns.

=== Example
#code(read("../ex/mvs-program/try.c"))

=== Related
@mvs-program-tryrc, @mvs-program-__linkt, @mvs-program-estae

== \_\_try <mvs-program-__try>
#idx("__try")

=== Format
```
#include <mvs/recovery.h>

int __try(void *func, ...);
```

=== Description
An older form of #cmd("try()"). It calls #var("func") with the arguments
that follow it under a recovery routine, in the same way.

=== Returns
0 when #var("func") returned normally, the abend code when it ended
abnormally, and a negative value when the recovery routine could not be
established.

=== Notes
The comment in the header says that #cmd("__try()") returns 0 unless the
ESTAE cannot be created. That is not so: it returns the abend code like
#cmd("try()"). Use #cmd("try()").

=== Related
@mvs-program-try

== tryrc, \_\_\_tryrc, \_\_tryrc <mvs-program-tryrc>
#idx("tryrc")
#idx("___tryrc")
#idx("__tryrc")

=== Format
```
#include <mvs/recovery.h>

unsigned ___tryrc(void);
#define tryrc() ___tryrc()
unsigned __tryrc(void);
```

=== Description
Return the result of the most recent #cmd("try()") or #cmd("__try()") on the
current task: 0 when the function returned, or the abend code. The two
functions read the same field.

=== Returns
The result, or #cmd("X'FFFFFFFF'") when the task has no task anchor.

=== Notes
#cmd("try()") sets the field to 0 when it starts and stores the abend code
when the function ends abnormally\; it leaves it 0 when the recovery
routine cannot be established. #cmd("__try()") stores its return value,
negative values included.

== abendrpt, \_\_abrpt <mvs-program-abendrpt>
#idx("abendrpt")
#idx("__abrpt")
#idx("abend", "report")

=== Format
```
#include <mvs/recovery.h>

int __abrpt(ESTAE_OP op, DUMP_OP dump);
#define abendrpt(eop,dop) __abrpt((eop),(dop))
```

=== Description
Creates, replaces or removes, according to #var("op") as for
#cmd("estae()"), a recovery routine that writes an abend report to the
operator and then lets the abend go on. The report gives the completion
code, the module and the offset of the failing instruction, the PSW, the
storage at the failing instruction, the registers and a traceback of the
save areas.

#var("dump") selects what happens to the dump afterwards:

#deflist(width: 1.35in,
  [#cmd("DUMP_SUPPRESS")], [no dump is taken.],
  [#cmd("DUMP_DEFAULT")], [the dump is left as MVS would take it.],
  [#cmd("DUMP_SNAP")], [a SNAP dump is written to the DD #cmd("SNAP"), and
    the system dump is suppressed. When the DD cannot be opened, the dump
    is left as for #cmd("DUMP_DEFAULT").],
  [#cmd("DUMP_SDUMP")], [the same as #cmd("DUMP_DEFAULT")\; the routine
    only writes a message.],
)

=== Returns
The value #cmd("estae()") returns, or -1 when #var("op") is not valid.

=== Notes
When the task is being ended, the routine writes a single message and does
nothing else.

#cmd("DUMP_SDUMP") takes no SVC dump.

=== Related
@mvs-program-estae, @mvs-program-__linkt

// -------------------------------------------------------------------------
== Authorization and Program State <mvs-program-apf>

#idx("APF authorization")
#idx("supervisor state")
#idx("PSW key")
#cmd("<mvs/apf.h>") tests and changes the authorization of the program:
APF authorization, supervisor state and the PSW key. Changing state or key
uses #cmd("MODESET") and the instruction #cmd("SPKA"), which are open only
to an APF-authorized program.

#idx("SVC 244")
A program that is not APF-authorized can obtain authorization only through
#cmd("__autask()"), which relies on SVC 244: an authorization SVC that is not
part of MVS 3.8j as IBM shipped it and must be supplied by the
installation. Whether it exists, and who may use it, is decided by the
installation.

The PSW key arguments take the values #cmd("PSWKEY0") to #cmd("PSWKEY15")
(#cmd("X'00'") to #cmd("X'F0'"), the key in the high-order four bits), or
#cmd("PSWKEYNONE") (#cmd("X'FF'")) to leave the key unchanged. A value
below #cmd("X'10'") is also accepted and taken as the key number.

== \_\_isauth, \_\_issup <mvs-program-__isauth>
#idx("__isauth")
#idx("__issup")

=== Format
```
#include <mvs/apf.h>

int __isauth(void);
int __issup(void);
```

=== Description
#cmd("__isauth()") tests with #cmd("TESTAUTH FCTN=1") whether the program is
APF-authorized. #cmd("__issup()") tests with #cmd("TESTAUTH STATE=YES")
whether it runs in supervisor state.

=== Returns
1 when it is, 0 when it is not.

=== Notes
The header declares #cmd("__isauth()") twice, with the same prototype.

== \_\_autask, \_\_uatask <mvs-program-__autask>
#idx("__autask")
#idx("__uatask")

=== Format
```
#include <mvs/apf.h>

int __autask(void);
int __uatask(void);
```

=== Description
#cmd("__autask()") makes the job step APF-authorized when it is not already,
by issuing SVC 244 with register 1 set to 1, under the protection of
#cmd("try()"). It records the change in the task anchor.

#cmd("__uatask()") reverses it, with SVC 244 and register 1 set to 0. It does
so only when the authorization was obtained by #cmd("__autask()")\; it never
removes an authorization the program had when it started.

=== Returns
#cmd("__autask()") returns 0 when the program is APF-authorized afterwards,
and the nonzero return code of #cmd("TESTAUTH") otherwise, for example when
SVC 244 is not installed or refused the request.

#cmd("__uatask()") always returns 0.

=== Notes
APF authorization of the task is not enough to call a module from an
unauthorized library as authorized\; see #cmd("__austep()") and
#cmd("clib_apf_setup()").

=== Related
@mvs-program-__austep, @mvs-program-clib_apf_setup

== \_\_austep, \_\_uastep <mvs-program-__austep>
#idx("__austep")
#idx("__uastep")
#idx("STEPLIB", "APF authorization")

=== Format
```
#include <mvs/apf.h>

int __austep(void);
int __uastep(void);
```

=== Description
#cmd("__austep()") marks the STEPLIB of the job step as an APF-authorized
library, by setting the authorization bit in its data extent block (DEB),
so that modules fetched from it run authorized. Nothing is changed when the
STEPLIB is already authorized or there is no STEPLIB.

#cmd("__uastep()") clears the bit again, but only when #cmd("__austep()") set
it.

=== Returns
0 when the program is APF-authorized, and 4, the return code of
#cmd("TESTAUTH"), when it is not\; nothing is changed then.

=== Notes
Both functions switch to key 0 and supervisor state for the change, and
return in problem state with the key of the task, whatever the state of the
caller was.

=== Related
@mvs-program-__autask, @mvs-program-__steplb

== clib\_apf\_setup <mvs-program-clib_apf_setup>
#idx("clib_apf_setup")

=== Format
```
#include <mvs/apf.h>

int clib_apf_setup(const char *pgm);
```

=== Description
Prepares a program for authorized work in one call: it makes the task
authorized with #cmd("__autask()"), the STEPLIB with #cmd("__austep()"), and
marks the module #var("pgm") and the thread entry point #cmd("CTHREAD") as
authorized in their contents directory entries with
#cmd("clib_auth_name()"). #var("pgm") is the name of the program itself, in
uppercase.

Before it marks #cmd("CTHREAD"), it makes the name known with
#cmd("clib_identify_cthread()") (see @mvs-sync-clib_identify_cthread).

=== Returns
The return code of #cmd("__autask()") or #cmd("__austep()") when that step
failed\; otherwise the return code of #cmd("clib_auth_name()") for
#cmd("CTHREAD"), normally 0. -1 when the task has no task anchor.

=== Notes
When the task was already authorized before the call, the STEPLIB is left
unchanged and only the contents directory entries are marked.

When #cmd("__autask()") had to authorize the task but the STEPLIB was already
authorized, or there is no STEPLIB, nothing is marked and the function
returns 0.

The return codes of the marking of #var("pgm") and of
#cmd("clib_identify_cthread()") are not reported.

Through #cmd("clib_identify_cthread()") the function refers to
#cmd("CTHREAD"), so a program that calls it links the thread driver, whether
or not it creates threads.

The external name is #cmd("@@APFSET").

=== Related
@mvs-program-__autask, @mvs-program-clib_auth_cde

== clib\_auth\_cde, clib\_auth\_name <mvs-program-clib_auth_cde>
#idx("clib_auth_cde")
#idx("clib_auth_name")

=== Format
```
#include <mvs/apf.h>

int clib_auth_cde(CDE *cde);
int clib_auth_name(const char *name);
```

=== Description
#cmd("clib_auth_cde()") marks the module described by the contents directory
entry #var("cde") as an authorized module from a system library, so that it
can be invoked from an authorized task. #cmd("clib_auth_name()") first finds
the entry with #cmd("clib_find_cde()").

=== Returns
0, or 4 from #cmd("clib_auth_name()") when the module is not in the job pack
queue. #cmd("clib_auth_cde()") does nothing for a #cmd("NULL") #var("cde").

=== Notes
Both switch to key 0 and supervisor state with #cmd("MODESET"), and return
in problem state. The caller must be APF-authorized, or the program ends
with abend S047.

The external names are #cmd("@@AUTCDE") and #cmd("@@AUTNAM").

=== Related
@mvs-program-clib_find_cde, @mvs-program-clib_apf_setup

== \_\_super, \_\_prob <mvs-program-__super>
#idx("__super")
#idx("__prob")

=== Format
```
#include <mvs/apf.h>

int __super(unsigned char pswkey, unsigned char *savekey);
int __prob(unsigned char pswkey, unsigned char *savekey);
```

=== Description
#cmd("__super()") switches the program to supervisor state, stores the
current PSW key in #var("savekey") when that is not #cmd("NULL"), and then
sets the PSW key to #var("pswkey") unless it is #cmd("PSWKEYNONE").

#cmd("__prob()") reverses it: in supervisor state it stores the current key
in #var("savekey"), sets the key to #var("pswkey") unless it is
#cmd("PSWKEYNONE"), and switches to problem state. In problem state it
changes nothing and stores #cmd("PSWKEYNONE") in #var("savekey").

=== Returns
0 on success, #cmd("EPERM") when the program is not APF-authorized, or the
nonzero return code of #cmd("MODESET").

=== Notes
#cmd("EPERM") is returned, not stored in #cmd("errno").

Restore the key saved by #cmd("__super()") when calling #cmd("__prob()"), or
the program goes on in the wrong key.

=== Related
@mvs-program-super_do, @mvs-program-__pswkey

== \_\_pswkey <mvs-program-__pswkey>
#idx("__pswkey")

=== Format
```
#include <mvs/apf.h>

int __pswkey(unsigned char *savekey);
```

=== Description
Stores the current PSW key in #var("savekey"), as #cmd("PSWKEY0") to
#cmd("PSWKEY15"). In problem state it switches to supervisor state for the
time it takes to read the key.

=== Returns
0, or #cmd("EPERM") when the program is not APF-authorized.

=== Related
@mvs-program-__super

== super\_do, super\_key\_do <mvs-program-super_do>
#idx("super_do")
#idx("super_key_do")
#idx("__sudo")
#idx("__sukydo")

=== Format
```
#include <mvs/apf.h>

int __sudo(void *func, ...);
int super_do(void *func, ...);

int __sukydo(unsigned char pswkey, void *func, ...);
int super_key_do(unsigned char pswkey, void *func, ...);
```

=== Description
#cmd("super_do()") calls #var("func") with the arguments that follow it in
supervisor state, and returns to the state of the caller afterwards.
#cmd("super_key_do()") also sets the PSW key to #var("pswkey") for the call,
unless it is #cmd("PSWKEYNONE"), and restores the caller's key afterwards.

#cmd("__sudo") and #cmd("super_do()") are the same function,
#cmd("@@SUDO")\; #cmd("__sukydo") and #cmd("super_key_do()") are
#cmd("@@SUKYDO").

=== Returns
The return value of #var("func"), or the nonzero return code of
#cmd("MODESET") when the switch failed.

=== Notes
Neither function checks the authorization of the caller. A program that is
not APF-authorized ends with abend S047.

=== Related
@mvs-program-__super, @mvs-program-__loadhi

== \_\_steplb <mvs-program-__steplb>
#idx("__steplb")

=== Format
```
#include <mvs/apf.h>

void *__steplb(void);
```

=== Description
Returns the address of the DCB that MVS opened for the STEPLIB or JOBLIB
of the job step, taken from the TCB.

=== Returns
The DCB address, or #cmd("NULL") when the step has no step or job library.

=== Notes
The DCB can be passed to #cmd("__load()") and #cmd("__link()") to search that
library only. The external name is #cmd("@@STEPLB").

=== Related
@mvs-program-__load

// -------------------------------------------------------------------------
== TSO Command Processors <mvs-program-tso>

#idx("TSO", "command processor")
#idx("CPPL")
A program that runs as a TSO command processor receives a command
processor parameter list (CPPL) instead of a #cmd("PARM") string. The
start-up code keeps it in the program properties area, and #cmd("tsocmd()")
uses it to call another command processor with a parameter list of its
own.

== tsocmd, tsocmdf <mvs-program-tsocmd>
#idx("tsocmd")
#idx("tsocmdf")

=== Format
```
#include <mvs/tso.h>

int tsocmd(const char *pgm, const char *buf);
int tsocmdf(const char *pgm, const char *fmt, ...);
```

=== Description
#cmd("tsocmd()") calls the command processor #var("pgm") with #cmd("LINK"),
as TSO would for the command #var("pgm") #var("buf"). It builds a command
buffer holding the command name and the operands #var("buf"), and a copy of
the caller's CPPL that points to it. The command name is stored in the
environment control table (ECT) as the primary command, and the
no-operands switch of the ECT is set when #var("buf") is #cmd("NULL") or
empty.

#cmd("tsocmdf()") formats the operands from #var("fmt") and the arguments
that follow it, as #cmd("printf()") does, and calls #cmd("tsocmd()").

=== Returns
The return code of the command processor, -1 when it could not be called,
and 8 when the program has no CPPL, that is, was not itself started as a
TSO command processor, or when #var("pgm") is #cmd("NULL") or storage
cannot be obtained.

=== Notes
Each case that returns 8 is also reported to the operator.

The ECT belongs to the TSO session. #cmd("tsocmd()") leaves the primary
command name and the no-operands switch as it set them.

#cmd("tsocmdf()") formats into a buffer of 1024 bytes and truncates a longer
result.

A program started in batch with #cmd("EXEC PGM=") or under TSO with the
#cmd("CALL") command has no CPPL.

=== Related
@mvs-program-__link
