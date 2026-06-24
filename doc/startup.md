# C startup variants: `@@crt0`, `@@crt1`, `@@crtm`

This note explains the three C startup ("crt") modules in `asm/`, what they
actually do (not just what their comments claim), and **when to use which**.

## TL;DR

All three define the **same** entry point `@@CRT0`. They are **mutually
exclusive startfiles** — exactly like glibc's `crt1.o`. `mklibc.py` builds them
as separate objects (`crt0.o`, `crt1.o`, `crtm.o`) that live *outside* `libc.a`,
so the linker pulls **exactly one** into any given program. "Which module do I
use?" therefore means "which startfile do I link this one program with?".

| Startfile | Use it for | Threads | Builds the runtime? |
|-----------|------------|:-------:|---------------------|
| `crt0.o`  | A standalone C program that creates threads (`cthread_create*`) | yes | yes (full) |
| `crt1.o`  | A standalone C program that does **not** create threads (the common case) | no | yes (full) |
| `crtm.o`  | A C module entered **inside an already-running C runtime on the same TCB** (LINK/XCTL/LOAD+BALR from a C program) | no | no — it **reuses** the caller's runtime |

Rules of thumb:

* **crt0 vs crt1** = *"do I need threads?"* — the only real difference is the
  `IDENTIFY EPLOC=CTHREAD` at startup.
* **crt0/crt1 vs crtm** = *"do I build the runtime (top level) or inherit one
  (nested)?"*.
* **Never use `crtm` as the top-level startfile.** Without a prior crt0/crt1 on
  the same TCB its unchecked `@@CRTGET` dereferences a NULL CLIBCRT and abends.

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

## `@@crt0` — full startup, with threads

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
   `@@PPAGET`/`@@CRTGET`.
6. Initialise the NAB (`THEIRSTK = MAINSTK`).
7. `@@CRTSET` (create the CLIBCRT for this TCB in `ppacrt[]`), `@@GRTSET`
   (create the CLIBGRT process anchor).
8. `@@CRTGET` then store `R13` into `CRTSAVE` (so `@@EXITA` can find the save
   area again).
9. Program name from RB -> CDE (`PGMNAME`); TSO job id from the JSCB.
10. `EXTRACT TIOT,TSO,PSB` -> `PPATIOT`, TSO foreground/background flags,
    `PPAPSCB` (environment detection).
11. Arguments: dereference `R1 -> A(parm)` into `ARGPTR`; set `PGMNPTR`.
12. **`IDENTIFY EPLOC=CTHREAD`** — register the embedded `CTHREAD` entry point as
    a CDE minor so that `ATTACH EP=CTHREAD` can find it.
13. Call `@@START` -> `__start` -> `main()` -> `__exit()`. Normally never
    returns.

The module also contains the `CTHREAD` subtask driver and `@@CTEXIT` (thread
exit). The thread manager relies on step 12: `@@ctcrtx.c` issues
`ATTACH EP=CTHREAD,DPMOD=-1`, which only resolves because of the `IDENTIFY`.

Teardown is **not** in crt0 — it references `=V(@@EXITA)`, pulled from
`@@exita.asm` in `libc.a`, which does the **full** dismantle: `@@GRTRES`,
`@@CRTRES`, restore `8(TCBFSAB)` from `PPASAVE`, then FREEMAIN the PPA+stack.

Default stack: `MAINSTK DS 65536F` = **256 KB**.

## `@@crt1` — crt0 minus IDENTIFY (no threads)

`@@crt1.asm` is a line-for-line copy of crt0 with exactly one functional change:
the `IDENTIFY EPLOC=CTHREAD` is commented out. Everything else — PPA, TCBFSA
anchoring, `@@CRTSET`/`@@GRTSET`, `EXTRACT`, the external `@@EXITA` — is
identical.

Consequence worth knowing: the `CTHREAD`/`@@CTEXIT` code is **still assembled
into crt1 but is dead/unreachable**. With no CDE entry, `ATTACH EP=CTHREAD`
cannot resolve the name, so `cthread_create_ex()` fails (its `attach()` wraps the
ATTACH in `try()` for exactly this reason). So **crt1 = full C runtime, thread
creation disabled.** There is essentially no size saving — the choice is purely
semantic. Use crt1 to avoid the IDENTIFY (and its failure modes, e.g. a
duplicate CDE for nested C programs) when you do not spawn threads.

Default stack: **256 KB**.

## `@@crtm` — minimal, nested startup

`@@crtm` is **not** "crt0 with a smaller stack". It omits fundamental setup and
therefore **requires an already-initialised C runtime on the same TCB**. What it
does *not* do, versus crt0/crt1:

| Step | crt0 / crt1 | crtm |
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
protected low core and abends. So crtm is only valid when an outer crt0/crt1
already put a PPA in the TCBFSA and registered a CRT for this TCB; `@@CRTGET ->
@@PPAGET` then finds the *parent's* anchors.

Two more distinctive details:

* **Non-standard linkage.** crt0 takes the parm via `R1 -> A(parm)`. crtm keeps
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

Default stack: `MAINSTK DS 16384F` = **64 KB** (a quarter of crt0).

**Caveat:** `__start` re-opens `stdout`/`stderr`/`stdin` and overwrites
`grt->grtout/...`. Because crtm shares the parent's GRT, a crtm nesting
**clobbers the parent's standard streams**. crtm fits where that is intended or
harmless (its own I/O world), not for "call one C function and keep the parent's
stdout".

## Exit paths at a glance

| | Teardown module | GRT freed | CRT freed | PPA out of TCBFSA | FREEMAIN |
|---|---|:--:|:--:|:--:|:--:|
| crt0 / crt1 | `@@exita.o` (from `libc.a`) | yes (`@@GRTRES`) | yes (`@@CRTRES`) | yes | PPA + stack |
| crtm | inline `@@EXITA` | no | no | no | own stack only |

`@@exita.asm` finds the PPA directly via `8(TCBFSAB)` (not through `@@PPAGET`),
relying on crt0 having put it there — consistent with crtm not doing so and thus
needing its own exit.

## Note on stale comments

The CLIBCRT anchor was moved from `TCBUSER` to the `ppa->ppacrt[]` array keyed by
`crt->crttcb` (`@@crtset.c`/`@@crtget.c`/`@@crtres.c`). Some assembler comments
still say "TCBUSER" and are wrong; the field `CRTTCBU` in `clibcrt.copy` ("old
TCBUSER value") is legacy and is `crt->crttcb` ("Owning TCB") in `clibcrt.h`.
Treat the running C code, not the asm comments, as the source of truth here.
