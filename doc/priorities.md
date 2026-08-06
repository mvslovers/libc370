# What to work on next

A reading of the open issues as of **2026-08-06**, ordered. Not a plan anyone is
committed to — the order encodes what is risky, what is cheap, and what is
blocked on something other than effort. Re-read it when those change.

Revised the same day, after #48, #43, #58 and #64 closed.

## Landed since this was written

**#48 — `__cs()` stored the word at `new_value`** (PR #53, #54, #56). It was
also never a compare-and-swap: the `CS` retry loop turned the instruction's
comparison into an unconditional exchange. So it is gone, replaced by `__swap()`
and a real `__cas()`, with the clobbers the inline assembler never declared and
per-function retry labels. `test/mvs/tstatom.c` covers both. Breaking — see the
CHANGELOG.

**#43 — `__dsalc()` narrated its failures to the operator** (PR #57). Its 16
calls are gone or parked, the rule is in [`consumer-notes.md`](consumer-notes.md)
— a routine reports through its return value, the console belongs to the program
— and `test/mvs/tstdsalc.c` guards it on target. The sweep over the other 80
calls was surveyed and then **decided against**; the reasoning is in the closing
comment on #43, and it comes down to one measurement: `malloc.c` and
`@@crtget.c` are pulled by practically every program, so the `WTOF` → `VWTOF` →
`WTODUMPF` → `WTODUMP` chain (~3.7 KB) is in practically every module regardless
— and both are the kind of message the issue itself carved out as worth keeping.
The sweep would have removed console noise and no footprint, at the price of 26
translation units of churn.

Three defects that survey turned up are filed as #59, #60 and #61 — none of them
needs the sweep, and they are below.

**#58 — `racf_auth()` wrote the caller's ACEE into ASXBSENV** (PR #62). It now
passes it in the RACHECK parameter list, where RAKF looks first, and the
address-space-wide ENQ that was there to make the poke survivable is gone with
it. The race is measured, not argued: a worker TCB parking NULL in ASXBSENV read
the main task's ACEE back 215 times in 6146 iterations against the old library,
and 0 times against the new one. A second defect went with the ENQ —
`racf_auth()` ignored `lock()`'s "you already have it" rc and DEQd
unconditionally, so a caller holding the ASXB lock across the call lost it; that
is what `test/mvs/tstracau.c` guards. Follow-up in the consumer: ftpd#81.

**#64 — the other two RACF entry points still had the ASXB ENQ** (PR #65).
`racf_auth()` was never the only holder. `racf_login()` bracketed itself in
`lock(asxb)` without ever touching ASXBSENV, and `racf_logout()` read the field
on entry and wrote the *observed* value back on exit — which with one TCB per
user is routinely another session's ACEE, re-pinned after its owner had moved
on. Both gone. The issue's premise that RACINIT clears ASXBSENV itself turned
out to be **false**, measured: the first cut left the field pointing at a freed
ACEE. The clear stays as a `__cas()` against the dead pointer — the library's
first use of the compare-and-swap from #48 — which is why no ENQ is needed to
make it safe. `test/mvs/tstracfl.c`, pre-fix 0008 / fixed 0000.

## Now

**#59 and #60 — two lines the #43 survey found by reading rather than counting.**
Both are cheap and neither needs a judgement call. #59: `open_vatlst()`'s
"unable to open" message has two `%s` and one argument, so `vsprintf` formats a
garbage pointer — the diagnostic can S0C4 the program it was about to report a
recoverable failure for, and it only ever runs when something is already wrong.
#60: `__txdsn()` dumps the DALMEMBR text unit to the console **on the success
path**, so every allocation of a DSN with a member name writes a hex dump to the
SYSLOG; it also dumps the text unit before checking it for NULL. #60 is host
testable, which makes it the better first issue of the two.

**#49 — `clock64()` returns milliseconds, its type says seconds.** Small once
decided, and the decision is not the maintainer's to skip: correcting the
function silently divides every existing caller's value by 1000 (rexx370 has
adapted to the current behaviour), while correcting the documentation leaves the
library with no function returning seconds at all. Whichever way, it belongs in
a release note next to the `jesprint()` breaks, not in a quiet commit.

**#61 — `__listvl()` truncates its volume list silently.** The same shape of
decision as #49, and the third of the #43 findings. A `calloc` failure mid-scan
`break`s out and returns the volumes found so far; the function returns
`VOLLIST **` and has no way to say the list is short, so a caller cannot tell 12
volumes from 40 with storage having run out at the 13th. Today the WTO is the
only signal, which is the wrong audience. Failing the whole call is the cheapest
honest answer and the one I would take, but the signature is public, so it is
the maintainer's to pick — and it is the reason that one WTO could not simply be
deleted with the rest.

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
- **mvslovers/cc370#37** — `cc370` silently drops `-Wl,--ac,1` and `ld370
  --pack` loses the AC again, so an authorized program can look linked and end
  S047 with an empty SYSPRINT. Two deploy cycles, found building the #58 probe.
- **mvslovers/ftpd#81** — the ASXBSENV/ENQ change from #58 and #64: recovery
  comments describing a mechanism `racf_auth()` no longer has. Only actionable
  now that #64 has landed; before that, removing the DEQ would have reintroduced
  a stall.
- **mvslovers/ftpd#82** and **mvslovers/httpd#135** — the rc handling #63 needs.
  Both must land *before* libc370 flips the flag bit, and both are safe against
  the old and the new library, so they can go whenever.

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
