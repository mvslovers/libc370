#import "../bookmaster/bookmaster.typ": *

= Reentrant and Reusable Programs <pg-rent>

#idx("reentrant program")#idx("reusable program")
MVS can run one copy of a load module for many callers at once, or for one
caller after another, or it can fetch a fresh copy for every call. Which of
these it may do is stated by the attributes of the module, and whether the
attributes are true is decided by the program: a module that changes its own
storage cannot be shared. This chapter explains what the attributes mean,
where a C program keeps the data it changes, what makes a C program
non-reentrant, why a module that may ever be installed in a system library
must not change its own storage at all, and how to keep changeable data out
of the module.

== What the Attributes Mean <pg-rent-attrs>

#idx("RENT")#idx("REUS")#idx("REFR")#idx("attributes", "of a load module")
The attributes are stored in the directory entry of the load module. They
are set when the module is linked and are read by MVS each time it fetches
the module. @pg-rent-attrs-tab summarizes them.

#tab(caption: [Load module attributes])[
  #table(columns: (0.9in, 1.1in, 1fr),
    [*Attribute*], [*ld370 option*], [*Meaning*],
    [none], [(the default)], [Not reusable. Every request for the module
      gets a copy of its own, fresh from the library. The module may change
      its storage as it likes.],
    [#cmd("REUS")], [#cmd("--reus")], [Serially reusable. One copy may serve
      one request after another, but only one at a time. The module must
      set up, at each entry, every piece of its storage that it changes:
      what the previous caller left behind is still there.],
    [#cmd("RENT")], [#cmd("--rent")], [Reentrant. One copy may serve any
      number of requests at the same time, from different tasks. The module
      must not change its own storage at all.],
    [#cmd("REFR")], [#cmd("--refr")], [Refreshable. The module may be
      replaced by a fresh copy at any time, even while it is in use, so it
      must not change its storage either.],
  )
] <pg-rent-attrs-tab>

ld370 marks a module neither reentrant nor reusable unless it is told to,
which is also the default of the IBM linkage editor. Each option sets exactly
the attribute it names: #cmd("--rent") does not imply #cmd("--reus"), so give
both for what the IBM linkage editor calls #cmd("RENT"). With the cc370
driver, pass them through #cmd("-Wl"), as in @pg-rent-attr-fig.

#fig(caption: [Linking a reentrant, reusable module and checking its attributes])[
  #screen(raw(read("../ex/pg-rent/attr.txt")))
] <pg-rent-attr-fig>

#idx("mbt", "rent and reus keys")
In an mbt project, declare the attributes of each module in
#cmd("project.toml") with the keys #cmd("rent"), #cmd("reus") and
#cmd("refr") of its #cmd("[[module]]") table. A key that is declared is
passed to ld370 in both directions (#cmd("rent = false") becomes
#cmd("--norent")), so the module does not depend on the default of the
linker. The options of ld370 are described in the _cc370 Command
Reference_, Chapter 3, “The ld370 Command”.

*An attribute is a promise, not a protection.* ld370 sets the bit you ask
for and does not look at the code. A module that is marked #cmd("RENT") and
changes a static variable links without complaint and runs correctly as long
as one caller at a time uses it. When MVS gives the same copy to two tasks at
once, both change the same variable -- this has been seen with a server that
ran three of its worker tasks in one copy of such a module. The failures
that follow are intermittent and look like anything but a wrong attribute.

== Where a C Program Keeps Its Data <pg-rent-data>

#idx("static data", "in the load module")
A C program has three places for data, and only one of them is part of the
load module:

#deflist(width: 1.35in,
  [Automatic storage], [The variables declared inside a function without
    #cmd("static"). They live in the frame of the function on the C stack,
    which the start-up obtains for each run of the program
    (@pg-startup-stack). Every run has a stack of its own.],
  [The heap], [Storage from #cmd("malloc()") and its relatives, obtained
    from MVS while the program runs (@pg-storage). It belongs to the run that
    obtained it.],
  [Static storage], [Variables declared outside a function, variables
    declared #cmd("static") inside one, and string literals. cc370 places all
    of them in the load module itself, beside the code.],
)

@pg-rent-data-fig shows a small source and the assembler that cc370 makes
of it. Both static variables and the global pointer are assembled into the
control section of the module: #cmd("limit") as the constant #cmd("@V1"),
#cmd("count") as the storage #cmd("@V2") at the end, and #cmd("msg") as
#cmd("MSG"), which holds the address of the literal #cmd("@@LC0"). The
function #cmd("next") stores into #cmd("@V2") with an #cmd("ST")
instruction -- that is, into the module.

#fig(caption: [Static data is assembled into the module])[
  #grid(columns: (auto, auto), column-gutter: 0.25in,
    code(read("../ex/pg-rent/data.c"), size: 7pt),
    code(read("../ex/pg-rent/data.s"), size: 6.5pt))
] <pg-rent-data-fig>

Nothing in the assembler output tells a constant from a variable: a
#cmd("static const") table is assembled exactly like a #cmd("static")
table. The difference is only in the source, where #cmd("const") tells the
compiler, and the reader, that nothing stores into it.

== What Makes a Program Non-Reentrant <pg-rent-nonrent>

#idx("reentrant program", "what breaks it")
A C program is reentrant when nothing it runs stores into static storage.
Reading static storage is always allowed. The usual offenders are:

- *A global or static variable that is changed:* a counter, a "first time"
  flag, a cached result, a buffer declared #cmd("static") so that a function
  can return its address. A #cmd("static") variable inside a function is just
  as much part of the module as one outside.
- *A global pointer that is changed.* #cmd("const char *msg") is a variable
  pointer to constant characters: assigning another string to #cmd("msg")
  stores into the module. Write #cmd("const char *const msg") for a pointer
  that is never changed.
- *A string literal that is written to.* Literals are in the module too, so
  #cmd("strtok()") on a literal, or #cmd("putenv()") with a literal (which
  writes into its argument for a moment), stores into the module.
- *Data in a header.* A #cmd("static") variable defined in a header is
  defined again in every source that includes it, and each copy is in the
  module.
- *An assembler routine that keeps its save area or work area in its own
  control section.* @pg-asm shows how to write one that does not.

#idx("libc370", "reentrancy")
The library does not stand in the way. It keeps its own state in the
run-time anchors that the start-up builds for each program and each task,
not in static storage, so calling a library function does not by itself
make a program non-reentrant. Two cases need care:

- Functions such as #cmd("strtok()"), #cmd("rand()"), #cmd("localtime()") and
  #cmd("asctime()") keep their state or their result once for each _task_,
  so a second call in the same task overwrites the result of the first.
  #cmd("inet_ntoa()") keeps its result in one buffer for the whole program,
  which a call from any task overwrites\; programs with more than one task
  use #cmd("inet_ntop()") (@pg-sockets).
- #cmd("__idcams()") is written in assembler and keeps its request and save
  areas in its own module. Do not use it in a program that is to be
  reentrant\; use #cmd("idcams()") instead (@pg-services).

The start-up reads the external variable #cmd("__stklen") (@pg-startup-stack)
but never writes it, so it does not make a program non-reentrant either.

#idx("reusable program", "initial values")
A serially reusable program has a different problem. Its static variables
receive their initial values when the module is fetched, not when the
program is entered. In a module that is used again, a variable declared
#cmd("static int count = 0") starts the second run with the value the first
run left. Such a
program either sets every static variable at the start of #cmd("main()"),
or is not marked #cmd("REUS").

== Programs in the Link List <pg-rent-lnklst>

#idx("link list", "static data")#idx("LNKLST")#idx("S0C4 abend", "store into the module")
Where a module is fetched from matters more than its attributes. A load
module fetched from a library in the link list can read its static storage
but cannot store into it: the first store ends the program with abend
#cmd("S0C4"). The same module, byte for byte, changes its static storage
without trouble when it is fetched from a private library through a
#cmd("STEPLIB") DD statement.

@pg-rent-lnklst-tab shows how this was measured on MVS 3.8j: one program
that stores into a static variable, linked in four ways, fetched from an
APF-authorized library in the link list, and in one case also through a
#cmd("STEPLIB").

#tab(caption: [Storing into static data, by library and link])[
  #table(columns: (1.6in, 1fr, 1fr),
    [*Linked with*], [*From the link list*], [*Through STEPLIB*],
    [#cmd("ld370 --ac 1")], [reads, #cmd("S0C4") on the store], [not tried],
    [#cmd("ld370")], [reads, #cmd("S0C4") on the store], [not tried],
    [#cmd("ld370 --norent")], [reads, #cmd("S0C4") on the store], [not tried],
    [the cc370 driver], [reads, #cmd("S0C4") on the store], [stores],
  )
] <pg-rent-lnklst-tab>

So neither the authorization code, nor the #cmd("RENT") attribute, nor the
way the module was linked decides: the library does. Three consequences
follow:

+ A program that is installed into a link list library -- or that may ever
  be -- must not store into its static storage, whatever its attributes
  say. One #cmd("static int") counter is enough to end it on the first
  store.
+ The same holds for a module in the link pack area, which MVS shares
  between all address spaces in storage that programs cannot change.
+ A program tested from a private library through a #cmd("STEPLIB") does
  not show the problem. Test a module that is meant for a system library
  from that library, or keep it reentrant from the start.

Programs that are deployed into a library of their own and named on a
#cmd("STEPLIB"), which is what mbt's #cmd("make deploy") produces, are not
affected. But they may be installed elsewhere later, and the cheapest time
to keep data out of the module is when the program is written.

#note[When a program abends, the last block of its #cmd("SYSPRINT") output
has not been written: the data set is never closed. A program that may end
with #cmd("S0C4") and should report how far it got writes its progress to
the console with #cmd("wtof()") (@pg-errors), which reaches the job log at
once.]

== Keeping Changeable Data Out of the Module <pg-rent-howto>

#idx("reentrant program", "writing")
There are three places to keep data that a program changes. Prefer them in
this order.

=== Automatic Storage <pg-rent-auto>

The simplest reentrant program keeps its state in a structure declared in
#cmd("main()") and passes its address to every function that needs it.
Each run of the program then has its own copy on its own stack, and
nothing is shared that was not meant to be.

#fig(caption: [State kept on the stack and passed down])[
  #code(read("../ex/pg-rent/ctx.c"), numbers: true)
] <pg-rent-ctx-fig>

Keep large structures off the stack, which is not checked
(@pg-startup-stack): allocate them with #cmd("malloc()") in #cmd("main()")
and pass the pointer instead.

=== The Heap and the Application Anchors <pg-rent-anchor>

#idx("grtapp1")#idx("crtapp1")
Where passing a pointer through every call is impractical -- in a callback
whose signature is fixed, for example -- allocate the state on the heap and
hang its address on one of the run-time anchors. Five pointers in the
anchors are reserved for the application and never used by the library:

#deflist(width: 1.6in,
  [#cmd("grtapp1") ... #cmd("grtapp3")], [in the process anchor, returned
    by #cmd("__grtget()"): one set for the program, shared by all its
    tasks.],
  [#cmd("crtapp1"), #cmd("crtapp2")], [in the task anchor, returned by
    #cmd("__crtget()"): one set for each task.],
)

They are declared in #cmd("<mvs/crt.h>"). Set the pointer once, early in
#cmd("main()") (or at the start of a thread, for the task anchor), and free
the storage before the program ends.

=== Writable Static Areas <pg-rent-wsa>

#idx("writable static area")#idx("__wsaget")
For data that should behave like a static variable -- found from any
function, with an initial value -- use a _writable static area_. The library
keeps it on the heap, in the process anchor, and creates it the first time
it is asked for:

+ Put the initial values in a #cmd("static const") object. It stays in the
  module and is only read.
+ Call #cmd("__wsaget()") with the address of that object as the key and its
  size as the length. The first call allocates an area of that size and
  copies the initial values into it\; every later call with the same key and
  length returns the same area.
+ Work on the area, never on the initializer.

#fig(caption: [A table of counters in a writable static area])[
  #code(read("../ex/pg-rent/wsa.c"), numbers: true)
] <pg-rent-wsa-fig>

This is how the library itself keeps the table of #cmd("signal()")
handlers. Two things to keep in mind:

- *The area is shared by all tasks of the program.* #cmd("__wsaget()")
  serializes its creation, not its use. @pg-rent-wsa-fig serializes the
  update with #cmd("lock()") on the address of the area (@pg-tasks), and
  unlocks only when its own #cmd("lock()") took the lock.
- *#cmd("__wsaget()") can return #cmd("NULL")*: when there is no process
  anchor, or no storage. Check the result.

The area is freed when the program ends.

== Checking a Module <pg-rent-check>

#idx("mbt", "module-data scan")
Since the assembler output cannot tell constants from variables, the check
belongs in the source:

- mbt scans the sources of every module before it links them. Writable
  static data in a module declared #cmd("rent = true") stops the build. In
  a module that is authorized (#cmd("ac = 1")) it is a warning, because an
  authorized module fetched from an APF-authorized library may be loaded
  into storage of protection key 0, which the program cannot change either.
  #cmd("make module-data") runs the scan on its own.
- Without mbt, look for the definitions outside functions and the
  #cmd("static") definitions inside them, and ask of each whether anything
  stores into it. A #cmd("const") qualifier on the object itself -- not
  only on what a pointer points to -- is the answer that needs no further
  thought.

Check the attributes that actually reached the module with #cmd("file370 -v")
on the transport file, as in @pg-rent-attr-fig, and on MVS in the directory
entry of the member.
