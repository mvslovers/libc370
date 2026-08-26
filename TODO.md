# libc370 — Open Work, Ranked

**State lives on GitHub, not here.** `gh issue list --repo mvslovers/libc370` is
the source of truth for what is open, closed or newly filed. What this file adds
is the part the tracker cannot hold: the **order**, the reason for it, and which
items wait on a decision rather than on code.

Ordered by **measured impact on running systems** — not by age, and not by effort.
libc370 is the base library of the whole ecosystem, so a defect here is a defect
in httpd, mvsMF, ftpd, ufsd and every other consumer at once; that is what puts
some cheap items high and some expensive ones low.

*Last reconciled against the tracker: 2026-08-23, 22 issues open.*

**Tier 1 is nearly empty, and that is the news.** #107, #70, #80 defect 2 and
now **#11** are all closed (PRs #137, #138, #139, #141, merged 2026-08-23 on top
of the CHANGELOG backfill `3e9c15b`). #11 was the only *observed and recurring*
production failure on this list since July; with it gone, what remains at the
top is one **decision** (#108) and two **campaigns**, not a burning defect. Rank
accordingly — the next thing to do is a judgement call, not an emergency.

**Update 2026-08-26: #145 briefly refilled Tier 1.** `vvprintf()`'s nested
public `fputs()`/`putc()` released the FILE lock at the first conversion, so
practically the whole printf line ran unserialized — the measured faces being
ftpd#117's S001-1 (reproduced in seconds, JOB02235) and a writer silently
wedged in the corrupted QSAM state (JOB02237, 8 of 400 lines). **PR #146**
(fix: internal writers + ownership-aware wrappers; red/green tests host and
MVS, green run JOB02239 CC 0000) is open; when it merges this drops back out
of Tier 1 and every multitasking consumer wants a relink on the next release.

---

## Tier 1 — now

### 1 · #108 — reclassify, do not hunt

No longer an open crash hunt. #109 and #110 are merged and the prime suspect named
in the closing comment, #111, is in `main` as `cd66e86`; every candidate in the
call tree is eliminated. The issue was renamed to *"every named suspect fixed,
needs re-verification"*.

**Run on 2026-08-23 against `mvsdev`, and it came back clean** — but read the
caveat, because it is not quite the test this item asked for.

The build was confirmed from the console rather than assumed
(`HTTPD005I LIBC370 1.0.3-DEV (3E9C15B)` = `main`), and the path was confirmed
in mvsMF rather than taken from the issue text: `jobFilesHandler` **and**
`jobRecordsHandler` both reach `jesjob(…, 1)` through
`find_job_by_name_and_id()` (`jobsapi.c:209/273 → :1238`), so a record read
rebuilds the inventory too — close to **2100 `dd=1` walks**, not the ~340 the
endpoint count suggests. 2172 requests went through the `dd=1` path: `/files` over every one of the 131 jobs on the system, then
1200 and 856 requests in 8 concurrent streams over the largest job available
(106 spool files), including a full record sweep. Zero 5xx, and the Master
Trace Table over the whole window holds no `S80A`, `S0C4`, `IEA703I`,
`MVSMF901E` or `HTTPD908E`. The server never restarted.

**What is missing is the degraded address space.** Storage stayed healthy
throughout — the worker pool grew from 3 to 8 of 9 and the last 65536-byte
stack GETMAIN succeeded at peak load. So this is *no regression under sustained
load*, not proof against the original failure.

The argument for closing anyway is that the degradation had a known engine and
it is fixed: #126 measured ~26 KB leaked **per abend** in the INTXT walk, which
is what walked the address space down until the next allocation failed. Route
closed, suspects all fixed, 2172 requests produce nothing.

Getting the real thing needs one deliberate step — bring HTTPD up under a
`REGION` too small for 8 worker stacks plus a 106-file walk, then repeat. That
breaks the stand for the duration, so it is a decision rather than a task.
Full evidence in the issue comment.

---

## Tier 2 — campaign: unchecked allocation

### 2 · #61 — `__listvl()` silently returns a truncated volume list, and #80 defect 3

Now the whole of this campaign, and #80's remaining weight sits here: defect 2
landed in PR #139, which deliberately stops the walk and keeps what the block
already yielded **without signalling the shortfall** — precisely the gap defect 3
has to close, in `__listpd()` and `__listvl()` alike.

Same shape as #80 defect 3. The two need **one** convention rather than two
separate decisions — apply the recommendation from #61 (variant 2: fail
completely, return NULL) to both. The caller has to check for NULL anyway, and a
short list that looks complete is worse than no list at all.

---

## Tier 3 — campaign: make the compiler see it (order matters)

### 3 · #125 — inline-asm SVC macros with partial clobber lists

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

### 4 · #39 — 129 of 712 TUs with implicit declarations

The parent case. Steps 1 (declare the 14 routines with no prototype) and 2 (the
missing `#include`s) are mechanical and independent of each other. **The payoff is
step 3**: `-Wall` in `sdk/mklibc.py` — only that stops this class from coming back.

Two more found while building the #80 host test, not in the issue's list:
`@@freepd.c` calls `__arcou()` and `__arfre()` with no prototype in scope. They
link on the target only because the `__` → `@@` symbol mapping happens to produce
the right CSECT — the same invisible-call shape the issue records for httpd's
`__arcou()`. Worth folding into step 1 when it runs.

### 5 · #68 — `format(printf)` for `wtof()`/`wtodumpf()`/`wtorf()`

Cheap here, **expensive across the ecosystem**: consumers clone libc370 `main`
unpinned, so the attribute turns httpd, mvsMF and ftpd CI red — with the breakage
in *their* code. Keep the order the issue prescribes:

1. fix the two "too many" cases and the `clibsa.h` inline here,
2. sweep the consumers with locally attached attributes (needs no libc370 change),
3. only then land the attributes in `clibwto.h`.

---

## Tier 4 — structural traps

### 6 · #140 — `src/thdmgr/clibthdi.h` duplicates `include/clibthdi.h`

Filed while fixing #11, and it bit during that work. The quoted include in
`src/thdmgr/*.c` finds the local copy, so **the library compiles against a
different file from the one consumers get** — for a header that declares
`CTHDMGR`/`CTHDWORK`/`CTHDQUE`, control blocks httpd decodes by offset
(`httpcons.c`). The two agree only by nobody having edited one of them.

It surfaced as a hard build error only because the edit was a `#define`; a
struct field would have linked and run, with the library and every consumer
disagreeing about a layout. Exactly #17's shape, one tier's worth cheaper to
fix: delete the copy and let `-I include` resolve it.

### 7 · #17 — consolidate the two `try()` wrappers

The trap is **active, not dormant**: since it first bit (#9 hardened the
unreachable copy), #89, #93 and #96 have each been applied *twice*. Every fix to
the central recovery path costs two edits and one chance to hit the wrong file.
Needs its own review and a validation plan — not a passenger in a relink round.

### 8 · #72 — the PPA environment flags do not say what they claim

No crash, but a documented API that answers wrongly: `TSOBG` is set in the TSO
foreground, and `TIN`/`TOUT`/`TERR` are set nowhere. Cheapest honest fix: correct
the semantics of `TSOFG`/`TSOBG` and either set the three dead defines (in
`@@fpstar.c`, which knows) or delete them. Declared-and-dead is the worst of the
three options.

### 9 · #105 — `GRTFLAG1_TSO` sticks beyond `__start()`

A design decision, not a patch: recompute per `__start()` (set *and* clear), or
move the TSO property into the CRT. The two readings differ for `fopen.c`,
`ropen.c` and `system.c`. **Settle reachability first** — does a second
`__start()` in the same address space actually happen at all?

---

## Tier 5 — consumers waiting (one coordinated relink, best done in a single round)

### 10 · #80 defect 1 — `__listpd()` has no way to ask for less

What is left of #80 after PR #139, and it is narrow: one exposed caller, ftpd's
`LIST`/`NLST` (`ftpd#mvs.c:941`) with a user-supplied filter. On `SYS1.SMPCDS`
(22982 members) it still walks the whole directory and `calloc`s a record per
member before returning anything. A `max` parameter or an iterator form fixes it,
but either is a signature change — hence this tier. It would also let mvsMF drop
the duplicated directory parser it carries at `dsapi.c:2076`.

### 11 · #79 — JESJOB carries no submit time

Two lines plus a struct field. Zowe shows `exec-submitted` empty today, and
`mvslovers/mvsmf#209` is waiting on the same gap for `exec-system`. Append at
offset 0x50 as the issue describes, so 0x00-0x4F stays stable.

### 12 · #50 — catalog name in DSLIST

Same class, more work. Decide before implementing: scrape `LISTCAT` output, walk
the CVTCATP chain, or use the `LOCATE` return area.

### 13 · #51 — `inet_addr()` / `inet_ntoa()`

A good entry-level issue and a real memory win: it saves ftpd the entire `sscanf`
in its load module — on a 24-bit target exactly the kind of saving that counts.
Host test is trivial, because neither function touches MVS.

### 14 · #71 — `idcams()` discards SYSPRINT and the IDCnnnn number

One store in a `switch` branch that does nothing today, plus a companion accessor.
Afterwards ftpd says "IDC3203I" instead of "failed". `idcams()` keeps its
signature.

---

## Tier 6 — latent, research, comfort

### 15 · #114 — `osbclose()` does not free a buffer pool built by OPEN

Latent by our own analysis: `MACRF=R` and no BUFNO in the prototype DCB, so OPEN
does not normally build a pool. The in-tree callers are one member rename and an
unbuilt wip tree. httpd#195 — the hunt that flushed this out — is closed; this was
by-catch, not the planter. Take it along whenever the `osb*` path is being worked
on anyway.

### 16 · #113 — `CRTOPTS_AUTH` is dead, an authorized task skips `__austep()`

### 17 · #122 — `clib_apf_setup()`: the already-authorized path is dead code

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

### 18 · #27 — JES spool support is single-volume

Latent: the reference system has one spool volume and all 264 observed MTTRs carry
`M=00`. It goes live the day a second volume appears — and then presents as
"empty data set", not as an error.

### 19 · #52 — a z/OS-compatible `dynit.h`

Decide *whether* before building: two APIs for one service (`__dsalc()` with a
string, `dynalloc()` with a struct, both ending in `__svc99()`). Only worth it if
z/OS code is actually being ported in.

### 20 · #30 — SYSOUT through PSO/SSI instead of the checkpointed IOT

A research project with a cheap first step: add held-class selection in
`jesxwrtr()` and measure once what comes back in `SSSODSN`. One job decides whether
the rest runs straight. Note it is **no longer a gate on `mvslovers/mvsmf#186`** —
#21 closing gave that endpoint what it needed — so start this only when someone
needs it.

### 21 · #37 — SDK: compile the `.c` files in parallel

6.7 s → ~1 s across 712 TUs. Developer comfort. Check first whether parallel
`cc370` invocations are safe (cc1 temp files), and do not lose an error message.

### 22 · #75 — `clock()` as real task CPU time

The issue says it itself: dormant, nobody is waiting, lua370 is not a blocker.
Route (a) via TCT/`TCBTCT` would be the way, but it makes `clock()` SMF-dependent
— decide before writing a line whether a conditionally working `clock()` is worth
more than an honestly broken one.

---

## Three campaigns instead of twenty-two tickets

- **Unchecked allocation** — #61 and #80 defect 3. Settle one convention for the
  whole library rather than deciding twice, separately. PR #139 made this the
  campaign's whole remaining content: it bounded the walk but deliberately did
  not signal the shortfall.
- **Compiler visibility** — #125, #39, #68 (#104 and #70 landed). The goal is
  `-Wall` in the SDK build. #125 is the one with a measured failure and is
  independent of the rest; #68 goes last and in its own three-step order, or it
  reddens consumer CI.
- **Relink round** — #79, #50, #51, #71 and #80 defect 1's `max` parameter. Land
  struct and signature growth in one batch, with a CHANGELOG entry and a
  coordinated rebuild of httpd, mvsMF and ftpd.

---

## Recently landed

Pointers only. The reasoning lives in the closing comments and the PRs.

- **#11** (PR #141, 2026-08-23) — the S33E on shutdown, and neither half was what
  this file or the issue said. **The drain failure was a deadlock on the manager's
  own ENQ**, not a wedged handler: `dispatch_thread_term()` held `lock(mgr,0)`
  across its wait, while `cthread_worker_wait()` — the only place a worker sees
  the shutdown post — *opens* with `cthread_queue_del()`, which needs that same
  lock whenever the worker still holds a dispatched item. Every busy worker
  blocked waiting on the thread that was waiting for it. **And the S33E and the
  nested ESTAE fault were one bug**: the stack lives inside the CTHDTASK
  (`calloc(1, sizeof(CTHDTASK) + newstack)`), so the force-DETACH was followed by
  `free()` of the storage the dying subtask's recovery exit stood on.
  Fixed by releasing the lock around the wait, gating every DETACH on `termecb`
  (including a third instance at `@@tmstop.c:46`), and propagating retention up
  to `cthread_manager_term()` so a retained worker keeps `mgr` alive.
  Measured red/green on mvsdev: pre-fix COND CODE 0008 at 13.31 s with the worker
  never returning, post-fix 0000 at 10.21 s — the 3.1 s difference *is* the
  deadlock. Probe: `test/mvs/tstwterm.c` + `jcl/tstwterm.jcl`.
  Two things the run corrected: no `S33E` message and no dump appear in a bare
  worker (the recovery exit is installed only via `try()`/`estae()`, which is why
  httpd#122 was loud), and pre-fix passes **six of seven checks** including
  `cthread_manager_term reports success` — which is why the probe cannot rely on
  return codes.

- **#80 defect 2** (PR #139, 2026-08-23) — `__listpd()` bounds its directory walk
  by the bytes `fread()` delivered. The fixed-part bound went into the loop
  condition (`pos + 12 <= len`, covering the end-of-directory `memcmp` and the
  user data length byte together), with `pos + size > len` behind it for the copy.
  **A full 256-byte block was the ordinary case that tripped it** — 21 entries end
  at 254, `pos < 256` still held, and the sentinel `memcmp` read six bytes past a
  256-byte stack array. New host test `test/host/tstlspd.c`, 15/15 under ASan, red
  against the pre-fix source at `offset 320` of a `[64, 320)` frame array.
  #80 stays open for defects 1 and 3, now ranked at items 10 and 3.
- **#107** (PR #137, 2026-08-23) — `cthread_worker_add()` releases the manager
  lock when `cthread_create_ex()` fails. Verified in the generated assembly, there
  being no host harness for the thread manager: `cc370 -O1 -S` before and after
  differ in exactly one instruction, `B @@L3` becoming `B @@L5`.
- **#70** (PR #138, 2026-08-23) — `sleep()` and `__tzset()` declared in `time.h`,
  and both `.c` files include it so the definitions are checked. No consumer
  conflicted: httpd's local declarations took their signatures from the same
  definitions. Generated code byte-identical, as a prototype should be.
- **#108 re-verified, not closed** (2026-08-23) — see item 2. Clean run, but the
  address space never became degraded, so it is not yet the test the issue asks
  for. The decision to close is recorded there as open.

- **CHANGELOG backfill** (`3e9c15b`, 2026-08-23, direct to `main`) — six landed
  changes had no entry at all: #104, #125 (bare half), #123/#124, #118/#119,
  #115/#116 and #115/#117. `Recently landed` here recorded them; the CHANGELOG is
  what ships, and #104 is a public header signature change consumers meet on their
  next unpinned clone. The `Unreleased` section also gained the `### Added`
  heading it was missing, and two duplicated bullet headlines (the `remove()` and
  `send()` entries) left by the changelog-keeping merges `82b32a1`/`ed578f7` are
  gone.

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
