# C startup: `@@crt0` and `@@crtm`

This note explains the C startup ("crt") modules in `src/mvs/crt/`, what they
actually do (not just what their comments claim), and **when to use which**.

## TL;DR

There is **one** C startup, `@@CRT0` (`src/mvs/crt/@@crt0.asm`), and since
2.3.0 it is a **member of `libc.a`** (#159). Nothing names it: a C `main`
compiles into a stub that references `@@CRT0` (`EXTRN @@CRT0`), so automatic
library call brings the startup in, and cc370 from 1.4.0 puts no startfile on
the link line. libc370 2.4.0 requires that cc370 and no longer installs the
startfile copies `crt0.o`/`crt1.o` (2.3.x still shipped them, identical).

| Module | Use it for | Threads | Builds the runtime? |
|--------|------------|:-------:|---------------------|
| `@@CRT0` in `libc.a` | every standalone C program | when it calls `cthread_create*()` | yes (full) |
| `crtm.o` (a startfile) | a C module entered **inside an already-running C runtime on the same TCB** (LINK/XCTL/LOAD+BALR from a C program) | no | no — it **reuses** the caller's runtime |

Rules of thumb:

* **Threads need no decision.** The subtask driver `CTHREAD` (with `@@CTEXIT`)
  is a member of its own, `src/mvs/thread/@@cthrd.asm`. `cthread_create_ex()`
  references it hard, so it is linked exactly when the program can create a
  thread; `@@CRT0` refers to it weakly and issues `IDENTIFY EPLOC=CTHREAD`
  only when it is there. A program without threads carries neither.
* **`@@CRT0` vs `crtm`** = *"do I build the runtime (top level) or inherit one
  (nested)?"*. `crtm.o` is named explicitly; an explicit object beats the
  archive, so `@@CRT0` is then not pulled.
* **Never use `crtm` as the top-level startfile.** Without a prior `@@CRT0`
  on the same TCB its unchecked `@@CRTGET` dereferences a NULL CLIBCRT and
  abends.
* **Startup work of your own goes into `__premain()`**, not into a private
  `@@START` (cc370#10): define
  `int __premain(char *parm, char *pgmname, void **pgmr1)` (`<mvs/crt.h>`) and
  `__start()` calls it first, before it opens the standard streams. A stream
  it sets is kept, a nonzero return ends the program before `main()`.

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

## `@@crtm` — minimal, nested startup

`@@crtm` is **not** "`@@CRT0` with a smaller stack". It omits fundamental setup and
therefore **requires an already-initialised C runtime on the same TCB**. What it
does *not* do, versus `@@CRT0`:

| Step | `@@CRT0` | crtm |
|------|:-----------:|:----:|
| GETMAIN for the **PPA** | yes (`+ L'CLIBPPA`) | **no** — stack only |
| Build PPA eyecatcher / `PPASTKLN` | yes | **no** |
| Anchor PPA in **TCBFSA** | yes | **no** |
| **`@@CRTSET`** (create CLIBCRT) | yes | **no** |
| **`@@GRTSET`** (create CLIBGRT) | yes | **no** |
| **`EXTRACT`** TIOT/TSO/PSB | yes | **no** |
| `@@CRTGET` | only to set `CRTSAVE` | **yes — assumes an existing CRT** |

Instead crtm calls `@@CRTGET` to fetch an **existing** CLIBCRT, saves its current
`CRTSAVE` into `OLDSAVE`, and swaps in its own save area. **`@@CRTGET` is
dereferenced without a NULL check** — if no CRT exists, the store lands in
protected low core and abends. So crtm is only valid when an outer `@@CRT0`
already put a PPA in the TCBFSA and registered a CRT for this TCB; `@@CRTGET ->
@@PPAGET` then finds the *parent's* anchors.

Two more distinctive details:

* **Non-standard linkage.** `@@CRT0` takes the parm via `R1 -> A(parm)`. crtm keeps
  **R0** (`PGMR0`) and passes it as `__start`'s first argument `p` — i.e. the
  address of the length-prefixed parm block goes **directly in R0**, one
  indirection less. This signals that crtm is entered by purpose-built caller
  code, not attached as a job step by the system.
* **Self-contained minimal `@@EXITA` inline.** Because crtm provides `@@EXITA` as
  its own `ENTRY`, the linker uses it and does **not** pull `@@exita.o` from
  `libc.a`. This inline `@@EXITA` restores `CRTSAVE` from `OLDSAVE`, FREEMAINs
  **only its own stack**, and `RETURN`s to crtm's caller. It deliberately calls
  **neither `@@GRTRES` nor `@@CRTRES`** — crtm did not create CRT/GRT/PPA, so it
  must not tear them down.

Default stack: `MAINSTK DS 16384F` = **64 KB** (a quarter of `@@CRT0`'s).

**Caveat:** crtm shares the parent's GRT, and the GRT holds the standard
streams, the list of open files, the `atexit()` table and the environment.
Since 2.4.0 `__start` opens a standard stream only when it is `NULL`, so a crtm
module starts with the parent's streams instead of opening new ones over them.
But it still ends through `__exit()`, which works on that same GRT: it runs the
parent's `atexit()` functions, **closes every open file — the parent's standard
streams included —** and frees the environment. (Read from `@@exit.c` and
`@@start.c`; not measured on MVS.) crtm fits where the nested module's end may
end the caller's C I/O as well, not for "call one C function and carry on".

## Exit paths at a glance

| | Teardown module | GRT freed | CRT freed | PPA out of TCBFSA | FREEMAIN |
|---|---|:--:|:--:|:--:|:--:|
| `@@CRT0` | `@@exita.o` (from `libc.a`) | yes (`@@GRTRES`) | yes (`@@CRTRES`) | yes | PPA + stack |
| crtm | inline `@@EXITA` | no | no | no | own stack only |

`@@exita.asm` finds the PPA directly via `8(TCBFSAB)` (not through `@@PPAGET`),
relying on `@@CRT0` having put it there — consistent with crtm not doing so and thus
needing its own exit.

## Note on stale comments

The CLIBCRT anchor was moved from `TCBUSER` to the `ppa->ppacrt[]` array keyed by
`crt->crttcb` (`@@crtset.c`/`@@crtget.c`/`@@crtres.c`). Some assembler comments
still say "TCBUSER" and are wrong; the field `CRTTCBU` in `clibcrt.copy` ("old
TCBUSER value") is legacy and is `crt->crttcb` ("Owning TCB") in `clibcrt.h`.
Treat the running C code, not the asm comments, as the source of truth here.
