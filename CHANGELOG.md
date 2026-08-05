# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

Silent-failure fixes: three paths that reported success while doing nothing,
losing data or losing storage. One breaking change (`jesprint()`).

### Changed
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
- **SVC 99 parmlist built in the caller's storage (#19).** `@@SVC99` set the
  high-order bit SVC 99 requires by modifying the caller's parameter list in
  place, relying on that storage being writable. It is not when a cc370 program
  is entered as a TSO command processor: the parameter list cc370 emits sits in
  the module's read-only static storage, so the store took an S0C4 with interrupt
  code 4. Any program opening a data set by DSN faulted at READY while working in
  batch, since `fopen()` reaches SVC 99 through
  `__fpshr`/`__fpold`/`__fpnew`/`__fpstar`. The one-word parmlist is now built in
  the work area the routine already GETMAINs; the caller's storage is never
  written. Found via RAKF's ADDUSER.
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
