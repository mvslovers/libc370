# libc370 — Open Work, Ranked

**State lives on GitHub, not here.** `gh issue list --repo mvslovers/libc370` is
the source of truth for what is open, closed or newly filed. What this file adds
is the part the tracker cannot hold: the **order**, the reason for it, and which
items wait on a decision rather than on code.

Ordered by **measured impact on running systems** — not by age, and not by effort.
libc370 is the base library of the whole ecosystem, so a defect here is a defect
in httpd, mvsMF, ftpd, ufsd and every other consumer at once; that is what puts
some cheap items high and some expensive ones low.

*Last reconciled against the tracker: 2026-08-23, 24 issues open.*

---

## Tier 1 — now

### 1 · #80 — `__listpd()`: unbounded allocation, unchecked block length, silent truncation

Three defects in one file. **The second is the worst and appears in no heading:**
`len` comes out of the block content and becomes a loop bound over a 256-byte
stack buffer without ever being reconciled against the `fread()` result
(`@@listpd.c:32-33`). That reaches the **bounded callers too** — mvsMF
`dsapi.c:540`, ftpd `:1191` — because the filter does not protect against it.

Sorted by reach, the three defects come out in the opposite order to the issue text:

- **Defect 2 first — it is the broad one and it has no exception.** The filter is
  applied *inside* the loop, so `ftpd:1191` and `mvsmf:540` run through the same
  overrun. Clamping to the `fread()` result is one line, needs no signature change
  and no relink: doable immediately.
- **Defect 1 is narrow now** — one caller, ftpd's `LIST`/`NLST`. A `max` parameter
  or an iterator form is still the right answer, but it is a signature change, so
  it belongs to the relink round (Tier 5). It would also let mvsMF drop its
  duplicated directory parser.
- **Defect 3 is decided together with #61**, not separately (see Tier 2).

### 2 · #11 — cthread teardown force-DETACHes a live worker (S33E)

The only *observed and recurring* production failure on this list. Confirmed
2026-07-22 on a libc **after** PR #7; a relink does not help, because the
unguarded `DETACH ...,STAE=YES` is in every version.

The open question — why a worker misses the five-second window — is still open;
the fix direction in the issue is settled (`termecb` as the single source of
truth, mark STUCK instead of DETACH, leave the storage to address-space
termination). Expensive, but it is the one thing here that actually burns.

### 3 · #107 — `cthread_worker_add()` leaves the manager lock held

One line: `goto quit` → `goto unlock`. Latent in-tree (all three callers already
hold the lock), but the function is exported in `clibthdi.h`. The failure is a
silent ENQ deadlock of the entire worker pool in the address space — no abend, no
message, nothing in a dump pointing here. **Best effort-to-payoff ratio on the
whole list.**

### 4 · #108 — reclassify, do not hunt

No longer an open crash hunt. #109 and #110 are merged and the prime suspect named
in the closing comment, #111, is in `main` as `cd66e86`; every candidate in the
call tree is eliminated. The issue was renamed to *"every named suspect fixed,
needs re-verification"*.

What is left is a **verification**: re-run the `dd=1` probe against a degraded
address space on a current build. No abend means close it; an abend means reopen —
but then with evidence worth more, because four hypotheses are off the table.
Cheap.

---

## Tier 2 — campaign: unchecked allocation

### 5 · #61 — `__listvl()` silently returns a truncated volume list

Same shape as #80 defect 3. The two need **one** convention rather than two
separate decisions — apply the recommendation from #61 (variant 2: fail
completely, return NULL) to both. The caller has to check for NULL anyway, and a
short list that looks complete is worse than no list at all.

---

## Tier 3 — campaign: make the compiler see it (order matters)

### 6 · #125 — inline-asm SVC macros with partial clobber lists

The one item in this campaign with a **measured miscompile in the wild**. In ftpd
the same form put a struct pointer in R15 and kept it there across a `STIMER`; the
next iteration tested whatever the SVC had left behind, read a field from that
address and exited — a wait loop returning after one pass with none of its exit
conditions true (`mvslovers/ftpd#113`).

**The bare half already landed** (PR #134): ten statements that declared no
clobbers at all now carry `"0","1","14","15"`. Six of the seven files generate
byte-identical code and `@@cmterm.c` gets *better*, so the remaining work costs
nothing either.

What is left, and why the issue stays open, is the **partial** lists — each
missing a register the SVC can destroy:

- `"0","1"` — `@@75acce.c`, `@@75recv.c`, `@@75selx.c`, `@@75send.c`
- `"0","1","15"` — `@@75conn.c`, `@@enqdeq.c` ×2
- `"1","14","15"` — `@@ctwait.c`, `@@ecbwt.c`, `@@ecbpst.c`, `@@vsclos.c`,
  `@@vsopen.c`, `jesiropn.c`, and `os/osbopen.c`, `osbclose.c`, `osdopen.c`,
  `osdclose.c`, `osxopen.c`, `osxclose.c`, `osxread.c`, `osxwrite.c`

A wide change across the base library of the whole ecosystem — it wants its own
pass with its own before/after comparison, which is exactly why PR #134 did not
carry it. **The masking argument does not generalise:** the bare cases were safe
only because every one of those loop bodies happens to contain a function call,
which already forces R0/R1/R14/R15 dead across the region. ftpd's loop had no
call. `@@cminit.c` is the one waiting to bite — it sits inside `#if 0`, so whoever
re-enables it gets no call before the `STIMER`.

File-scope `__asm__` blocks that define standalone routines (`EXITDRVR`,
`RETRY`/`RECOVERY`) are deliberately out of scope: they do their own
`SAVE (14,12)` and do not share the compiler's register allocation.

### 7 · #70 — `sleep()` and `__tzset()` have no prototype

Trivial. Afterwards httpd can delete its local declarations (httpd#140).

### 8 · #39 — 129 of 712 TUs with implicit declarations

The parent case. Steps 1 (declare the 14 routines with no prototype) and 2 (the
missing `#include`s) are mechanical and independent of each other. **The payoff is
step 3**: `-Wall` in `sdk/mklibc.py` — only that stops this class from coming back.

### 9 · #68 — `format(printf)` for `wtof()`/`wtodumpf()`/`wtorf()`

Cheap here, **expensive across the ecosystem**: consumers clone libc370 `main`
unpinned, so the attribute turns httpd, mvsMF and ftpd CI red — with the breakage
in *their* code. Keep the order the issue prescribes:

1. fix the two "too many" cases and the `clibsa.h` inline here,
2. sweep the consumers with locally attached attributes (needs no libc370 change),
3. only then land the attributes in `clibwto.h`.

---

## Tier 4 — structural traps

### 10 · #17 — consolidate the two `try()` wrappers

The trap is **active, not dormant**: since it first bit (#9 hardened the
unreachable copy), #89, #93 and #96 have each been applied *twice*. Every fix to
the central recovery path costs two edits and one chance to hit the wrong file.
Needs its own review and a validation plan — not a passenger in a relink round.

### 11 · #72 — the PPA environment flags do not say what they claim

No crash, but a documented API that answers wrongly: `TSOBG` is set in the TSO
foreground, and `TIN`/`TOUT`/`TERR` are set nowhere. Cheapest honest fix: correct
the semantics of `TSOFG`/`TSOBG` and either set the three dead defines (in
`@@fpstar.c`, which knows) or delete them. Declared-and-dead is the worst of the
three options.

### 12 · #105 — `GRTFLAG1_TSO` sticks beyond `__start()`

A design decision, not a patch: recompute per `__start()` (set *and* clear), or
move the TSO property into the CRT. The two readings differ for `fopen.c`,
`ropen.c` and `system.c`. **Settle reachability first** — does a second
`__start()` in the same address space actually happen at all?

---

## Tier 5 — consumers waiting (one coordinated relink, best done in a single round)

### 13 · #79 — JESJOB carries no submit time

Two lines plus a struct field. Zowe shows `exec-submitted` empty today, and
`mvslovers/mvsmf#209` is waiting on the same gap for `exec-system`. Append at
offset 0x50 as the issue describes, so 0x00-0x4F stays stable.

### 14 · #50 — catalog name in DSLIST

Same class, more work. Decide before implementing: scrape `LISTCAT` output, walk
the CVTCATP chain, or use the `LOCATE` return area.

### 15 · #51 — `inet_addr()` / `inet_ntoa()`

A good entry-level issue and a real memory win: it saves ftpd the entire `sscanf`
in its load module — on a 24-bit target exactly the kind of saving that counts.
Host test is trivial, because neither function touches MVS.

### 16 · #71 — `idcams()` discards SYSPRINT and the IDCnnnn number

One store in a `switch` branch that does nothing today, plus a companion accessor.
Afterwards ftpd says "IDC3203I" instead of "failed". `idcams()` keeps its
signature.

---

## Tier 6 — latent, research, comfort

### 17 · #114 — `osbclose()` does not free a buffer pool built by OPEN

Latent by our own analysis: `MACRF=R` and no BUFNO in the prototype DCB, so OPEN
does not normally build a pool. The in-tree callers are one member rename and an
unbuilt wip tree. httpd#195 — the hunt that flushed this out — is closed; this was
by-catch, not the planter. Take it along whenever the `osb*` path is being worked
on anyway.

### 18 · #113 — `CRTOPTS_AUTH` is dead, an authorized task skips `__austep()`

### 19 · #122 — `clib_apf_setup()`: the already-authorized path is dead code

**These two are one root cause and must be decided together.** `crt->crtopts` is
declared in `clibcrt.h:37` and tested in `@@apfset.c:15` — and **assigned
nowhere**. A grep across `src include asm` returns exactly those two lines, and a
sweep of every consumer repo turns up no writer either. The CRT is zeroed, so
`CRTOPTS_AUTH` is permanently clear.

Measured consequences, which is why both sit here rather than higher:

- `clib_apf_setup()` **always** takes `unauth_setup()`, so `auth_pgm()` and with it
  `clib_identify_cthread()` always run. That is why ftpd's threads work, and it is
  the answer to #122's "who is affected" section: **nobody, today.** As filed, it
  is not a live bug.
- SVC 244 is therefore issued unconditionally, including on a step that is already
  authorized. Harmless as measured, but not what the code claims.

The decision is the same one #113 already states: fill `crtopts` from the JSCB at
CRT init, or delete the field and the constant. Do it once, for both.

Note the correction recorded in #122: **ufsd does not belong in its
"who is affected" list.** It links `crt1` but never issues `ATTACH EP=CTHREAD`, so
the missing IDENTIFY cannot reach it; its APF troubles (ufsd#64) were the
module-storage ones. That leaves ftpd and httpd as the only consumers that both
link `crt1` and create threads.

### 20 · #27 — JES spool support is single-volume

Latent: the reference system has one spool volume and all 264 observed MTTRs carry
`M=00`. It goes live the day a second volume appears — and then presents as
"empty data set", not as an error.

### 21 · #52 — a z/OS-compatible `dynit.h`

Decide *whether* before building: two APIs for one service (`__dsalc()` with a
string, `dynalloc()` with a struct, both ending in `__svc99()`). Only worth it if
z/OS code is actually being ported in.

### 22 · #30 — SYSOUT through PSO/SSI instead of the checkpointed IOT

A research project with a cheap first step: add held-class selection in
`jesxwrtr()` and measure once what comes back in `SSSODSN`. One job decides whether
the rest runs straight. Note it is **no longer a gate on `mvslovers/mvsmf#186`** —
#21 closing gave that endpoint what it needed — so start this only when someone
needs it.

### 23 · #37 — SDK: compile the `.c` files in parallel

6.7 s → ~1 s across 712 TUs. Developer comfort. Check first whether parallel
`cc370` invocations are safe (cc1 temp files), and do not lose an error message.

### 24 · #75 — `clock()` as real task CPU time

The issue says it itself: dormant, nobody is waiting, lua370 is not a blocker.
Route (a) via TCT/`TCBTCT` would be the way, but it makes `clock()` SMF-dependent
— decide before writing a line whether a conditionally working `clock()` is worth
more than an honestly broken one.

---

## Three campaigns instead of twenty-four tickets

- **Unchecked allocation** — #80 defect 3 and #61. Settle one convention for the
  whole library rather than deciding twice, separately.
- **Compiler visibility** — #125, #70, #39, #68 (#104 landed). The goal is `-Wall`
  in the SDK build. #125 is the one with a measured failure and is independent of
  the rest; #68 goes last and in its own three-step order, or it reddens consumer CI.
- **Relink round** — #79, #50, and optionally #80 with a `max` parameter. Land
  struct and signature growth in one batch, with a CHANGELOG entry and a
  coordinated rebuild of httpd, mvsMF and ftpd.

---

## Recently landed

Pointers only. The reasoning lives in the closing comments and the PRs.

- **#104** (PR #136, 2026-08-23) — `strcpyp()` takes a `const void *source`. It
  was the last warning `make build` printed, so the noise floor over the ten
  `libc.a` directories is now 0 rather than 1. `test/host/tstjestx.c`'s shim had
  to move in the same commit or the header change is a hard `conflicting types`
  error, which no test target would have caught. `spl_strcpyp()` deliberately
  untouched (no callers); `memcpyp()` and the now-redundant `(void*)` casts at
  the call sites are still open, unfiled.
- **#125, the bare half** (PR #134) — ten inline SVC statements with no clobber
  list at all. The partial-list half is ranked at 6 above.
- **#126 / #127** (PR #129 / #130, 2026-08-22) — the INTXT walk in `jesjob(dd=1)`
  now goes through `__jesprb()` and no longer leaks on an abend (that was the
  fragmentation behind `mvslovers/mvsmf#282` and `#287`, measured at ~26 KB per
  abend), and `remove()` deletes members by STOW under SHR instead of IDCAMS
  (the ENQ escalation from `mvslovers/mvsmf#342`, measured closed by recipe).
  Together they also explain the open dd=0/dd=1 asymmetry in mvsmf#282.
- **#128 / #131** (PR #132 / #133, same day) — `vsnprintf()` honours its bound on
  every conversion path and always terminates (which made mvsMF's `make test-mvs`
  fully green for the first time, 508/0), and `rename()` renames members by STOW
  under SHR — including a GDG guard in both member branches, so `dsn(0)` and
  `dsn(+1)` still go to IDCAMS as whole-data-set operations.
- **#21** (PR #31) — `jesprint()` reports *why* it stopped instead of returning
  `rc=0` with no lines, verified on the target. This unblocked
  `mvslovers/mvsmf#186`.
- **#109 / #110 / #111** — every named suspect behind #108; see Tier 1 item 4.

Two corrections that shifted this ranking against the original issue text, both
pulled back into the issues on 2026-08-21:

- **#108 is no longer "mechanism unlocated"** — see Tier 1 item 4.
- **The caller table in #80 was out of date.** httpd's `httpdslp.c` lives under
  `httpd/tbd/` and is not built (`httpd/project.toml:108-113`), and mvsMF moved
  from `dsapi.c:369` to `:540`. Corrected in the issue.

The httpd-side hardening this file used to list as outstanding —
`mvslovers/httpd#238`, the SYSENV DD — **closed 2026-08-22**: SYSENV must name a
data set of its own and never `SYS2.PARMLIB`.
