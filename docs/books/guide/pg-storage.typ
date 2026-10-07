#import "../bookmaster/bookmaster.typ": *

// Narrow cells: ragged right.
#show table: set par(justify: false)

= Storage <pg-storage>

#idx("storage")
Storage is the resource a program on MVS 3.8j runs out of first. The
whole address space lies below 16 megabytes, the operating system takes
its share of it, and what is left for a job step is its _region_. Every C
program, its stack, its heap and every program it calls share that
region. This chapter describes where a C program's storage comes from, what
the allocation functions of the library cost, how to size the stack, and
the habits that keep a program small.

== The Address Space <pg-storage-space>

#idx("24-bit addressing")#idx("region")#idx("REGION parameter")
System/370 addresses storage with 24 bits, so no address is 16 MB or
higher. A pointer in C is 32 bits wide, as are #cmd("int"), #cmd("long")
and #cmd("size_t"), but only the low 24 bits of an address are used. A
program that is moved to MVS from a system with a larger address space may
need to be made smaller, not just recompiled.

A job step gets the storage its #cmd("REGION") parameter asks for, within
the limit the installation sets. Into the region go:

- the load module of the program, and of every program it loads or calls;
- the C stack, obtained in one piece by the start-up (@pg-startup-stack);
- every block that #cmd("malloc()") and the other functions return;
- the buffers of every open stream and the control blocks of every DD
  allocated while the program runs;
- the storage that MVS services obtain on behalf of the program.

When the region is exhausted, a request for storage fails. The library's
own requests are _conditional_: #cmd("malloc()") returns #cmd("NULL") and
#cmd("fopen()") fails with #cmd("ENOMEM"), and the program decides what to
do. A request made by MVS itself on behalf of the program may instead end
it with an abend, typically #cmd("S878") or #cmd("S80A"). If a program
fails for lack of storage, raise the #cmd("REGION") of the step before
looking for other causes.

== Allocating Storage <pg-storage-heap>

#idx("malloc")#idx("heap")#idx("GETMAIN")
#cmd("malloc()"), #cmd("calloc()") and #cmd("realloc()") obtain each block
with a #cmd("GETMAIN") of its own, and #cmd("free()") returns it with
#cmd("FREEMAIN") at once. There is no heap that the library manages: the
storage management of MVS is the heap. That has consequences for how a
program should allocate:

#tab(caption: [What a block costs])[
  #table(columns: (1.5in, 1fr),
    [Property], [Consequence],
    [Each block is rounded, with an 8-byte header, up to a multiple of 64
      bytes.], [A request for 1 byte takes 64 bytes, one for 60 bytes
      takes 128. Many small blocks waste most of their storage; allocate them as
      one array or from a pool.],
    [A block is at most 6 MB.], [A larger request returns #cmd("NULL").],
    [#cmd("realloc()") always allocates a new block and copies.], [Enlarging
      a block needs room for the old and the new block at once, and the
      address always changes. Growing a block a little at a time copies its
      contents each time.],
    [A failure writes a console message.], [Each failed #cmd("malloc()")
      writes #cmd("Out of memory, bytes needed=")#var("n") and a traceback to
      the console, and returns #cmd("NULL").],
    [#cmd("malloc(0)") is a failure.], [It returns #cmd("NULL"), sets
      #cmd("ENOMEM") and writes the console message. Test a length for 0
      before allocating it.],
    [#cmd("free()") of a pointer that did not come from #cmd("malloc()"),
      or was freed already, is not detected.], [The #cmd("FREEMAIN") fails,
      and the program abends.],
  )
] <pg-storage-cost-tab>

A block is aligned on a doubleword, which suits every type, and comes from
subpool 0 unless the program chooses another (@pg-storage-subpools).

To allocate storage economically:

+ Find out, or estimate, how much the program needs in total, and allocate
  it in as few blocks as possible.
+ For many small objects of the same kind -- records, nodes of a list,
  strings -- allocate a pool and carve the objects out of it, as
  @pg-storage-pool-fig does.
+ Grow a table by a large step, doubling it for example, not one element
  at a time, and remember that the old and the new copy exist together for
  a moment.
+ Free every block when it is no longer needed, as soon as possible.

@pg-storage-pool-fig and @pg-storage-pool-main-fig keep the lines of a data
set in pools of 32 KB. Each
line costs its length and a null character; with a block of its own, every
line of up to 55 characters would cost 64 bytes, and every longer one more.

#fig(caption: [Allocating many strings from a pool: the pool])[
  #code(read("../ex/pg-storage/pool.c").split("\n").slice(0, 47).join("\n"))
] <pg-storage-pool-fig>

#fig(caption: [Allocating many strings from a pool: the main program])[
  #code(read("../ex/pg-storage/pool.c").split("\n").slice(48).join("\n"))
] <pg-storage-pool-main-fig>

#idx("storage leak")
*Free what you allocate, even at the end.* When a program ends, the
library frees its own storage, but not the blocks the program allocated
and did not free: they belong to the task, and MVS releases them only when
the task ends. For a program that runs as a job step, that is at once. A
program that is called again and again in one address space -- by
#cmd("LINK") from a server, by #cmd("system()") or by #cmd("__link()") --
leaves its leaks behind on every call, and the server runs out of storage
in the end.

== Using Less Stack <pg-storage-stack>

#idx("stack", "use of")#idx("recursion")
The C stack has a fixed size, 256 KB unless the program asks for another
(@pg-startup-stack), and it is not checked: a program that needs more
overwrites the storage behind it. Each call takes a frame, whose size is
the #cmd("FRAME=") value of the function's #cmd("PDPPRLG") macro in the
assembler source, and gives it back on return. The deepest chain of calls
decides how much stack a program needs.

- *Large arrays.* An array declared in a function is part of its frame. A
  buffer for one record of 32,760 bytes is an eighth of the default stack.
  Allocate large buffers with #cmd("malloc()"), or declare them once, in
  #cmd("main()"), and pass a pointer down.
- *Recursion.* A recursive function needs its frame once for every level.
  Know the deepest level the data can produce; if it has no firm limit,
  as with input that the program does not control, write the function as a
  loop with an explicit stack on the heap.
- *Library functions.* Some functions of the library have large frames
  of their own. #cmd("wtof()"), for example, formats its message in a
  buffer of 4096 bytes on the stack.

Asking for a smaller stack saves storage too. A module that a server calls
many times holds its stack for every call that is active, so a module that
needs 32 KB and asks for 256 KB wastes the difference on each one.

== Subpools and GETMAIN <pg-storage-subpools>

#idx("subpool")#idx("__setsp")#idx("getmain")
#cmd("<mvs/storage.h>") gives a program more control over where its storage
comes from:

- #cmd("__setsp()") selects the subpool from which #cmd("malloc()") takes
  every later block of the task. A program that calls other code -- another
  program, a plug-in module -- can let that code allocate from a subpool of
  its own and then release the whole subpool in one step, whatever the code
  forgot to free. Each block records its subpool, so #cmd("free()") still
  works. Storage that must survive the release has to be taken from a fixed
  subpool with #cmd("__getmsp()").
- #cmd("getmain()") and #cmd("freemain()") obtain and release storage in an
  explicit subpool, cleared to zeros. Their blocks are released with
  #cmd("freemain()") only, never with #cmd("free()"), and the blocks of
  #cmd("malloc()") never with #cmd("freemain()").

Both are described in the _libc370 Library Reference_, Chapter “Program
Management and Storage”. A problem-state program may use the subpools 1 to
127.

== Static Data <pg-storage-static>

#idx("static data")
Variables declared at file scope, or #cmd("static") in a function, are part
of the load module. They cost no allocation, but they are there whether
the program needs them or not, and a large initialized array makes the load
module, and its load time, larger by its size.

Whether a load module can store into its static data depends on the
library it is fetched from, not on its RENT attribute: fetched from a
library in the link list, the module can read its static data but the
first store ends the program with abend #cmd("S0C4")\; fetched from a
private library through a STEPLIB, the same module stores without trouble.
A program that may ever be installed in the link list keeps the data it
changes in automatic storage or on the heap. @pg-rent describes how.
