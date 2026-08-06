# What to work on next

A reading of the open issues as of **2026-08-06**, ordered. Not a plan anyone is
committed to — the order encodes what is risky, what is cheap, and what is
blocked on something other than effort. Re-read it when those change.

## Now

**#48 — `__cs()` stores the word at `new_value`.** The inline assembler does
`L 1,0(,%2)` where `%2` holds a value, so the library's compare-and-swap writes
whatever lives at that address: silently nothing useful when the value looks
like low storage, an S0C4 when it looks like protected storage. The fix is one
instruction (`LR`) plus the clobbers the asm never declared. A broken
serialization primitive that is declared in a public header is the worst thing
on this list per line of code needed to remove it.

**#43 — library routines WTO diagnostics to the operator console.** `__dsalc()`
is done (PR #57): its 16 calls are gone or parked, the rule is written down in
[`consumer-notes.md`](consumer-notes.md), and `test/mvs/tstdsalc.c` guards it on
target. What is left is the sweep — **80 live calls across 26 shipped
translation units**, each needing a decision: keep (genuinely unrecoverable, no
other trace), park under `#if 0` the way most of the library already does, or
delete. Two of them decide the question for everyone and should lead the
decision table rather than trail the easy deletes in `@@64mul.c`: `malloc.c` and
`@@crtget.c` are pulled by practically every program, so the `WTOF` → `VWTOF` →
`WTODUMPF` → `WTODUMP` chain is linked into practically every module no matter
what the rest of the library does — measured while fixing `__dsalc()`, whose own
module still carries the chain for exactly that reason. Both are also *keep*
candidates on diagnostic grounds, which is the real content of the sweep.
`@@dsfree.c` is the reference row: it parks the same SVC 99 dump already. On this
system the console *is* the SYSLOG, and #4 is the story of how scarce that is.

## Next

**#49 — `clock64()` returns milliseconds, its type says seconds.** Small once
decided, and the decision is not the maintainer's to skip: correcting the
function silently divides every existing caller's value by 1000 (rexx370 has
adapted to the current behaviour), while correcting the documentation leaves the
library with no function returning seconds at all. Whichever way, it belongs in
a release note next to the `jesprint()` breaks, not in a quiet commit.

**#39 — 129 of 712 TUs compile with implicit declarations.** The biggest quiet
risk here: an implicit declaration means the compiler invents a signature, and
on this target that decides linkage. #5 was one instance of it, found because a
consumer tripped over it. Mechanical but broad — it touches hundreds of files,
so it wants a window with no other branch open, and it splits cleanly by source
directory.

## Consumer-driven, in the order someone is waiting

**#50 — catalog name in `DSLIST`.** mvsMF needs `catnm` for Zowe Explorer.
Note that `DSLIST` is a public struct consumers allocate, so growing it is a
coordinated rebuild, not a field append.

**#51 — `inet_addr()`.** ftpd parses dotted-decimal with `sscanf` today, which
drags the whole of `sscanf` into a load module for four integers. Small,
self-contained, host-testable. A good first issue for someone new.

**#52 — `dynit.h`.** Only after answering whether a second interface onto SVC 99
earns its maintenance beside the string-based `__dsalc()` that the library and
its consumers already use. If nobody is porting z/OS source *in*, the cheaper
answer is to close it.

## Deferred, and why

**#11 — `cthread` teardown DETACHes a live worker.** A real bug, labelled as
one. It sits in the shutdown/S33E area, which needs on-target ABEND work and
careful reasoning about thread lifetimes — effort, not risk, is what defers it.

**#17 — the two near-duplicate `try()` wrappers.** Only one is reachable.
Cleanup with no behaviour change, and no red→green test can be written for it,
which is exactly why it keeps losing to work that can be proven.

**#27 — JES spool is single-volume.** Cannot be tested without a second spool
volume; the reference system has one. Writing the fix blind is how latent bugs
get replaced by different latent bugs.

**#30 — read SYSOUT through PSO/SSI instead of the checkpointed IOT.** An
architecture change, not a fix. Worth doing when the JES area is otherwise
settled, and after #27 is decided — the two overlap.

## Independent of all of the above

**#37 — compile the `.c` in parallel.** Twenty minutes, touches nothing else,
turns 6.7 seconds of serial `cc370` into roughly a quarter of that. Take it
whenever the queue above feels heavy.

## Filed elsewhere today, same afternoon's work

- **mvslovers/cc370#36** — `'\n'` compiles to EBCDIC NEL (`X'15'`), not LF
  (`X'25'`). Moved out of libc370's predecessor because only the compiler can
  emit the other byte. Filed as an investigation: the first deliverable is a
  survey of who compares against `'\n'` across the projects.
- **mvslovers/mvsmf#198** — `PUT` of a data set with LRECL > 1024 smashes the
  stack (S0C1). The record length is clamped against LRECL and then copied into
  a fixed 1024-byte buffer.

## Where the crent370 issues went

crent370 is superseded by this repository and now has no open issues. All seven
were re-checked against today's code before being carried over:

| crent370 | verdict | now |
|----------|---------|-----|
| #32 `__cs()` | code unchanged, defect present | #48 |
| #33 `clock64()` | code unchanged, defect present | #49 |
| #13 `catnm` | still missing | #50 |
| #23 `inet_addr()` | still missing | #51 |
| #24 `dynit.h` | still missing | #52 |
| #26 `'\n'` → NEL | not a library issue | cc370#36 |
| #34 S228 with 2+ env vars | **not reproducible** | closed |

#34 was the one that needed measuring rather than reading. `@@start.c:87` calls
`loadenv("dd:SYSENV")` unconditionally, and six runs on MVS 3.8j — no DD, one
entry, two entries, the reporter's exact variable names, four entries of mixed
length, and the `ENVIRON` fallback, with and without a REGION on the step — all
ended CC 0000. The reporter's own setup (rexx370's `IRXJCL` built against
crent370 1.0.9) could not be reconstructed, so the door is left open in the
closing note rather than declared shut.
