# C startup: `@@crt0` and `@@CCALL`

This note explains the C startup ("crt") modules in `src/mvs/crt/`, what they
actually do (not just what their comments claim), and **when to use which**.

## TL;DR

There is **one** C startup, `@@CRT0` (`src/mvs/crt/@@crt0.asm`), and since
2.3.0 it is a **member of `libc.a`** (#159). Nothing names it: a C `main`
compiles into a stub that references `@@CRT0` (`EXTRN @@CRT0`), so automatic
library call brings the startup in, and cc370 from 1.4.0 puts no startfile on
the link line. libc370 2.4.0 requires that cc370 and no longer installs the
startfile copies `crt0.o`/`crt1.o` (2.3.x still shipped them, identical).
The nested startup `crtm.o` is gone too since #608 (see "`@@crtm` — removed"
below), so libc370 installs no startfile at all.

| Module | Use it for | Threads | Builds the runtime? |
|--------|------------|:-------:|---------------------|
| `@@CRT0` in `libc.a` | every standalone C program | when it calls `cthread_create*()` | yes (full) |
| none: `@@CCALL` (since 2.10.0) | C functions called from a **COBOL or assembler main program**, through `CCALL` stubs | no | yes, on the first call; `CCTERM` releases it |

Rules of thumb:

* **Threads need no decision.** The subtask driver `CTHREAD` (with `@@CTEXIT`)
  is a member of its own, `src/mvs/thread/@@cthrd.asm`. `cthread_create_ex()`
  references it hard, so it is linked exactly when the program can create a
  thread; `@@CRT0` refers to it weakly and issues `IDENTIFY EPLOC=CTHREAD`
  only when it is there. A program without threads carries neither.
* **A C module LINKed from a C program** is linked like any program, with
  `@@CRT0`: it builds its own PPA/CRT/GRT on the caller's TCB (the caller's
  PPA is saved in `PPASAVE` and back in `8(TCBFSAB)` after `@@EXITA`), its own
  lazy standard streams and environment, and inherits only the heap subpool
  (`PPAHEAPS`, #89). `__link()` and `system()` call such modules.
* **C code that must run on the caller's runtime** (what `crtm` was for) is a
  C function behind a `CCALL` stub: `@@CCALL` case 1 finds the caller's C
  frame through the save-area chain and runs the function on the caller's
  stack and GRT; it returns without `__exit()`, so nothing of the caller's is
  closed. (By design of `@@CCALL`; the LINKed-module variant of that call has
  not been measured on MVS.)
* **A program that is not C** (a COBOL main program) links no startup at all.
  The first call through a `CCALL` stub builds the runtime the way `@@CRT0`
  does, and `CALL 'CCTERM'` before `STOP RUN` releases it (#507); see
  "`@@CCALL` — the runtime for a caller that is not C" below.
* **Startup work of your own goes into `__premain()`**, not into a private
  `@@START` (cc370#10): define
  `int __premain(char *parm, char *pgmname, void **pgmr1)` (`<mvs/crt.h>`) and
  `__start()` calls it first. A stream it sets is kept, a nonzero return ends
  the program before `main()`.
* **The standard streams open on first use** (#564): `__rtinit()` only sets
  `GRTFLAG1_STDLAZY`; `__gtin()`/`__gtout()`/`__gterr()` call `__stdlz()`
  (`src/stdio/@@stdlz.c`) the first time a slot is still NULL. Each stream is
  tried once (a TRY bit, set before `fopen()`, under a lock on the GRT);
  `__exit()` sets `GRTFLAG1_STDEND` before it closes the files, so closing
  never opens one. Library code must read `grt->grtin/grtout/grterr`
  directly wherever touching a stream would be wrong.
* **So do SYSENV and the time zone** (#564 steps 3/4): `__rtinit()` sets
  `GRTFLAG2_ENVLAZY`; `getenv()`, `getenvi()`, `setenv()`, `putenv()` and
  `unsetenv()` call `__envlz()` (`src/mvs/env/@@envlz.c`), which loads the DD
  once under a lock of its own (`&grt->grtflag2`, not `&grt->grtenv`, which
  `setenv()` releases unconditionally). `__tzget()` calls `tzset()` once per
  task (`CRTFLAG_TZSET`). Library diagnostics on the always-linked paths use
  `src/internal/wtofmt.h` + `wto()`, never `wtof()`/`fprintf()`: one `wtof()`
  there links all of printf and, through `vsprintf -> @@fputc -> @@fpswt`,
  half of stdio. Check with `ld370 --map ... --xref` on an empty `main()`.

## The runtime model you need first

The startups differ in *which* of the following anchors they build, so know them
before reading the differences.

| Block | Scope | Anchored in | Found by key |
|-------|-------|-------------|--------------|
| **CLIBPPA** (`clibppa.copy`) | per program invocation | the TCB first-save-area "next" slot, `8(TCBFSAB)`, eyecatcher `'@PPA'` | reachable from the TCB |
| **CLIBCRT** (`clibcrt.copy`) | per **TCB / thread** | the `ppa->ppacrt[]` array | `crt->crttcb == current TCB` |
| **CLIBGRT** (`clibgrt.copy`) | per **process / address space** | `crt->crtgrt` and `ppa->ppagrt` | — |

Everything is located **without a global variable** (the runtime is reentrant):

* `@@PPAGET` reads `PSATOLD -> TCB -> TCBFSAB -> 8(fsa)` and checks the `'@PPA'`
  eyecatcher (it also falls back to the owner TCB and a save-area scan).
* `__crtget` searches `ppa->ppacrt[]` for the entry whose `crttcb` equals the
  current TCB.
* `__grtget` is simply `crt->crtgrt`.

**The C stack is a NAB (Next Available Byte) scheme, not an MVS save-area
stack.** In the `STACK` DSECT the field `THEIRSTK` sits at offset 76. That exact
offset is what every compiled C function's prologue reads:

```
pdpprlg.macro:   L 15,76(,13)     <- the NAB
```

So each startup does `LA R0,MAINSTK; ST R0,THEIRSTK` to set the initial NAB to
the start of its GETMAIN'd stack region. Every C call then bumps the NAB to
carve out its frame. `MAINSTK` is that stack region.

## `@@CRT0` — the full startup

In order:

1. `SAVE`, set base register, keep `R1` (the parm pointer).
2. Size the stack: the weak extern `@@STKLEN` (a fullword) overrides the default
   `STACKLEN` if it is `>= 4096`. *This is how a program sets its stack size.*
3. Add `L'CLIBPPA`, round to a doubleword, do **one** GETMAIN (subpool 0) for
   **PPA + stack together**.
4. Build the PPA: eyecatcher `'@PPA'`, `PPASTKLN`, `PPASUBPL`; chain the save
   area; `R13` -> our save area.
5. **Hang the PPA off the TCB**: save the old `8(TCBFSAB)` in `PPASAVE`, then
   store the PPA there. This is what makes the PPA findable by
   `@@PPAGET`/`@@CRTGET`. Since #89 this step also **inherits the heap
   subpool**: if the old `8(TCBFSAB)` word validates as a PPA (nonzero, 24-bit,
   `'@PPA'` eyecatcher — the word is *unvalidated* MVS residue on the first
   `@@CRT0` of a TCB), `PPAHEAPS` (+0x22) is copied from it; otherwise it stays
   0 from the `XC`. A LINKed C module therefore allocates from its caller's
   ambient subpool, and the caller's own value is current again on return.
   `@@GETM` resolves `PPAHEAPS` per call (same validated walk, current TCB
   only) and records the subpool in the header's high byte, where `@@FREEM`
   reads it back — see `__setsp()`/`__getsp()`/`__getmsp()` in `clibos.h`.
6. Initialise the NAB (`THEIRSTK = MAINSTK`).
7. `@@CRTSET` (create the CLIBCRT for this TCB in `ppacrt[]`), `@@GRTSET`
   (create the CLIBGRT process anchor).
8. `@@CRTGET` then store `R13` into `CRTSAVE` (so `@@EXITA` can find the save
   area again).
9. Program name from RB -> CDE (`PGMNAME`); TSO job id from the JSCB.
10. `EXTRACT TIOT,TSO,PSB` -> `PPATIOT`, TSO foreground/background flags,
    `PPAPSCB` (environment detection).
11. Arguments: dereference `R1 -> A(parm)` into `ARGPTR`; set `PGMNPTR`.
12. **`IDENTIFY EPLOC=CTHREAD`, when `CTHREAD` is linked** — `WXTRN CTHREAD`,
    `ICM`, skip when zero (the `@@STKLEN` pattern). It registers the subtask
    driver as a CDE minor so that `ATTACH EP=CTHREAD` can find it. The RC is
    not checked: 4 means the name exists already (a program that IDENTIFYs it
    itself), 20 that another module in the task identified its own copy.
13. Call `@@START` -> `__start` -> `main()` -> `__exit()`. Normally never
    returns.

The `CTHREAD` subtask driver and `@@CTEXIT` (thread exit) are **not** in this
module: they are `src/mvs/thread/@@cthrd.asm`, pulled in by the hard
`EXTRN CTHREAD` in `@@ctcrtx.c`. Its `ATTACH EP=CTHREAD,DPMOD=-1` resolves
because of step 12.

Teardown is **not** in `@@CRT0` — it references `=V(@@EXITA)`, pulled from
`@@exita.asm` in `libc.a`, which does the **full** dismantle: `@@GRTRES`,
`@@CRTRES`, restore `8(TCBFSAB)` from `PPASAVE`, then FREEMAIN the PPA+stack.

Default stack: `MAINSTK DS 65536F` = **256 KB**.

## `@@crt1` — gone

Until 2.3.0 there was a second startfile, `@@crt1`: a copy of crt0 with the
`IDENTIFY` commented out, so that a program could choose "no threads". The
choice is now made by what the program links (above), and the copy is gone.

## `@@crtm` — removed (#608)

Up to 2.23.0 a second startfile, `crtm.o` (`src/mvs/crt/@@crtm.asm`), was
installed beside `libc.a`. It did not build a runtime but **joined** the one
already on the TCB: it GETMAINed only a 64 KB stack, called `@@CRTGET` for the
existing CLIBCRT (abend `U0801` when there was none), took the parameter
address in **R0** instead of `R1 -> A(parm)`, and carried its own inline
`@@EXITA` that freed only its stack. Because it ended through `__exit()` on
the caller's GRT, it ran the caller's `atexit()` functions and closed every
open file, the caller's standard streams included. It also defined `@@CRT0`,
so it had to stay outside `libc.a`, and it was the only way a second
`__start()` could run on one GRT (#105).

No known project linked it (checked 2026-10-19), and #608 removed it: the
source, the build, the install (which now deletes an old
`<sysroot>/lib/crtm.o`, like `crt0.o`/`crt1.o`), the packages. A link line
naming it fails with `crtm.o: No such file or directory`; MBT 3 reports
`startup = "crtm"` as a configuration error naming libc370#608, MBT 2
complains only in `make doctor`. Replacements: link the module normally
(`@@CRT0`), or call the C function through a `CCALL` stub (TL;DR above).

## `@@CCALL` — the runtime for a caller that is not C

`@@CCALL` (`src/mvs/crt/@@ccall.asm`) calls a C function from code with an
OS save area (#502). With no C runtime on the task it builds one on the first
call (#507), the `@@CRT0` way: PPA + stack (`@@STKLEN`), the PPA as the
"next" of the TCB's first save area (old value in `PPASAVE`), `@@CRTSET`,
`@@GRTSET`, the TSO `EXTRACT`, then `@@RTINIT` — the runtime part of
`@@START` (since #564 only the flags that let the standard streams, the
`SYSENV`/`ENVIRON` environment and the time zone be set up on first use), a
member of its own so
that no `@@START` and no `main()` are linked. `__premain()` is not called and
there is no parameter to parse. The PPA carries `PPAFLAG_CCALL`, by which
later calls, `CCTERM` and readers of `ppaflag` tell it from a C program's.

It ends through the same `@@EXITA` as `@@CRT0`: the outermost call records
its caller's save area at 4 of the PPA, so `exit()` in a C function, and
`CCTERM`, return to that caller with the environment released. Not built
when a TCB above has a C runtime (a C thread, a subtask of a C program):
U3101.

## Exit paths at a glance

| | Teardown module | GRT freed | CRT freed | PPA out of TCBFSA | FREEMAIN |
|---|---|:--:|:--:|:--:|:--:|
| `@@CRT0` | `@@exita.o` (from `libc.a`) | yes (`@@GRTRES`) | yes (`@@CRTRES`) | yes | PPA + stack |
| `@@CCALL` environment (`exit()`, `CCTERM`) | `@@exita.o` (from `libc.a`) | yes | yes | yes | PPA + stack |

`@@exita.asm` finds the PPA directly via `8(TCBFSAB)` (not through `@@PPAGET`),
relying on `@@CRT0` (or `@@CCALL`) having put it there. (The removed `crtm`
put no PPA there and therefore carried its own `@@EXITA`.)

## Note on stale comments

The CLIBCRT anchor was moved from `TCBUSER` to the `ppa->ppacrt[]` array keyed by
`crt->crttcb` (`@@crtset.c`/`@@crtget.c`/`@@crtres.c`). Some assembler comments
still say "TCBUSER" and are wrong; the field `CRTTCBU` in `clibcrt.copy` ("old
TCBUSER value") is legacy and is `crt->crttcb` ("Owning TCB") in `clibcrt.h`.
Treat the running C code, not the asm comments, as the source of truth here.
