# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Changed
- **BREAKING — the C startup stack GETMAIN is conditional, and a shortage
  abends U0801 by name instead of S80A from inside the SVC (#108).**
  `@@CRT0`/`@@CRT1`/`@@CRTM` obtained their stack with `GETMAIN R,LV=(0)` —
  register form, *unconditional* — so an address space that could not spare
  the ~262K abended inside SVC 10 with nothing to say. What reached the
  operator was a bare `S80A`, which on a busy system is indistinguishable
  from any other storage abend anywhere else in it: httpd logged only
  `HTTPD908E EXTERNAL PROGRAM MVSMF failed with S80A ABEND`, and neither the
  requester nor the size was recoverable from it. All three sites now issue
  `GETMAIN RC,LV=(R8),SP=(R2)` and, on a nonzero R15, `WTO` the module name
  and `ABEND 801,DUMP` — the same shape the CLIBGRT and CLIBCRT guards in
  those very files have carried since #81/#85. httpd renders that as
  `failed with U0801 ABEND`, and the console carries
  `@@CRT0 - No storage for C stack`.
  **This does not reverse #83's decision, it completes it.** #83 left these
  three GETMAINs unconditional on the grounds that "at program start there is
  no caller to fail back to", and that reasoning still holds — the program
  still dies. What changes is only that it dies *by name*. In particular the
  startup does **not** retry with a smaller stack: `PDPPRLG`
  (`maclib/pdpprlg.macro`) has no bounds check at all — it loads the NAB from
  `76(13)`, chains, and stores it back — so a short stack would run off its
  end silently rather than fail, which is strictly worse than abending.
  The refused length stays in **R8** for the dump. It is deliberately not in
  the WTO text: formatting a number needs writable storage, and at that point
  there is none — the GETMAIN just failed, and the CSECT itself is not
  writable because consumer load modules link RENT by default (mbt passes
  `--norent` only on request), which is the one place a static work area would
  turn this diagnostic into the S0C4 it is meant to explain.
  **What changes for callers:** nothing at compile time; the new abend code
  arrives at each consumer's next relink. Monitoring keyed on `S80A` from a
  libc370 program start should key on `U0801` instead.
  Verification is by inspection of the assembled expansion, not by test: a
  startup storage shortage is not reproducible from batch on MVS 3.8j. The
  listing confirms `SVC 120` with a `B'00000000'` mode byte (conditional),
  only `LR`/`SLL`/`ICM`/`SR` ahead of it (no store into the inline parameter
  list, so the RENT attribute holds), `LR 0,R8` leaving R8 intact for the
  `ST R8,PPASTKLN` that follows, and the `LTR`/`BZ`/`WTO`/`ABEND` path behind
  it. `@@estae.c:147`'s `GETMAIN RU` stays unconditional as #83 left it.

### Fixed
- **`process_exec()` and `process_job()` bound every copy by its destination
  (#111).** Both took `len = t->len & 0x7F` — a one-byte field masked but not
  bounded, so up to **127** — and `memcpy`'d that into `process_intxt()`'s
  twelve-byte locals (`jobname`, `userid`, `stepname`, `procstep`, `program`).
  Five keys reached them: `USERK` and `JOBK`, `PGMEK`, `PROCEK` and `EXECK`.
  The values then left the frame: `process_sysout()`/`process_sysin()`
  `strcpy()` `stepname`/`procstep` into **nine**-byte `JESDD` fields, and
  `process_intxt()` `strcpy()`s `userid` into a nine-byte `JESJOB.owner` — so
  one bad length byte overran a stack buffer and a heap one. Both are clamped
  to 8 now, which is what an MVS name is and exactly what those nine-byte
  fields hold. This is the clamp PR #22 (`58319b2`, "Fix S0C4 in process_dd
  for multi-digit SPACE values") gave the sibling `process_dd()` and did not
  give these two; that PR's trigger was an ordinary JCL detail and its symptom
  was an S0C4. Red→green on the host under ASAN by `test/host/tstjestx.c`,
  which `#include`s the real translation unit and hands each parser a
  destination allocated at exactly twelve bytes: pre-fix ASAN reports
  `WRITE of size 64 ... 0 bytes after 12-byte region` inside `process_job`,
  post-fix 14/14. Case (7) is also PR #22's regression guard, which it never
  had. Whether this is the mechanism behind #108 is **not** settled — it is
  fixed on its own terms.
- **`jesopen()` no longer returns a JES handle whose spool array was never
  allocated (#108).** `try_jesopen()` checked every allocation but the
  `arrayadd()` that stores the freshly opened spool handle into `jes->js`. On
  a storage shortage that call returns -1, `jes->js` stays NULL, and the
  handle was still returned looking complete — eye catcher set, `jes->cp`
  populated. `jesjob()` and `jesprint()` then evaluated `jes->js[0]`, and on
  MVS that load *succeeds*: low-address protection stops stores into page
  zero, not fetches, so they took a non-NULL value out of the PSA, walked past
  `__jsrd4()`'s own `if (!js)` test and stored through it — a store into
  nucleus-derived storage from problem state, key 8, i.e. an S0C4 whose cause
  was a GETMAIN that failed several frames earlier. Same shape as #61 and #80:
  the allocation is not the problem, the unchecked result is. `jesopen()` now
  reports the failure and returns NULL, closing the spool data set directly
  before `jesclose()` (which reaches it only through the array that does not
  exist); both indexing sites are guarded so a handle from any other source
  cannot take the same route. `jesopen.c` also gains `clibwto.h` — it was
  calling `wtof()` with no prototype in scope, which on this target decides
  linkage (#39). Verified red→green on the host by
  `test/host/tstjesop.c`, which links the real `jesopen.c`, `jesclose.c` and
  array code and injects the failure at `calloc` so the real
  `arrayadd()`/`arraynew()` pair runs and really fails: 13/18 pre-fix, 18/18
  after.

## [1.0.2] - 2026-08-12

Mostly silent failures: paths that reported success while doing nothing, losing
data, losing storage, or building against a stale compiler. Two heap defects in
the JES2 spool record walk that any foreign or truncated block could reach, a
compare-and-swap that stored the wrong word entirely, an S0C4 on open-by-DSN
from a TSO command processor, and public prototypes for four routines consumers
had to declare by hand. Eight breaking changes: `jesprint()` twice, the atomics,
`clock64()` — which finally returns the seconds its type always claimed, in one
coordinated move with `time64()` so that `time64()`'s own value does not budge —
`racf_auth()` — which stops asking for audit suppression with the bit that
means "this is a VSAM data set", and in exchange answers 4 where it answered 0 —
`__dsalc()` — which also stops narrating its failures to the operator, the
first instalment of taking the console back from the library — `malloc()`,
which can finally fail: storage shortage returns NULL with `errno = ENOMEM`
where it used to abend S878 — and the `fopen()`/dynalloc path, which follows
suit: shortage during an open fails the call instead of abending S80A. `__txdsn()` is
the second, and the plainer case: it dumped a control block on the path where
everything had worked. `__loadhi()` — the load-into-CSA path ufsd and nsf370
depend on — carries two more of the same kind: an `fclose()` on stack residue,
in supervisor state under PSW key 0 where that is cross-key corruption rather
than an abend, and an RLD walk bounded by a continuation bit instead of the
record's byte count, which relocated past the end of the module and stored an
adcon into unmapped storage. And the build stopped throwing warnings away:
`-Wuninitialized` is on, its 23 hits are cleared — five of them real — and a
warning on an otherwise successful compile now reaches the build output.

### Added
- **The malloc subpool is a runtime value (#89).** `@@GETM` no longer assembles
  subpool 0 in: it resolves the ambient heap subpool per call from `PPAHEAPS`
  (+0x22) in the current TCB's own PPA — validated exactly like `@@PPAGET`
  tier 1, no owner-TCB fallback — and records it in the high byte of the
  rounded-size header word, which is precisely the `SP||LV` pair `@@FREEM` now
  feeds the R-form FREEMAIN. A block therefore travels with its subpool and
  `free()` needs no variant; a pre-#89 header decodes as subpool 0 unchanged.
  `@@CRT0`/`@@CRT1` inherit `PPAHEAPS` from the caller's PPA when the old
  `8(TCBFSAB)` word actually validates as one (on the first CRT of a TCB it is
  unvalidated MVS residue), so a LINKed C module allocates from its caller's
  ambient subpool and the caller's value is current again on return. New API in
  `clibos.h`: `__setsp()` (set ambient, returns previous), `__getsp()`, and
  `__getmsp(size, sp)` — the explicit-subpool `__getm()` used to pin storage
  that must survive a `FREEMAIN SP=n` reclaim. **Nothing changes until someone
  calls `__setsp(n)`**: the ambient value is 0 everywhere today, cthread TCBs
  (no PPA) stay pinned to 0 by construction, and the T0 probe (`tstsubp`)
  measured subpools 1-127 as strictly per-task on MVS 3.8, so one constant
  subpool number is all httpd#154's stage 2 needs. Guard rails that came with
  it: `__getm()` now refuses a rounded size past 24 bits (it is callable
  directly and malloc's 6M cap does not protect it), and the danger inherent
  in an ambient subpool — server-lifetime storage allocated from module
  context landing in the module's subpool — is documented at the API with the
  pinning rules (`__getmsp(size, 0)` / `__setsp(0)` brackets).
- **`__cas()` — compare and swap the way the instruction does it (#48).** Stores
  `new_value` only if `*mem` is still `*expect`; returns 0 when it swapped, 1
  when it did not — and then `*expect` holds what is in memory instead, which is
  what a retry loop needs and what a plain exchange cannot tell you. `-1` for
  NULL arguments, so "did not swap" and "you passed nonsense" are
  distinguishable. This is the operation rexx370 needed when it worked around
  the broken `__cs()` by swapping a value in and back out again
  (`irx#anch.c`): between those two swaps another thread sees a value that was
  never meant to be published. One `CS` has no such window.
- **`JESPR_TRUNC` (#23).** A new `jesprint()` stop reason: a record ran past the
  end of a block, so the block is truncated or malformed and the rest of it was
  skipped. The chain is intact and the walk continues with the next block, same
  as `JESPR_NOBUF`. Purely additive — a consumer that does not know the constant
  reports nothing for it, exactly as it does today for a block it never noticed
  was bad. httpd and mvsmf should each gain one `case` in their
  `do_print_sysout_why()`.
- **A spanned-record fixture from a real spool block (#44).** Case (12) of
  `test/host/tstjesprb.c` is a byte-for-byte reconstruction of a 4000-byte
  SYSOUT record captured on MVS 3.8j: a `FIRST` part carrying 3647 bytes and
  announcing 4000, then a `LAST` part with the remaining 353 in the next block.
  It pins three things at once — that `len2` counts the payload *after* the
  2-byte prefix (which closed #29), that the parts sum to exactly the announced
  total, so the clamp added for #24 cannot fire on legitimate data, and that a
  `FIRST` part filling its block to within one byte ends that block normally
  rather than as `JESPR_TRUNC`. Also worth knowing from the capture: "spanned"
  does not mean "too big for a block" — `PRLINE.len` is one byte, so *any*
  record over 255 bytes takes the spanned form, and one that fits arrives with
  `FIRST|MIDDLE|LAST` all set at once.
- **Host regression for the spool record walk, `test/host/tstjesprb.c` (#25).**
  It links and executes the *real* `__jesprb()` — unlike `test/host/tstcmtt.c`,
  which had to hand-mirror the code under test because its TU cannot be built on
  the host. 87 checks over plain records, carriage control, a spanned line inside
  one block and across two blocks, an immediate EOB, a zero-filled block, and a
  callback that stops the walk. Two cases from #25's list are deliberately
  missing: a truncated block (#23) and a MIDDLE/LAST part with no FIRST (#24) are
  red today and land with those fixes. What the spanned cases pin is the parser's
  own contract, not JES2's format — whether `len2` on a FIRST part includes the
  2-byte total-length prefix is #29 and needs a captured block from the target.
  **The same source also runs on MVS** (`jcl/tstjesprb.jcl`): cc370 compiles it
  unchanged, because since #25 the walk needs neither assembler nor I/O. That run
  adds what a host run cannot — cc370's code generation, 24-bit pointers under
  the bounds arithmetic, and the record layouts (`sizeof(PRLINE)`,
  `sizeof(SPLINE)`, the block header offsets). It does not add the memory-safety
  verdict: there is no sanitizer on MVS 3.8j, so the host ASan run stays the gate.
  87/87 on both, COND CODE 0000 on MVS 3.8j.
- **Prototypes for `loadenv()`, `tzset()`, `__exit()` and `__svc99()` (#5).**
  All four link fine — only the declarations were missing, so consumers got
  implicit-declaration warnings and carried local `extern`s (httplua's
  `a804a8f`). `loadenv()` is now in `clibenv.h`, `tzset()` in `time.h`,
  `__exit()` in `clibcrt.h`, and `__svc99()` moved out of the `#ifdef MUSIC`
  guard in `mvssupa.h` that hid it on MVS — SVC 99 is an MVS service. No
  `#pragma linkage` on `__svc99()`: `@@SVC99` takes the standard OS parameter
  list cc370 builds for any call, which is what the implicit declarations were
  already producing. `__exit()` is deliberately not `noreturn` — control does
  not come back, but the definition ends in a plain `return`. Verified as a
  declaration-only change: every one of the 712 generated `.s` is byte-identical
  across the change except `@@ver.s`, which bakes in the git revision.

### Changed
- **The startup version stamp reads `LIBC370 1.0.2 (<commit>)`.** It was
  `libc370 v1.0.0 (<commit>)`: the module name is uppercase like every other MVS
  message on the console it is written to, and the `v` carried nothing the
  surrounding format did not already give. `libc370_version()` is otherwise
  unchanged and no consumer parses the string — it is only logged — but anything
  grepping a job log for the old form needs the new one. The example in
  `clibver.h` moved with it so the documented and the emitted form do not drift.
- **BREAKING — `fopen()`, `__aopen()` and `__svc99()` fail on storage
  shortage instead of abending (#83).** The #81 fix left three unconditional
  GETMAINs on the open/dynalloc path: the FUNHEAD `SAVE=` dynamic save area
  in `mvsmacs.macro` — used by `@@AOPEN`, `@@ACLOSE`, `@@ALINE` and
  `@@SYSTEM`, so the S80A hit before the function's first real instruction —
  `@@AOPEN`'s DCB area plus its three buffer sites, and `@@SVC99`'s work
  area. All of them are `GETMAIN RC` now, and each failure surfaces through
  the caller's existing error contract. `__aopen()` returns **-1** when even
  the save area cannot be obtained and **-12 (-ENOMEM)** from the DCB and
  buffer sites — with everything acquired up to that point released: the
  data set is closed, an already-obtained buffer freed, the DCB area freed.
  A failed open leaks nothing. `__svc99()` returns **-1**, the SVC never
  issued, the caller's request block untouched. `ropen()` already translates
  negative `__aopen()` codes into `errno`, so it reports ENOMEM without a
  change; `fopen()` returns NULL exactly as its callers always assumed. The
  `SAVE=` failure path is generic — restore the caller's registers, R15=-1 —
  which is why the same shortage also fails `fclose()`, terminal line I/O
  and `system()` cleanly instead of abending them.
  Measured red→green on MVS 3.8j: `test/mvs/tstaopn.c` (REGION=1024K)
  exhausts its region in three phases — the #81 diagnostics fire for each —
  then calls `__aopen()` and `__svc99()` directly. Pre-fix: S80A in the
  FUNHEAD GETMAIN. Post-fix: rc -1/-1, and after free-all the same
  `__aopen()` against DD:SYSUT1 succeeds, COND CODE 0000. Deliberately
  unchanged: `@@estae.c:147`'s `GETMAIN RU` (nothing sane to do when the
  recovery environment itself cannot be built) and the CRT startup GETMAINs
  (`@@crt0/1/m` stack allocation — at program start there is no caller to
  fail back to).
- **BREAKING — `malloc()` can fail: storage shortage returns NULL instead of
  abending S878 (#81).** `@@GETM` issued `GETMAIN RU` — register form,
  *unconditional* — which does not report a shortage, it abends. So every
  `if (!p)` in this library and in every consumer was dead code for the exact
  case it was written for, and a storage race in httpd/mvsmf surfaced as an
  S878 somewhere down whatever call chain allocated next (the #217 death
  spiral). `@@GETM` now issues `GETMAIN RC` and returns NULL on a nonzero
  R15, and `malloc()` sets `errno = ENOMEM` on the way out. `calloc()`,
  `realloc()` and `strdup()` already propagated NULL correctly, so the whole
  family fails the way its callers always assumed. The fix sits in `@@GETM`
  itself and not in a wrapper above it because httpd and mvsmf call
  `__getm()` directly.
  **What changes for callers:** nothing at compile time — the change is
  source-compatible and arrives at each consumer's next relink. At run time,
  allocation checks that never ran can now run; a site that does not check
  gets a NULL dereference at the point of *use* instead of an S878 at the
  point of *allocation*. The ecosystem sweep lives in #81: httpd, httplua,
  lua370, ufsd and mvsmf each have named follow-up work, and
  picozip370-app/mqtt370 build against crent370 and are not covered by this
  change at all.
  Two guards ride along because a failable allocator creates hazards ahead
  of `main()`. The CRT startup code — `@@crt0`/`@@crt1`, mainline and
  CTHREAD, plus `@@crtm` — used the `@@CRTGET` result as a base register
  without testing it; with a failable calloc behind `@@CRTSET`, that `ST`
  would land in low storage at the `CRTSAVE` offset. All five sites now test
  R15 and fail loudly (WTO + user abend **U0801**) instead of corrupting the
  PSA. And `jesiropn.c` passed an unchecked 80-byte RPL work area to
  `MODCB` — under NULL the RPL would point its record area at address 0.
  The `wtof("Out of memory, bytes needed=%u")` + save area traceback in
  `malloc()`, previously unreachable for real shortage, now fires on every
  failed allocation — it names the requester, which is exactly the
  diagnostic #217 lacked. That path allocates nothing itself, so it cannot
  recurse.
  `test/mvs/tstgetm.c` (with `jcl/tstgetm.jcl`, REGION=2048K) is the
  red→green probe: it drives its own region to exhaustion, which abends
  S878 before this change and must arrive as NULL with `errno = ENOMEM`
  after it, then frees everything and proves allocation works again. It
  also pins `malloc(0)` → NULL, the 6 MB `MAX_CHUNK` cap, and the size
  prefix at p-4 that `realloc()` reads.
- **BREAKING — `clock64()` returns seconds, not milliseconds (#49).** It
  divided the microseconds out by 1000, so it returned milliseconds while
  `clock64_t` and its prototype both said seconds. `mclock64()` is the same
  seven lines, which made `clock64()` a duplicate of it and left the library
  with no function returning seconds at all — while `time64_t`, `gmtime64()`
  and the whole `m*`/`u*` scaling family are built on seconds.
  **What changes for callers:** anything reading `clock64()` as milliseconds
  now gets 1/1000 of what it did. **Those callers move to `mclock64()`**,
  which is unchanged and always was milliseconds. Anything feeding
  `clock64()` to `gmtime64()` as seconds was ~56 000 years out and is now
  right. `time64()`, `mclock64()` and `uclock64()` are **unchanged in value**;
  so are `mtime64()`, `utime64()` and everything derived from them.
  This could not be a one-line change. `time64()` was cancelling the bug with
  a second wrong constant — `clock64()` then `/CLOCKS_PER_SEC` — so correcting
  `clock64()` alone would have moved the ×1000 error into `time64()` and
  broken every in-repo consumer instead of none. Both sites moved together:
  `time64()` is now a straight `clock64()` pass-through, exactly as
  `utime64()` is for `uclock64()` and `mtime64()` for `mclock64()`, and it no
  longer reads `CLOCKS_PER_SEC` at all. That macro describes `clock()`, which
  this library does not implement (`src/clib/clock.c` returns `-1`), and it is
  no longer referenced anywhere in `src/`.
  The quietest thing this could have broken is in the thread manager.
  `dispatch_work()` (`src/thdmgr/@@cminit.c`) subtracts two `time64()` values
  and tests the result for truth, so it is a raw second count — an assumption
  nothing in that file named, and one that costs no abend and no message when
  it is wrong: milliseconds would post every waiting worker on essentially
  every manager pass, seconds/1000 would stop the timer posts for up to ~16
  minutes. It is now written down at the site, and `test/mvs/tsttm64.c` case
  (9) guards it by reading `time64()` twice across a real two-second `STIMER`
  wait and requiring a difference of 1..3. It measured 2 before the change and
  2 after.
  The rest of that test landed one PR ahead of this one (#73) for exactly this
  reason. Nineteen cases on MVS 3.8j, COND CODE 0000 on both sides of the fix;
  the only two that moved are the two that record the unit contract —
  `clock64() == mclock64()` became `clock64() == time64()`, and
  `clock64()/1000 == time64()` became `clock64()*1000 == mclock64()`. Every
  invariant — the `gmtime64()` era check, the seconds-since-1970 magnitude,
  the µs/ms/s tier agreement, monotonicity, `difftime64()` and case (9) — is
  untouched and green on both sides, which is the evidence that `time64()` did
  not move.
  Known consumer: lua370's `os.clock()` (`src/loslib.c`) reads `clock64()`
  directly and reimplemented the same `/CLOCKS_PER_SEC` compensation; it moves
  to `mclock64()`. rexx370 migrated to `uclock64()` ahead of this
  (mvslovers/rexx370#103). No other repository in the ecosystem calls
  `clock64()` at all.
- **BREAKING — `racf_auth()` asks for `LOG=NONE` with the bit that means it,
  and an unprotected resource now answers 4 instead of 0 (#63).** The library
  set `0x10` under the name `RACHECK_FLAG1_LOG_NONE`. `0x10` is **`DSTYPE=V`**
  (`sysmac/racheck.macro:562`); `LOG=NONE` is `0x02`. So two things were true
  at once: the audit suppression the library asked for never happened, and
  every `CLASS=DATASET` check it issued told RACF the entity was a VSAM data
  set. **What changes for callers:** a resource with no profile answered
  `0` and now answers `4` — "not protected", which is SAF's other way of
  saying allowed. **Test `rc <= 4`, not `rc == 0`.** ftpd (ftpd#82) and httpd
  (httpd#135) already do; mvsmf calls only `racf_set_acee()` and is
  unaffected. A denial is unchanged at 8.
  The gate on this was never the rc — it was whether suppressing the audit
  also softens a decision, and that is now measured rather than assumed.
  `test/mvs/tstracmx.c` walks eight cells against RAKF on MVS 3.8j, each with
  five `flag1` values (`00`, `10`, `02`, `04`, `12`), counting both the rc and
  the RAKF messages the check produced:
  a user who is genuinely not permitted answers **8 with every flag value** —
  in FACILITY and in DATASET, with the ACEE in the parameter list and with it
  reached through the ASXBSENV fallback — and `ATTR` still decides, `UPDATE`
  refused against a profile granting only READ. Two answers move, and only
  two: a FACILITY resource with no profile goes 0 → 4, and the audit goes
  2 RAKF lines per check → 0. `0x04` (`LOG=NOFAIL`) behaves exactly like
  `0x02` on both counts, so there is no flag that buys the silence without
  the rc: on this RAKF they are the same trade.
  `DSTYPE=V` turned out to change no outcome anywhere in the matrix — `0x00`
  and `0x10` are identical in all forty cells, including the DATASET rows
  where `DSTYPE` is consulted — so the lie in the parameter list was a lie and
  not a defect. Worth knowing for anyone reading the old code.
  One measurement is narrower than it looks: the reference system carries a
  `DATASET *` READ profile, so no data set name on it is genuinely
  unprotected, and the DATASET rows answer 0 throughout. A system without that
  catch-all should expect 4 there too.
  While in the header: every `flag1` bit is now defined and named after the
  macro (`RACHECK_FLAG1_DSTYPE_V`, `..._LOG_NOFAIL`, `..._RACFIND`,
  `..._31BIT`, `..._ENTITY_CSA`), the three `flag2` bits the macro can set are
  defined for the first time, and the four `ATTR` values were re-checked
  against `sysmac/racheck.macro:231` and are **correct** — cell (7) confirms
  `UPDATE` on target. `racf.h` now documents the return codes it hands back.
- **BREAKING — `__dsalc()` no longer writes to the operator console, and an
  unknown `DISP=` token now fails instead of being ignored (#43).** Creating a
  data set that already exists — an ordinary outcome — put three lines on the
  console: a `wtof()`, a 20-byte hex dump of the SVC 99 request block, and this
  once per attempt for a client that retries. The rc and `S99ERROR` reach the
  caller unchanged; mvsMF already prints what a human needs (`MVSMF64E`), and it
  is the caller, not the library, that knows whether the failure was expected.
  The dump is parked under `#if 0`, the way `@@dsfree.c` already parks the same
  one. Fourteen further calls in the routine went with it — thirteen
  `Invalid …` reports and a duplicate of the out-of-storage message `malloc()`
  writes itself. Eleven of the fourteen sat on a path that already returned an
  rc, so nothing that reached the caller changed. Three did **not**: an
  unrecognized token in any of the three
  `DISP=` positions used to be reported to the console while `err` stayed 0, so
  the allocation went ahead with no disposition text unit at all — a WTO in place
  of a return code. `__dsalc()` now returns 1 there. A caller passing a
  disposition libc370 does not parse (`DISP=(NEW,PASS)` — `PASS` is not in the
  list) gets a failed allocation where it used to get a silently different one.
  Consumers that build the option string from user input should check their rc
  handling. `@@dsalc.o` no longer references `WTOF`/`WTODUMPF`, and a minimal
  module that calls `__dsalc()` links 1,076 bytes smaller — though the WTO chain
  itself stays, because `malloc.c` and `@@crtget.c` still pull it into everything.
  `test/mvs/tstdsalc.c` is the guard, run on MVS 3.8j against both libraries:
  the pre-fix module returns 0 from all three `DISP=` cases and reports the DD it
  allocated anyway (COND CODE 0008), the fixed one returns 1 (0000). Case (6) is
  the issue's own scenario, and since "no WTO" cannot be asserted from inside the
  program it brackets the call with two WTOs of its own — in the pre-fix job log
  four `__dsalc` lines sit between the markers, in the fixed one they are
  adjacent. The rule behind this is written down in
  [`doc/consumer-notes.md`](doc/consumer-notes.md); the remaining live calls
  across the library are the rest of #43.
- **The four counters and `cthread_wait()` declare what their assembler writes
  (#55).** `__inc()`, `__uinc()`, `__dec()`, `__udec()` and `cthread_wait()`'s CS
  block all load R0 and R1 and told the compiler nothing — safe today because
  cc370 happens not to want those registers across the asm, which is a register
  allocator's mood rather than a guarantee. `cthread_wait()` is the telling one:
  its *first* asm block declares `"1", "14", "15"`, the CS block below it
  declared nothing. Their retry labels (`AGAIN`, and `INCIT`/`SWAPIT` in the
  inc/dec pair) were file-scope in the generated assembler, so a second inline
  loop in any of those files — or merging the near-identical TUs, which is
  tempting — would have produced a duplicate symbol. Now named after their
  function. Verified as a no-op: of the 12 changed lines in each generated `.s`
  (4 in `@@ctwait.s`), **none** has a cause other than the label name. The four
  counters also gained their first tests, `test/mvs/tstatom.c` (8)-(10),
  including the documented wrap at the limits — `__inc()` at `INT_MAX` goes to
  0, not to `INT_MIN`, which is what makes them counters and not fetch-and-add.
- **BREAKING — `__cs()` is gone, and it was broken; use `__swap()` or `__cas()`
  (#48).** Two things were wrong with it and only one of them was a typo. The
  inline assembler did `L 1,0(,%2)` where `%2` holds the value, so it stored the
  word *at* `new_value` rather than `new_value`: nothing useful when that looked
  like low storage, an S0C4 when it looked like protected storage. And it never
  was a compare and swap — the `CS` retry loop turns the instruction's
  comparison into an unconditional exchange, because the caller never gets to
  say what it expected. So the exchange survives as `__swap()`, correct this
  time, and `__cas()` is the operation the name always promised. Nothing in the
  library or in httpd, mvsmf, ftpd or ufsd called it, so the rename breaks
  nobody; the one project that tried, rexx370, had already backed out to a plain
  load/store. `test/mvs/tstcs.c` became `test/mvs/tstatom.c` and covers both
  functions, including the slot-claim pattern that motivated
  `__cas()`.
- **`TSTJESLG` drives the shipping walk instead of a copy of it (#45).** The
  probe reconstructed what `jesprint()` would do with its own `scanblk()`, a
  hand-written mirror — and the mirror had drifted: it advanced a spanned part
  by `4 + len2` where the walk advances `4 + 2 + len2` past a `FIRST` part's
  length prefix. On a data set of 500-byte records it reported half the lines
  `jesprint()` actually reads, while printing "jesprint() would print N lines"
  about code it never ran. Exactly the failure `test/host/tstcmtt.c` warns about
  in its own case. Since #25 there is no need for a mirror: the probe now calls
  `__jesprb()` and reports what it emitted, the longest line (over 255 bytes
  means the record was spanned) and `JESPRB.reason` per block.
- **BREAKING — `jesprint()`'s return value is a status, not the callback's rc
  (#26).** `rc` started as 503, became 404 or 0 — and was then overwritten by
  every `prt()` call, so on a completed walk the caller received whatever the
  *last* callback returned. Three meanings in one `int`: a callback returning a
  byte count leaked it into `rc`, a data set that printed 0 lines was
  indistinguishable from one that printed 500, and a callback returning a
  *positive* value was indistinguishable from "JES2 unusable". `jesprint()` now
  returns 0, 404 or 503 and nothing else; a callback that stops the walk is
  `st->reason == JESPR_STOPPED` with its rc in `st->prtrc`, and the lines that
  went out are `st->lines`. **Consumers that read the negative rc must move to
  `st` and relink** — mvsmf `jobsapi.c` (its `RC_SPOOL_CAP` sentinel travelled
  back through `rc`) and httpd `httpjes2.c` (`if (rc < 0) goto quit`). ftpd
  already reads `st->reason`/`st->prtrc` and needs no change. The break is
  silent — the signature is unchanged, so an un-updated consumer compiles and
  simply stops noticing.
- **`jesprint()`'s record walk extracted to `__jesprb()` (#25).** The block and
  record parser moved into `src/jes/jesprb.c`, an asm-free translation unit
  (buffer in, lines out) with its own `src/jes/jesprb.h`; `jesprint()` keeps the
  `spool_read()` chain, the jobkey/dsid checks and the `EX`/`TR` translate on its
  side of the emit callback. The spanned-line state (`prbuf`/`blksize`/`linelen`)
  and the line count moved into a `JESPRB` the caller carries across blocks,
  because a spanned line legitimately continues into the next block. Behaviour is
  otherwise unchanged: the two known memory-safety defects in the walk (#23, #24)
  are still there, deliberately — they now have somewhere to be tested from.
  One side effect worth the line: `esc_print()`'s stack frame drops from 1128 to
  104 bytes. Its HTML-escaping branch has been `#if 0` since output went to
  `text/plain`, but the `char buf[1024]` it used was declared unconditionally and
  cc370 allocated it on every call. Peak stack on the print path goes from 1304
  bytes (`jesprint` + `esc_print`) to 400 (`jesprint` + `__jesprb` + `esc_print`).
- **BREAKING — `jesprint()` reports why it stopped (#21, #22, PR #31).** `rc` was
  set to 0 before the block loop and every early exit left it there: a failed
  `spool_read()`, a block belonging to another job and a block belonging to
  another dsid all returned "success, nothing printed", indistinguishable from a
  genuinely empty data set. An MVS I/O error was swallowed and reported as
  success — the reason #4 was unanalysable. `jesprint()` now fills a `JESPRST`
  out-parameter (`st` may be NULL) with the stop reason, the blocks accepted and
  lines emitted, the MTTR it stopped on and the callback's rc. `JESPR_OPENEND`
  separates a still-open data set (foreign block *after* accepted blocks) from a
  purged one (`JESPR_FOREIGN`, first block foreign) — without it a consumer would
  answer 410 Gone for a running job's log it had just read 350 lines from. The
  chain follow is now bounded (#22): a self-chaining block ends the walk with
  `JESPR_LOOP`, `JESPR_MAXBLK` caps the iterations. The print callback gains a
  `void *arg`, removing the need to route context through the per-task GRT.
  **httpd, mvsmf and ftpd must be updated and relinked.** The overloaded return
  value is deliberately unchanged; untangling it is #26. Verified on MVS 3.8j
  (JOB00321) with `test/mvs/tstjeslg.c` `PARM=',PRINT'` — on-target only, since
  `spool_read()` is a BDAM READ/CHECK and the TU carries file-scope assembler.

### Fixed
- **`__loadhi()` relocated past the end of the module (#100).** The RLD walk let
  the T bit (`x'01'` — "the next item shares these two ESD ids and is 4 bytes
  rather than 8") decide when to stop, adding the size of the *next* item rather
  than the current one. That comes out exact only because the first item always
  costs 8 and the last was assumed to have the bit clear; when the last item in a
  record has it **set**, the sum lands 4 bytes short of `rldcnt`, the loop runs
  once more, and the walk reads an item the record does not contain. What sits
  there is the point: `__aread()` does no deblocking for `RECFM=U`
  (`asm/@@aread.asm`) — it issues a READ for `BLKSIZE` and hands back the buffer,
  so behind a short record lies the tail of the previous, longer one. That
  residue reads as a perfectly plausible RLD item whose 3-byte offset can be
  anything, and `store()` wrote the adcon into unmapped storage: **S0C4,
  deterministic, no diagnostic**, and moving as soon as the module grows. An
  ld370 before mvslovers/cc370#42 emits that stale bit routinely — UFSDSSIR has
  two such records, IRXJCL 65. The walk now runs to `rldcnt`, capped at the
  length `__aread()` actually returned, and the T bit only selects the next
  item's size; a stale bit on the last item ends the loop instead of stepping
  past it, so every module an older ld370 already produced — which is everything
  now installed — relocates correctly. MVS program fetch bounds its own walk the
  same way. Two more from the same report ride along: `process_rldr()` refuses an
  offset that is not inside the module instead of storing through it and counts
  the refusals (accumulated across the module and reported **once**, with the
  total), and `__loadhi()` now **tests** `relocate_load()`'s return code, which it
  previously discarded — a module that did not relocate cleanly is freed and the
  load fails rather than being published to the caller, because half-relocated
  code in CSA is worse than no module at all. The control/text interleaving the
  report suspected is not what fails: ld370 emits control and RLD records
  separately, established by walking a real UFSDSSIR's record stream before
  touching any code. Red→green on the host over real module bytes —
  `test/host/tstrldwk.c` links and executes the real `@@loadhi.c` against the 252
  bytes at offset 27383 of an ld370-linked UFSDSSIR, one of the records whose
  last item carries the stale bit. Pre-fix ASan reports `heap-buffer-overflow,
  READ of size 1, 0 bytes after the 252-byte region` — the phantom item's flag
  byte; post-fix 8/8, 41 items walked and not 42. `fetch()`/`store()` are out of
  the test's reach (`@@loadhi.c` holds addresses in `unsigned`, correct on the
  24-bit target and truncating on a 64-bit host), so every case passes `size = 0`;
  that arithmetic is unchanged.
- **The build compiles with `-Wuninitialized`, and a warning now reaches the
  build output (#102).** #99 was not a toolchain blind spot — cc370 finds it and
  always could. Two separate things kept it from ever being seen. The flag was
  never set: `sdk/mklibc.py` compiled with `-O1` and no warning options at all,
  and in this gcc 3.4.6 `-Wall` does **not** imply `-Wuninitialized`, it has to be
  named. And the output was thrown away: `compile_c()` captured cc370's stderr
  but only looked at it when the compile had *failed*, so a warning on an
  otherwise successful compile reached nobody. Both change together — warnings
  are collected and printed after the compile pass — since setting the flag alone
  would have changed nothing observable. The 23 hits in the sources that go into
  `libc.a` are cleared, and five were real: `rand()` returned stack residue as a
  random number when `__crtget()` came back NULL (0 now); `recv()` with
  `len <= 0` never entered the read loop and then tested and returned an unset
  `rc` (0 now, which is also the right answer for a zero-length receive);
  `__fseek()` left `newpos` unset for a `whence` that is none of `SEEK_SET`,
  `SEEK_CUR` or `SEEK_END` and every use below read garbage — **an unknown
  `whence` is rejected with -1** and `SEEK_END` folds into the same if/else chain,
  so there is exactly one decision on `whence`; `fgets()` decided on residue for
  `n <= 1` — `fgets(s, 1, fp)` now does what C99 7.21.7.2 asks (store the
  terminator, return `s`) and **`n < 1` returns NULL without touching the
  buffer**, since with no room even for the terminator there is nothing that may
  be stored; and `__start()`'s `progLen` is assigned only inside the branch that
  *sets* `GRTFLAG1_TSO` but read under a test of the flag itself, which lives in
  the GRT, is address-space wide and is only ever OR'd in — a second `__start()`
  with a non-TSO parm reached the read with `progLen` unset and used it as a loop
  bound and a pointer increment. Initialized to 0 so the read is at least
  defined; the design question behind it is #105. The rest the compiler cannot
  prove (`@@estae.c`, `@@tmrid.c`, 14 in `vvscanf.c`, `tm64gmtr.c`) and are
  initialized to say so. The reporting half earned its keep immediately by
  surfacing a warning cc370 had been issuing by default all along —
  `strcpyp()` discarding qualifiers from its source pointer — left alone here
  because the fix widens a header that ships into every consumer's sysroot
  (#104). Verified two ways: 23 cc370 warnings before and 0 after over the ten
  directories that make up `libc.a`, and an independent clang pass over all 784
  TUs in `src/` agrees, its only remaining hits being three in `src/wip`, which
  is not built. `-Werror=uninitialized` is deliberately not proposed yet.
- **`__loadhi()` called `fclose()` on stack residue (#99).** `FILE *fp` was
  declared without an initializer but tested at the common exit, and it is
  assigned only after the module has been LOADed, the CDE located and the CSA
  storage obtained — so the three failure paths before that point (the LOAD
  failing, `clib_find_cde()` returning NULL, the subpool 241 GETMAIN coming back
  empty) all reached `quit:` with `fp` holding whatever the stack happened to
  contain. Those three are exactly the failures a caller most needs to see: the
  diagnostic WTO is issued, but the address space then dies of a second,
  unrelated abend before the caller can act on the return code, so a legible
  `cannot GETMAIN 25784 bytes from subpool 241` is followed by something that
  looks like a storage overlay somewhere else entirely — and whether it abends at
  all depends on stack residue, so the same error reproduces differently between
  builds. Both callers, ufsd and nsf370, run `__loadhi()` in supervisor state
  under PSW key 0, where a garbage `fp` that happens to point into mapped storage
  raises no protection exception at all: `fclose()` writes, and the failure
  degrades from an abend into silent cross-key corruption. Every other variable
  tested at `quit:` was already initialized at its declaration, so this was an
  oversight rather than intent. Verified statically, which is the right check for
  this class — the symptom depends on stack contents, so no runtime test
  reproduces it reliably, while the dataflow is decidable at compile time: three
  clang *used uninitialized* diagnostics and cc370's own
  `'fp' might be used uninitialized` before, silent after. Two dead declarations
  turned up by the same pass went with it.
- **A caught abend now costs ~nothing durable: the dead program's runtime is
  torn down (#96).**  After #93 a caught abend of a LINKed program still cost
  a fixed ~172K: 40K of ambient-subpool heap, and 132K that NO subpool
  release could reach.  The composition probe (`test/mvs/tstcrtlk.c`) pinned
  it by experiment: not abandoned module copies (a 128K-bigger inner module
  leaks the same), not RTM recovery (a local caught abend costs 0K), but the
  dead program's own runtime — above all the three stdio FILEs `@@start.c`
  eagerly fopens for every C program (`*SYSPRINT`, `*SYSTERM`, SYSIN), still
  OPEN after the abend with their DCBs and buffers deliberately pinned
  outside the ambient heap subpool.  New `__ppahrv()` (`src/clib/@@ppahrv.c`),
  called from the #93 walk for each validated abandoned PPA before its
  stack+PPA block is FREEMAINed, mirrors `__exit()` on the dead CLIBGRT:
  fclose() every FILE in the dead `grtfile` array (CLOSE is legal — same
  TCB, the task is not terminating, and fclose validates each FILE's own
  eyecatcher), free the env/wsa/devtb elements and arrays, free the
  atexit/on_exit registration arrays WITHOUT running the dead functions,
  then free the CLIBGRT and the CLIBCRTs as `@@GRTRES`/`@@CRTRES` would
  have.  Everything is validated before it is trusted (24-bit pointers,
  eyecatchers, never the survivor's GRT) — what does not validate is left
  alone, a leak instead of a corruption.  Measured red→green on MVS 3.8j:
  172K per abend steady state, 132K of it release-proof (JOB00903, RC 8 on
  the probe's 16K thresholds) → ~0K per abend (JOB00906, COND CODE 0000);
  the #93 probe's drain verdict improves from 3 to 4 of 4 1M blocks
  (JOB00907), tstsplnk and tstecbtw stay green (JOB00908/909).  Together
  with #93 the fixed per-caught-abend cost measured in mvslovers/httpd#172
  goes from ~427K to ~0.
- **`ecb_timed_waitlist()` no longer waits on an ECB nothing will post (#94).**
  The STIMER REAL failure code stored by `ERRET=SAVERC` was never read: the
  WAIT ran regardless, and for a caller-local ECB the timer exit is the only
  poster in the address space — a failed STIMER meant a task frozen in that
  WAIT forever, the shape of mvslovers/httpd#159's wedged worker, reachable
  exactly when storage is tight enough for STIMER REAL to fail (the
  httpd#154/#172 exhaustion curve).  The macro's ERRET path reaches the
  fall-through only via `LTR 15,15`, so rc is 0 exactly when the timer
  exists; on nonzero the call now unparks the plist slot from `fsa[0]`,
  skips the WAIT and returns `-rc` — no ECB is touched, callers own the
  retry policy — with one WTO per task the first time (`CRTFLAG_TMRFAIL`:
  a per-call WTO would flood the console precisely when the system is
  already starving).  `ecb_timed_wait()`/`ecb_timed_waitarray()` propagate
  the new return; `cthread_timed_wait()` keeps its contract — its callers
  loop on their own deadlines, which now bound the degradation to a tight
  poll instead of a dead worker.  The STIMER asm also became a proper
  output-operand asm: the old form used R0 without declaring it and stored
  rc through a pointer the compiler knew nothing about — harmless only while
  nobody read rc.  New probe `test/mvs/tstecbtw.c` + `jcl/tstecbtw.jcl`:
  timer-post semantics, the parked `fsa[0]` word across every call, and a
  drained-region leg (~5.7M malloc'd away, wtof-only while drained).  On a
  healthy system LSQA is fenced from the region, so STIMER survives a full
  problem-state drain — outcome B on both libcs (JOB00890 pre-fix,
  JOB00893 post-fix, both COND CODE 0000): the freeze itself needs
  httpd#159's degraded field state, and the probe documents which outcome
  it saw (a pre-#94 libc hangs at the marked call on such a system; the
  fixed one returns the error).  What the runs do prove: the guard never
  false-triggers under storage pressure, and the timer path is unchanged
  when the timer exists.
- **A caught abend in a LINKed program no longer leaks its 256K stack (#93).**
  `try()`'s retry path unhooked the dead program's PPA from `8(TCBFSAB)` (#89)
  and then discarded the address — the stack+PPA block `@@CRT0` obtained as
  one subpool-0 GETMAIN (~262K: `MAINSTK` alone is 65536 fullwords) stayed
  allocated for the life of the address space, 61% of the fixed ~427K
  per-caught-abend cost measured from the consumer side in httpd#172.
  `call()` now walks `PPASAVE` from the abandoned head back to its snapshot —
  so a dead program that itself LINKed a dead program releases the whole
  chain — validating every hop the way `@@PPAGET` does (non-zero, 24-bit,
  `PPAEYE`), and FREEMAINs each block conditionally (RC) with the
  `PPASUBPL||PPASTKLN` pair `@@EXITA` frees: garbage at `8(TCBFSAB)` frees
  nothing, and a bad request inside abend recovery is a return code, not a
  second abend.  The unhook happens before the walk, so a second abend
  re-enters the retry with the chain already popped and frees nothing twice.
  Scope stays inside #89's decision: `@@AOPEN`'s DCBs, `@@SVC99` and
  `@@ESTAE` remain untouched — the task does not terminate and MVS closes
  nothing, so handing those back would convert a leak into corruption; only
  the stack block provably has no outside pointers (its owner's RB was purged
  by RTM before the retry point).  The `__try()` twin in `@@try.c` carries
  the same walk so the copies do not drift.  New probe `test/mvs/tstppafr.c`
  (+ `tstppamd.c`, `tstppain.c`, `jcl/tstppafr.jcl`): 6 single-level plus
  3 nested caught S0C1s must cost nothing durable — the verdict counts how
  many of four 1M mallocs fit in REGION=6M, sized against the ~434K measured
  per-abend cost so neither IEFUSI generosity nor fragmentation can decide
  it — while normal returns must not double-free and garbage at `8(TCBFSAB)`
  must free nothing.  Verified red→green on MVS 3.8j: pre-fix JOB00880
  (RC 8, 0 of 4 blocks fit), post-fix JOB00882 (COND CODE 0000, 3 of 4, and
  successive inner stacks land at the SAME address — the block demonstrably
  comes back).  The out-of-scope CLIBCRT/stdio remainder (~170K per caught
  abend, the rest of httpd#172's ~427K) still leaks by design.  `tstsplnk`'s
  red control leg leaned on the pre-#93 leak and moves from five to six
  unreclaimed 1M abends — green again in JOB00883.
- **`@@AOPEN`'s buffer-1 cleanup exists again (#90).** The failure path
  "buffer 1 obtained, VBS record area not" was written as
  `FREEMAIN R,LV=(0),A=(1),SP=SUBPOOL`, which the FREEMAIN macro rejects
  (IHB019: `SP=` is not allowed with `LV=(0)`) and then MEXITs having
  generated **no code at all** — and as370 treats the severity-12 MNOTE as a
  printing no-op, so the build stayed green and the #83 failure path silently
  leaked the buffer precisely when storage was already short.  The statement
  now packs the subpool into R0's high byte (`ICM R0,8,=AL1(SUBPOOL)`), the
  idiom `@@EXITA` and `@@AREAD` already use.  Red→green on MVS 3.8j with
  `test/mvs/tstabuf.c`: a carved 22K window (the only hole in an exhausted
  region) feeds three failing opens of a RECFM=VS data set whose 28K record
  area cannot fit — pre-fix each open leaks buffer 1 into the window and an
  18K probe no longer fits (JOB00820, RC 8); post-fix the window survives
  intact (JOB00822, COND 0000), and the #83 probe `tstaopn` stays green
  (JOB00824).
- **`try()` no longer resumes with a dead LINKed program's runtime environment
  (found by #89's T4 probe).** When a C program entered through LINK abends
  under an ESTAE, its `@@EXITA` never runs, so the PPA its `@@CRT0` chained
  at `8(TCBFSAB)` stayed there after the retry — and every CRT-anchored libc
  call in the surviving caller (stdio, `__crtget()`, since #89 the ambient
  heap subpool) resolved through the dead program's environment.  Under a
  worker that keeps running, that is httpd's post-CGI-abend state today; in
  the probe it was an immediate S0C4 (JOB00790).  `___try()`'s `call()` now
  snapshots the word before dispatching the protected function and restores
  it on the retry path, so a caught abend leaves the caller's own runtime
  current.  The abandoned PPA and stack still leak (subpool 0 by #89's scope
  decision), but they are no longer *live*.  The unreachable `__try()` twin
  in `@@try.c` carries the same guard so the copies do not drift.  Verified
  red→green with `test/mvs/tstsplnk.c` (JOB00790 S0C4 → JOB00802 COND 0000,
  eight caught S0C1s, `8(TCBFSAB)` asserted after each).
- **The CRT/GRT anchor tier no longer dereferences NULL (#85).**
  `__crtget()`/`__grtget()` can return NULL — "CRT for TCB not found", and
  since #82 a failed constructor is a second route — but ~25 sites
  dereferenced the anchor unchecked: the stdio anchors (`stdin`/`stdout`/
  `stderr` themselves), the env family, `atexit()`/`on_exit()`, cthread
  push/pop/find, the mutex family, the socket table, `gmtime()`,
  `__dsalc()`, `clib_apf_setup()`, `__wsaget()` and the whole timer family.
  Every listed site now fails through its function's own contract (NULL,
  -1, a zero id, or a no-op — whatever its callers already handle). Riding
  along, the ignored-rc class: `atexit()`/`on_exit()`/`cthread_push()` pair
  their func/arg array adds with a rollback so the two arrays can never
  desynchronize; `newthread()` fails the create instead of returning a
  thread that `cthread_find()` and cleanup would never see; `@@listvl` and
  the JES job/DD walks log, free and stop instead of silently dropping the
  element; the six timer creators free the TQE and return id 0 when it
  could not be queued, instead of leaking one that would never fire. And
  the one route a *healthy* program could actually reach — `@@CRT0`/
  `@@CRT1` ignored `@@GRTSET`'s rc, leaving a program running with no GRT
  (no stdio anchors, no env) when the GRT calloc failed — is closed the
  way #82 closed the CRT route: WTO + user abend U0801 at startup.
  The NULL branches themselves require a TCB running C code without a
  CRT — the unsupported situation itself — so they are guards, not
  black-box-testable behavior (same standing as the `failed()` guard from
  #9). What is testable is that no touched happy path moved:
  `test/mvs/tstanchr.c` exercises the anchors, an env round trip, the exit
  hooks (handler firing visible as a WTO in the job log), cthread
  push/pop, the mutex family, `gmtime()` and a timer that really fires —
  COND CODE 0000 on both sides of the change.
- **`calloc()` no longer masks `nmemb * size` to 24 bits (#84).** The old
  `((nmemb * size) + 7) & 0x00FFFFF8` rounded up to 8 — fine — but also
  truncated the product to 24 bits, and there was no overflow check at all.
  Measured on MVS 3.8j before the fix: `calloc(1, 0x1000009)` returned a
  valid **16-byte** block for a 16 MB request, and `calloc(0x8001, 0x20000)`
  returned **128 KB** for a 4 GB one — no message, no NULL, the caller
  overran the heap at first use. Now any product a 32-bit `size_t` cannot
  hold (including the +7 rounding) is refused with NULL and
  `errno = ENOMEM`; the untruncated total goes to `malloc()`, whose 6 MB
  `MAX_CHUNK` cap rejects merely-huge requests, so nothing that used to
  work stops working — `test/mvs/tstcaloc.c` pins that with a zero-checked
  `calloc(1, 5M)` next to the two truncation cases (assertion-red: COND
  CODE 0008 before, 0000 after) plus the exact-2^32 wrap and the prefix
  word `realloc()` reads.
- **`open_vatlst()`'s "unable to open" diagnostic names the data set again
  (#59).** The message had two `%s` conversions and one argument, so `vwtof()`
  → `vsprintf()` formatted as a `char *` a word nothing had stored into: in the
  generated code the parameter list was two words long and `vsprintf` read a
  third. Garbage in the best case, an S0C4 in the worst — in the handler for a
  failure that had just been detected and was about to be reported cleanly by
  returning NULL. The line runs only once `fopen()` has already failed, i.e. on
  a missing or misspelled VATLST member, so the code that could turn a
  recoverable "no VATLST, carry on without comments" into an abend was the code
  that only ever ran when something was already wrong. The data set name is
  passed now — the built one, `SYS1.PARMLIB(member)`, not the string the caller
  handed in — and the `@@listvl:` prefix is gone from both live messages in the
  file, since `__func__` already names the routine. `@@listvl.c` also gains
  `#include "clibwto.h"`, which is the part worth remembering: `wtof()` was an
  implicit declaration there, and with no prototype in scope there is no format
  checking at all, which is how a two-`%s`-one-argument call could sit in a
  shipped library. Generated code is unchanged apart from internal function
  indices. A sweep of the whole library with `format(printf)` attributes
  temporarily attached to `wtof()`/`wtodumpf()`/`wtorf()` found this to be the
  only *too few arguments* in `src/` (two *too many* remain, in
  `@@abrpt.c:336` and `tm64ltmr.c:69`, where the surplus is ignored). The probe
  is `test/mvs/tstlstvl.c` with `jcl/tstlstvl.jcl`; its COND CODE only proves
  the call survived and kept the volume list, because a WTO cannot be read back
  from inside the program — the message itself is checked in the job log,
  between markers the probe writes itself. Run on MVS 3.8j against both
  libraries, and the pre-fix log is the argument in one line: for
  `__listvl(NULL, 0, "NOSUCHM")` it read
  `unable to open "NOSUCHM"` — the caller's string, still lying in the varargs
  slot, not the `SYS1.PARMLIB(NOSUCHM)` that was actually attempted — and for a
  full DSN it read `unable to open "   "`. Two calls, two different wrong
  values, which is what an unwritten word looks like. With the fix both name
  their data set, and the control window with `vatlst=NULL` stays empty. COND
  CODE 0000 either way: this one is not decided by the return code.
- **`__txdsn()` stops dumping the DALMEMBR text unit to the operator console,
  and checks the text units it builds before storing them (#60).** Every
  allocation of a data set name carrying a member —
  `__dsalc("dsn=SYS1.MACLIB(IEFZB4D0);disp=shr")`, and so every
  `fopen("DD:x(member)")` that goes through dynamic allocation — wrote a hex
  dump of the text unit to the console. Not on failure: on the success path,
  with no `if` in front of it. On MVS 3.8j the console is the SYSLOG (#4), and
  unlike #43 there is no judgement call attached — nothing had gone wrong. The
  same line held a second defect: the text unit was dumped *before* it was
  checked, so a failed `calloc` gave `wtodumpf(NULL, …)` — a plausible-looking
  dump of low storage in place of an allocation failure — and then
  `arrayadd(txt99, NULL)`. The DALDSNAM unit two lines below was passed to
  `arrayadd()` unchecked in the same way. Both are checked now, and the
  function reports through the return value every caller already reads
  (`if (err) goto quit`); a unit that was built but could not be added is freed
  rather than leaked, since `FreeTXT99Array()` only reaches what made it into
  the array. The NULL matters more than a defensive check usually would:
  `__dsalc()` ORs the high-order bit into the last array element and hands the
  list to SVC 99, so a NULL element is a text unit at address 0.
  `test/host/tsttxdsn.c` links and executes the real `@@txdsn.c` — 22/22 with
  the fix, 14/22 and eight failures against the pre-fix source. The generated
  `@@txdsn.s` no longer references `WTODUMPF`, and dropping the call also
  retires the implicit declaration it needed (the file includes neither
  `clib.h` nor `clibwto.h`), so it compiles clean under `cc370 -Wall -Werror`:
  one of the 129 translation units in #39, off the list for free.
- **`racf_login()` and `racf_logout()` stop taking the address-space-wide ASXB
  ENQ, and `racf_logout()` stops re-pinning a foreign ACEE (#64).** #58 removed
  the ENQ and the ASXBSENV poke from `racf_auth()`; it was never the only entry
  point holding either. `racf_login()` bracketed itself in `lock(asxb)` without
  ever touching ASXBSENV — RACINIT ENVIR=CREATE returns the new ACEE through the
  `ACEE=` pointer — so the ENQ bought nothing and cost every login an
  address-space-wide serialization point. `racf_logout()` did worse: it read
  ASXBSENV on entry, parked the ACEE being deleted there, and wrote the *observed
  value* back on exit. In a server with one TCB per user that observed value is
  routinely another session's ACEE, so the restore re-pinned an identity whose
  owner had already moved on — the chain in mvslovers/ftpd#64, which ftpd could
  not close from its side. The delete needs no poke: the ACEE travels in the
  parameter list at offset X'34', which is where RACINIT looks first.
  One thing did **not** survive contact with the target. The issue expected
  RACINIT to clear ASXBSENV itself when it holds the ACEE being deleted, and the
  first cut of this fix dropped libc370's hand-coded clear on that assumption.
  It does not: `test/mvs/tstracfl.c` case (4) caught the field still holding the
  dead pointer, which is worse than holding none, because the next authorization
  decision follows it. So the clear stays — as a `__cas()` against the dead
  pointer, which cannot clobber a concurrent writer and therefore still needs no
  ENQ. That is the first use in the library of the compare-and-swap added in #48.
  Verified on MVS 3.8j against both libraries: pre-fix a caller holding the ASXB
  lock loses it across `racf_login()` and `racf_logout()` (cases 2 and 3), and a
  worker TCB parking NULL in ASXBSENV reads back a foreign ACEE 14 times in 1210
  loops while the main task churns 200 login/logout pairs (case 6) — COND CODE
  0008. With the fix all three are clean and the run is 0000. Consumers see no
  API change; ftpd's ABEND-recovery DEQ and its `SITE ABEND=LOCK` hook lose their
  subject once this ships (mvslovers/ftpd#81).
- **`racf_auth()` passes the ACEE in the RACHECK plist instead of writing it
  into ASXBSENV (#58).** It used to authorize against a caller-supplied ACEE by
  parking it in ASXBSENV for the length of the RACHECK and restoring it after,
  holding an address-space-wide ENQ on the ASXB to keep concurrent callers out.
  The ENQ only serialized `racf_auth()` against other `racf_auth()` calls: any
  caller switching identity for another reason — which every multi-TCB server
  does, since data set OPEN authorizes against ASXBSENV — was not serialized
  against it, and the save/restore could then leave a foreign ACEE parked there.
  RAKF fails open when it finds zero. The RACHECK parameter list has had an ACEE
  field at offset X'18' all along, which RAKF resolves before falling back to
  ASXBSENV (`SRCLIB/ICHSFR00.hlasm:111-115`), and `racf_login()` already did the
  equivalent for RACINIT. Setting it touches nothing shared, so the check is
  race-free by construction rather than by locking; `acee == NULL` leaves the
  field zero and the ASXBSENV fallback behaves exactly as before, so callers see
  no API change. Consumers that DEQ the ASXB defensively after an ABEND inside
  `racf_auth()` (ftpd, `src/ftpd#ses.c`) keep working — the DEQ becomes a no-op —
  and can drop that cleanup once they require this version. See mvslovers/ftpd#64
  for the full failure chain.
  A second defect goes with the ENQ, and it is the one that could be proven on
  target: `lock()` returns 8 when you already hold the lock, `racf_auth()`
  ignored that and DEQd unconditionally, so a caller holding the ASXB lock
  across the call **had it released out from under them** and its own `unlock()`
  then failed. `test/mvs/tstracau.c` case (3), run on MVS 3.8j against both
  libraries: pre-fix `testlock()` comes back 0 (gone) and `unlock()` 8 (not
  ours), COND CODE 0008; fixed, 8 and 0, COND CODE 0000. `racauth.o` no longer
  references `@@LK`/`@@LKUNLK` at all.
- **The PDDB scan was bounded by the read buffer, not by the IOT (#28).**
  `jesjob()` scanned the PDDBs of an IOT from `cp->pddb1` to the end of the
  3664-byte read buffer and relied on hitting a zero `PDBDSKEY` to stop — for a
  spin IOT holding a single PDDB that meant walking 2500 bytes of whatever the
  block happened to contain, on nothing but that terminator. The IOT carries the
  real bound: `IOTPDDBP`, "OFFSET BEYOND LAST PDDB IN IOT" (`haspiot.h`). It is
  used now, treated as an upper bound on the area rather than the address past
  the last entry — measured on the target it is 1828 for an IOT whose PDDBs
  start at 908 and are 104 bytes apart, and 920 is not a multiple of 104, so
  requiring an entry to *fit* below it could drop a legitimate last PDDB.
  Coming out of the same untrusted block, a value that does not land between the
  first PDDB and the buffer is ignored in favour of the old bound, so the scan
  can only ever get tighter. Two things came with it: the buffer clamp now
  leaves room for a whole `__PDDB`, closing an over-read the old bound allowed
  for an entry starting in the last 103 bytes; and the three `spool_read()`
  calls that feed these scans have their return code checked, without which the
  new bound would be read out of the *previous* IOT still sitting in the buffer.
  Verified on MVS 3.8j by running `jesjob()` through `TSTJESLG` before and
  after: 108 DD lines over five job filters, including STCs with spin IOTs,
  byte-identical.
- **Heap over-read walking the records of a block (#23).** The loop test was
  `line->len != EOB && p < eob` — C evaluates `&&` left to right, so the record
  header was dereferenced *before* the bounds test that was supposed to protect
  it. `p` advanced by up to 258 bytes per iteration from a position only
  required to be `< eob`, so a malformed, truncated or foreign block read past
  the end of the `calloc`'d buffer — and handed those bytes to the print
  callback as a line. Every record is now measured against the end of the block
  before anything in it is read. A header that *is* readable and then points
  past the end means the block is malformed and ends that block's walk with the
  new `JESPR_TRUNC`; a tail too short to hold another header is the ordinary end
  of a block — a full block has no room for an EOB byte and a zero-padded one
  walks 3 bytes at a time into exactly that, so reporting either as truncated
  would have cried wolf on every padded block. Verified red→green under
  `-fsanitize=address`: the old walk reports `heap-buffer-overflow, READ of size
  200, 0 bytes after the 128-byte region`.
- **Heap overflow reassembling a spanned line (#24).** Two ways past the end of
  `prbuf`, both now closed. A `MIDDLE`/`LAST` part with no `FIRST` opening the
  line copied into whatever buffer an *earlier, possibly much shorter* line had
  left behind — the `!prbuf` guard only caught the case where no spanned line
  had ever been seen; the walk now tracks whether a line is actually open. And
  the parts were only checked against the announced total *after* the `memcpy`
  had already run past the end. ASan on the old walk: `WRITE of size 60` into an
  8-byte region. The check is against *this* line's announced total, which is
  now tracked separately: `prbuf`'s size is the largest total seen so far
  because the buffer only ever grows, so clamping against it would measure a
  short line against a long predecessor's buffer and let it overrun its own
  announcement by thousands of bytes unnoticed (case (11) pins it). When a block gives up, what was already assembled is handed to
  the callback as a truncated line — a visible fragment beats a line that
  silently disappears — and the reassembly state is then dropped, so the next
  block cannot append across the gap and produce a line that never existed on
  the spool. The four cases are in `test/host/tstjesprb.c` (7)-(10); run it
  under ASan, without it three of them pass against the broken walk too.
- **`__stow()` assembled to nothing (#32, PR #33).** as370 has no STOW operation
  code and there is no `stow` macro in the macro library, so the mnemonic
  produced no instruction at all: the func letter was left in R15 and returned as
  the return code (rc=195 = X'C3' = 'C'). Every PDS directory operation was a
  silent no-op and `__renmem()` never changed a directory entry. The SVC 21
  linkage is now built by hand from `SYS1.MACLIB(STOW)` and `IHBINNRA` — R1 =
  DCB, R0 = area, function encoded by *negating* those registers (LCR), not by a
  function code byte. The build no longer accepts a non-zero as370 return code:
  as370 writes an object file even when it flags statements, and `assemble()`
  only checked that the file existed, so a missing macro shipped a silently wrong
  object into `libc.a`. Verified red and green on MVS 3.8j with
  `test/mvs/tststow.c`, which forces three different STOW return codes (0/8/4)
  out of one data set — old libc returned 195/195/195 with the directory
  unchanged.
- **SVC 99 parmlist built in the caller's storage (PR #19, contributed by
  [@mainframed](https://github.com/mainframed) of
  [MVS-sysgen/RAKF](https://github.com/MVS-sysgen/RAKF)).** `@@SVC99` set the
  high-order bit SVC 99 requires by modifying the caller's parameter list in
  place, relying on that storage being writable. It is not when a cc370 program
  is entered as a TSO command processor: the parameter list cc370 emits sits in
  the module's read-only static storage, so the store took an S0C4 with interrupt
  code 4. Any program opening a data set by DSN faulted at READY while working in
  batch, since `fopen()` reaches SVC 99 through
  `__fpshr`/`__fpold`/`__fpnew`/`__fpstar`. The one-word parmlist is now built in
  the work area the routine already GETMAINs; the caller's storage is never
  written. Found through RAKF's `ADDUSER`, diagnosed and fixed in the same pull
  request — the first outside contribution to this library.
- **The build reused stale generated `.s` (#8).** `compile_c()` skipped `cc370
  -S` whenever the `.s` was newer than its `.c`, which is not a staleness test:
  a `.c` mtime says nothing about the headers it includes, the flags it was
  compiled with, or the code generator that compiled it. A fixed cc370
  (mvslovers/cc370#14) and an edited `include/*.h` both left the old `.s` in
  place, so `libc.a` kept the old object code with nothing in the build output
  to show a skip — the miscompile was only caught by reading the raw object
  bytes. Every `.c` is now compiled on every build, which costs ~7s for all 712
  and is the whole of what the skip saved; a full `make build` goes from 1.3s to
  8.2s. `make clean` additionally removes the generated `.s` (only those with a
  `.c` sibling), which `rm -rf build/sdk` never did. **If you have built
  libc370 before, rebuild: the installed `libc.a` may contain object code from
  an older compiler.**
- **`__listpd()` leaked one allocation per directory entry (#34, PR #35).** Every
  PDSLIST entry was allocated twice and only the second pointer kept, so the
  first block could be freed neither by the caller nor by `__freepd()` — one
  block lost per entry returned, on every call. The cost is larger than the entry
  size suggests: `USE_MEMMGR` is not defined, so `malloc()` takes the `__getm()`
  path, which rounds every request to `(size + 8 + 63) & ~63` — 64 bytes for a
  member with no user data, 128 with full ISPF statistics, roughly 128 KB per
  listing of a 1000-member PDS. The storage is not reclaimed at the end of the
  request either: httpd runs its CGIs with `__linkds()` on a pooled worker task
  that loops until shutdown, so subpool 0 blocks accumulate for the life of the
  address space, and because `__getm()` issues `GETMAIN RU` the exhaustion
  surfaces as an S80A rather than a NULL from `malloc()`. Removes one contributor
  to mvsmf#43. `-Wall` could not catch it — the variable is used after the second
  assignment, so no dead-store warning fires; a sweep across libc370, httpd,
  mvsmf, ufsd, ftpd and ufsd-utils found no other site.

## [1.0.1] - 2026-07-26

Recovery-path hardening. No interface or ABI change; identical behavior for
well-formed inputs.

### Fixed
- **`cmtt_get_array` MTT-walk bounds (#14, PR #15).** Both walk loops now
  validate the whole entry (`start + 10 + mtentlen <= mttendpt`) and reject a
  negative `mtentlen` (`>= 0`, so a legitimate zero-length entry still advances
  and is not dropped). Fixes the over-read (upstream of mvsmf#176) and the
  backward-jump non-termination that drove unbounded `array_add` until GETMAIN
  failed — the mechanism of the S878 on `GET /zosmf/restconsoles/v1/log`. Adds
  host regression `test/host/tstcmtt.c` (over-read, backward-jump, zero-length
  survival).
- **SDWACLUP guard on the reachable `failed()` (#16).** Mirrors the cleanup-only
  guard into `@@@try.c` (`___try`, reached by `try()`); v1.0.0's #10 guarded only
  the unreachable `@@try.c` copy. At termination the reachable exit now emits one
  CRT-free WTO and returns RC=0 instead of requesting a retry under a torn-down
  CRT. Review-only (`SDWACLUP` is RTM-set at real termination, not host-testable).
  `recovery()`/`@@abrpt.c` unchanged.

## [0.1.0] - 2026-02-16

Repository restructured for focused MVS C runtime development.

### Changed
- Moved active C modules into `src/` hierarchy:
  `clib`, `cmtt`, `time64`, `dyn75`, `jes`, `racf`, `thdmgr`
- `asm/`, `include/`, `maclib/` remain at top level

### Added
- `doc/` directory for documentation
- `samples/` directory for example programs
- `jcl/` directory for JCL procedures
- `VERSION` file (0.1.0)
- `CHANGELOG.md`

### Removed
- Legacy modules moved to `legacy` branch:
  `emfile`, `ipc`, `miniz`, `modmap`, `os`, `pdf`, `pdf2`, `pdfprt`,
  `resident`, `srb`, `svc`, `test`

See tag `v0.0.0-legacy` for the pre-restructure snapshot.
