# libc370 — Open Work, Ranked

**State lives on GitHub, not here.** `gh issue list --repo mvslovers/libc370` is
the source of truth for what is open, closed or newly filed. What this file adds
is the part the tracker cannot hold: the **order**, the reason for it, and which
items wait on a decision rather than on code.

Ordered by **measured impact on running systems** — not by age, and not by effort.
libc370 is the base library of the whole ecosystem, so a defect here is a defect
in httpd, mvsMF, ftpd, ufsd and every other consumer at once; that is what puts
some cheap items high and some expensive ones low.

*Last reconciled against the tracker: 2026-08-30, 29 issues open — of which the
ranked list below covers 25; #151 is fixed and merged.* **Four are
filed but not yet ranked**, all newer than the last full reconciliation: #142 (`jesopen()` should dynalloc the
checkpoint and spool — measured 2026-08-27, and the issue text understates it
both ways: `__cpopen()`/`__jsopen()` **already** dynalloc when the argument is
not `DD:`, so the minimum is two literals in `jesopen.c:37,47`; but the data set
name is not a constant — JES2 builds it from `$DSNPRFX` (init parameter,
default `SYS1`) plus the assembled literal `.HASPACE`/`.HASPCKPT`, and that
prefix lives in the HCT, which is unreachable from another address space. See
the measurements in the issue), #143 (no volume-addressed
SCRATCH/RENAME), #144 (an MVS test for `select()` silently dropping sockets)
and #149 (stdio fail-fast after `_FILE_FLAG_ERROR`, plus `clearerr()`). Place them
on the next pass rather than guessing a tier for them here. (#155 was a fifth
and is closed — see the update below; it asked for macros libc370 does
not assemble.)

**Tier 1 was emptied by seven closures and then refilled by an issue that had
been sitting in the tracker the whole time.** #107, #70,
#80 defect 2, **#11** (PRs #137, #138, #139, #141, merged 2026-08-23 on top of
the CHANGELOG backfill `3e9c15b`), then #145, #147 and now **#108** are all
closed. #11 was the only *observed and recurring* production failure on this
list since July, and #108 was the last open crash hunt — after which, for three
days, nothing ranked here was a failure anyone was seeing on a running system.
**#154 ends that**, and it was not new: filed 2026-08-27, missed by that day's
reconciliation, ranked 2026-08-30 as item 1. Below it the picture is unchanged —
two **campaigns**, a **relink round**, and a set of traps that have not bitten
yet. Rank accordingly: past item 1, every choice here is a judgement about
order, not an emergency.

**Update 2026-08-26: #145 briefly refilled Tier 1 — and is closed again.**
`vvprintf()`'s nested public `fputs()`/`putc()` released the FILE lock at the
first conversion, so practically the whole printf line ran unserialized — the
measured faces being ftpd#117's S001-1 (reproduced in seconds, JOB02235) and a
writer silently wedged in the corrupted QSAM state (JOB02237, 8 of 400 lines).
**PR #146 merged same day** (fix: internal writers + ownership-aware wrappers;
red/green tests host and MVS, green run JOB02239 CC 0000). Every multitasking
consumer wants a relink on the next release. Its known neighbours are **#147**:
items 2 (`puts()` split), 1 (`fclose()` teardown outside the lock) and 4 (DEQ
drops the scope bits — `sysunlock()` could never release, measured JOB02241/43)
landed via **PR #148**, and item 3 — **the SYNAD on the DCBs**, so a genuine
I/O error is `ferror()`+`EIO` instead of an address-space-killing S001 — via
**PR #150** (red JOB02246 / green JOB02250, plus the JOB02248 lesson that "no
abend" alone is not the contract), both merged 2026-08-26. **#147 is closed:**
its parked sweep came back empty — nothing in the 34 ecosystem repos calls
`syslock()`/`sysunlock()` at all, every other lock family is SCOPE=STEP and so
was unreachable by that bug, and no consumer owes a change for it. Follow-up
ideas (fail-fast after `ferror()`, `clearerr()`) live in **#149**, which is a
deliberate API decision, not a defect. With #145/#147 done, every multitasking
consumer wants a relink on the next release — now for four reasons, not one.

**And #108 is closed the same day**, on the 2026-08-23 re-verification run with
its caveat intact: the address space was never degraded, so that run is *no
regression under sustained load*, not proof against the original failure. It is
enough because #111 — 127 bytes into a 12-byte buffer — has exactly the
signature the evidence demanded (constant trigger, layout-dependent blast
radius), and because #126 closed the ~26 KB-per-abend leak that produced the
degraded state in the first place. The deliberate `REGION`-starved re-run stays
undone on purpose; it threads a narrow window, breaks the stand while it runs,
and would at best buy a negative on a spent hypothesis. **Closing it emptied
Tier 1 of live defects** — and its unfiled review footnotes were harvested first,
as **#151** and **#152**.

**Update 2026-08-27: #151 is fixed and merged (PR #153).** Four external names were each exported by
two archived objects; three were byte-identical twins from a mistyped filename,
but `@@ERRNO` was a function prologue in one object and a `DC F'0'` in the other,
with every `errno` in the ecosystem compiling to a call to that name. Latent —
link order happened to pick the right one — and now pinned by deletion rather
than by luck. The guard is `sdk/dupscan.py`, a build step scoped to exactly the
set being archived (733 modules, matching the archive exactly), fail-closed before it
is written. That empties Tier 1 again.

**Update 2026-08-30: #155 is decided and closed — the mirror's scope is now a
written rule.** The ask was four SYS1.MACLIB members a COBOL-74 code generator's
output expands (`SPIE`, `TIME`, `WTOR`, `PUTX`). All four declined, and the
reason generalises: `sysmac/` exists to carry what **libc370 itself assembles**,
and a generator decides its own macro set, so adding on request quietly turns
this repo into the ecosystem's system macro library. The rule is in
`doc/consumer-notes.md`, with the measurement behind it — 120 of the 123 members
are reachable from libc370's own build (27 `asm/*.asm` + 716 generated `.s` +
`maclib/`, inner macros closed transitively). The three that are not: `GENCB`
and `TESTCB` have no user anywhere in the ecosystem, and `XCTL` has one that
matters — rexx370's `asm/irxtmpw.asm:110`, a declared build source in a project
with no macro directory of its own. All three stay: `<sysroot>/macros` is a
published surface, so removing a member is a breaking change and needs its own
decision, not a tidy-up.

**Update 2026-08-30: the Tier 2 convention is decided, and the campaign is four
functions rather than two.** The sweep that settled it found the same shape in
`__listds()` and `__listal()`, both unfiled until now — **#157** and **#158**.
The convention lives in #61, retitled to hold it. Details in Tier 2 below; the
short form is that it needs no signature change and no relink, and that it moves
the wrong answer rather than removing it, so three consumer follow-ups are part
of the campaign — filed 2026-08-30 as `mvslovers/ftpd#118`,
`mvslovers/mvsmf#360` and `mvslovers/lua370#15`, each marked blocked by the
libc370 issue it waits on.

---

## Tier 1 — one live defect

### 1 · #154 — `recv()` caps its X'75' chunk at 4096, and only 256 or less is safe

Filed 2026-08-27, missed by that day's reconciliation and by every list in this
file until 2026-08-30. It is ranked here rather than parked with the unranked
items because the argument is read off the emulator source rather than inferred,
and because what it describes is silent data corruption in every X'75' consumer.

`@@75recv.c:29-36` caps each RECV at 4096 and explains it with a dyn75/Hercules
buffer-size limit. **The observation is right and the explanation is wrong**, so
the cap does not prevent the thing it was written for.

X'75' copies in **256-byte segments** and the instruction is restartable.
Upstream `x75.c` recomputes the host-side pointer on every entry:

```c
if (regs->GR_L(1) != 0) s = (unsigned char *)(map32[regs->GR_L(2)]);
```

The guest side has architected state to resume from — the base register
`GR_L(b2)` and the remaining count `GR_L(1)` both survive a nullifying
exception. The host side has none: R2 is a slot index that never advances. So
after a page fault on the guest buffer the copy resumes from the **start** of
the host buffer and writes it to the already-advanced guest address. That is
exactly the symptom the comment records.

`vstorec()` resolves both page addresses through `MADDRL` before either
`memcpy`, so one segment is atomic against a translation exception. **256 or
less is immune by construction**: a single segment either faults having copied
nothing — where resuming from the start is correct — or completes. Above 256 a
segment can complete before a later one faults, and the likelihood scales with
how many guest pages the copy touches. That is the whole of the observed size
correlation.

Which is why every cap so far worked and then failed. 4096 here since `cd43a70`
(December 2024) is still 16 segments; mvsMF then met the same corruption at
>2048 **with that cap in place**, capped at 2048 (`d2783f5`), and that failed
five days later (`4bc1014`). `receive_raw_data()` has read **one byte per
`recv()`** ever since — immune for the same reason 256 is. Sighted independently
outside the ecosystem too: `twinslow/mvs_nfsd`, `socktest/`, same X'75' layer.

**The change is one line and a truthful comment**, and it does not wait on the
emulator. The host-side fix is committed on the fork as
`mvslovers/hyperion` `fix/x75-restart-resume` (`fcf7d15d`, adding
`+ lar_offset(&regs->gr[0])` so the host pointer resumes where the guest one
did), with a red/green pair on `diag/x75-restart-trace` that instruments every
restart. Neither change requires the other.

**The mechanism is now measured, not read off source.** `test/mvs/tst75rst.c`
forces the fault (page boundary at a chosen multiple of 256, `PGRLSE` on the
page beyond it) and was run on MVSCE on 3 Sep 2026 against both halves of the
red/green pair:

| Case | red `first_bad` | green `first_bad` |
|---|---|---|
| boundary 0 (control) | none | none |
| boundary 256 | **256** | none |
| boundary 512 | **512** | none |
| boundary 768 | **768** | none |

Red `JOB03045` RC=8 under `gf1f1f9d1`, green `JOB03046` RC=0 under
`g392c22c6`. In every red case the tail is a clean replay of the host buffer
from its start, and the emulator's own trace reports `done` equal to the
guest's `first_bad`. The control faults having copied nothing and is clean on
both — which is exactly why **256 or less is immune**, and that is no longer an
argument but an observation. The three restarts after a completed segment still
occur under green with the same `done` values, so the fix changed the resume
path and not the fault rate.

The cap is also **permanent**: a guest
cannot detect a patched emulator — no return value, status bit or function code
distinguishes one, and adding such a thing would change the interface for every
existing guest — so this can never be raised again on the strength of a fixed
host.

Cost is 16x more X'75' pairs than at 4096, and still a large net win against
what consumers actually do: roughly 5700 pairs for a 1.4 MB body, against
roughly 1.47 million single-byte `recv()` calls in mvsMF's present workaround.
Once it lands, mvsMF can return `receive_raw_data()` to bulk reads.

`test/mvs/tst75rst.c` (`jcl/tst75rst.jcl`) is the probe. It does not wait for a
page fault, it causes one: a page boundary is placed at a chosen multiple of 256
inside a 1024-byte receive buffer and the page beyond it is released with PGRLSE
immediately before the receive, over a loopback pair the program owns both ends
of. The receive goes through `__75()` rather than `recv()` so that one
measurement is one pair of instructions. Read its RC together with the emulator
trace and never alone — a clean run has two causes, resumed correctly and never
faulted, and the guest cannot tell them apart.

**`@@75send.c` is deliberately out of scope** — same exposure and no cap at all
(`:46` passes `len` straight through), the mirror image with the host buffer
losing its leading segments. Capping it changes what every caller sees per call,
because `send()` returns a byte count and callers loop on partial writes. Its
own decision and its own issue, not filed yet.

The asymmetry in what has been observed fits the mechanism: a send buffer was
just written by the application and is hot, while a receive buffer can have lain
idle across an I/O wait — exactly when its pages get stolen.

---

## Tier 2 — campaign: unchecked allocation

### 2 · #61, #80 defect 3, #157, #158 — four list builders hand back a silently short list

**The convention is decided (2026-08-30) and recorded in #61**, which was
retitled to hold it: on any allocation failure a list builder frees the partial
list and everything in it, returns `NULL`, guarantees `errno == ENOMEM` at the
return, and sets `errno = 0` on entry so a legitimately empty `NULL` is not read
as a failure. No signature change, no relink. What is left is the
implementation — four functions, one pass, one convention.

The sweep that settled it found the family is four, not two:

| Function | On allocation failure | Issue |
|---|---|---|
| `__listpd()` | `goto quit` → partial list | #80 defect 3 |
| `__listvl()` | `break` → partial list | #61 |
| `__listds()` | callback returns 0, **the scan continues** → holes in the middle | #157 |
| `__listal()` | `goto quit` → partial list, **and the record leaks** | #158 |

**`__listds()` fails worst and has the most live callers** (ftpd ×2, mvsMF ×2).
It does not truncate: `parse()` returns 0 after the failure and `__listc()` keeps
feeding lines, so entries go missing out of the *middle* and the list ends
exactly where a good run would end. mvsMF's data set list endpoint
(`dsapi.c:1313`) renders that as a complete listing with a data set absent from
it — a false negative with no short tail to notice. Its own complication is that
the callback cannot stop the scan at all: `__listc()` discards the `prt()`
return (`@@listc.c:67,93`).

**The cost is not uniform.** For `__listpd()` the `errno` half is nearly free,
and measured rather than assumed: `fclose()` never assigns `errno` itself, and
the one write in its chain (`@@fflush.c:64`, `EIO`) is unreachable for a
`"r,record"` FILE. For `__listvl()` there is a prerequisite — the `break` falls
straight into the VATLST block, whose `fopen`/`fgets`/`fclose` can overwrite
`errno`, and the `quit:` label at `@@listvl.c:136` has **no `goto` pointing at
it**: that exit was written and never wired. `__listds()` has to carry its
`errno` in `UDATA` across the rest of the scan. #158 also owes two plain
`free()`s on its `array_add` paths, independent of the convention.

**The fix moves the wrong answer, it does not remove it.** `NULL` is already
overloaded as "empty / not found" by every consumer: mvsMF answers 404
(`dsapi.c:713-716`), ftpd answers 550 (`ftpd#mvs.c:943`), lua370 does not check
at all (`loslib.c:1111`). So a storage shortage will be reported as an empty
result instead of a short one until those three read `errno` — **three
consumer-side follow-ups**, filed 2026-08-30 and each blocked by the libc370
issue it waits on: `mvslovers/ftpd#118` and `mvslovers/mvsmf#360` (both on #80
defect 3 and #157), `mvslovers/lua370#15` (on #61). Without them, "fixed in
libc370" reads as fixed when it is not.

lua370 is worth noting separately: it is the **only** live consumer of
`__listvl()` anywhere, so #61's consumer side is that one ticket and nothing
else.

**Ordering against #80 defect 1** (Tier 5 item 10): if the shortfall is ever to
be signalled explicitly rather than through `errno`, it belongs *in* that
signature change, not in a round of its own — otherwise `__listpd()` takes two
API breaks in two releases. `errno` costs nothing and lands now; an explicit
out-parameter can ride the relink round later, with `errno` as the fallback for
callers that never adopt it.

PR #139 is what made this the campaign's remaining content: it bounded
`__listpd()`'s walk and deliberately kept what the block had already yielded
**without signalling the shortfall**.

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

### 23 · #152 — `arraydel()` reads one slot past the allocation

Bottom of the list on purpose: it is real, and it is currently harmless. The
shift loop runs to `count` instead of `count - 1`, so on a full array it reads
`(*carray)[size]` — but the value is written into the slot that the next two
statements overwrite with NULL, so nothing observes it. ASAN-class, not a
measured fault.

Filed anyway because the #108 review recorded it as sitting behind `#if 0`, and
that holds for the jesjob path only: `arraydel()` has ~18 live callers — worker
table, work queue, socket table, FILE table, mutex table, `atexit`/`on_exit`,
CRT push/pop. One-line fix, and worth a host test at full occupancy, of which
there is none today.

---

## Three campaigns instead of twenty-five tickets

- **Unchecked allocation** — #61, #80 defect 3, #157 and #158. The convention is
  settled (NULL + guaranteed `errno`, 2026-08-30, recorded in #61) and covers all
  four list builders; what is open is one implementation pass over them, plus
  three consumer follow-ups (`ftpd#118`, `mvsmf#360`, `lua370#15`) that are filed
  and blocked on it. PR #139 made this the
  campaign's remaining libc370-side content: it bounded the walk but deliberately
  did not signal the shortfall.
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

- **#151** (PR #153, 2026-08-27) — four external names were each exported by two
  archived objects. Three were byte-identical twins from a mistyped filename
  (`diff` on the generated assembler is empty for all three pairs); `@@ERRNO`
  was a function prologue in one object and a `DC F'0'` in the other, while
  every `errno` in the ecosystem compiles to `L 15,=V(@@ERRNO)` + `BALR`
  (measured, `src/dyn75/@@75sock.s:66`) — so link order decided whether `errno`
  reached the per-task accessor or branched onto four zero bytes. Latent, never
  observed. Archive 737 → 733 members, nothing else changed; the surviving
  object of each pair is the one that was already being linked. Guarded by
  `sdk/dupscan.py`, a fail-closed build step scoped to exactly the archived set,
  proven red when the offender is restored. Note for cc370: the as370 corpus
  manifest lists these four, and that gate was already red — 110 `CHANGED` and
  10 coverage drifts from a manifest generated 2026-07-18, 58 `src/` commits ago.

- **#108 closed** (2026-08-26) — the `jesjob(dd=1)` S0C4 hunt, closed on the
  2026-08-23 re-verification run (~2100 `dd=1` walks on a build confirmed to be
  `main`, zero 5xx, a clean MTT) with its caveat intact: the address space was
  never degraded. Enough because every named suspect is fixed (#109, #110, #111)
  and #126 removed the ~26 KB-per-abend leak that produced the degraded state;
  the symptom, mvsmf#282, closed `COMPLETED` 2026-08-17. Its three unfiled
  review footnotes were re-measured against `main` before closing:
  `spool_read()` in `process_intxt()` is now checked (`jesjob.c:613`), the
  duplicate externals became **#151**, the `@@ardel.c` one-past read became
  **#152**.

- **#147 item 3** (PR #150, 2026-08-26) — SYNAD on the BSAM DCBs: an
  uncorrectable I/O error is `ferror()`+`errno EIO` instead of ABEND S001. The
  stub is per-FILE and R15-relative (the R1-based first cut measurably
  delivered a truncated block as data — JOB02248); red JOB02246, green
  JOB02250, guards `test/mvs/tstsynad.c`. Fail-fast/`clearerr()` ideas → #149.
  The `syslock()` sweep that item 4 parked ran on close: **zero callers across
  all 34 ecosystem repos**, and every other lock family is SCOPE=STEP, so the
  DEQ bug was unreachable for them by construction. A real defect that never
  bit anyone; no consumer owes a port.
- **#147 items 2/1/4** (PR #148, 2026-08-26) — `puts()` is one critical section,
  `fclose()` tears down under the FILE lock, and `__enqdeq()`'s DEQ keeps its
  scope bits (`sysunlock()` could never release what `syslock()` took — measured
  JOB02241 red / JOB02243 green). Guards: tstiolk case 9, tstenqdq (SVC
  parameter-list capture), tstfcls, tstslk. Item 3 (SYNAD) still open there.
- **#145** (PR #146, 2026-08-26) — `vvprintf()`'s nested public `fputs()`/`putc()`
  released the FILE lock at the first conversion; concurrent printf corrupted
  the stream (ftpd#117's S001-1, plus a silent writer wedge, both measured).
  Internal writers go through `__fputs()`/`__fputc()`, the public one-FILE
  wrappers release only a hold they acquired. `test/host/tstiolk.c` +
  `test/mvs/tstiolk.c` are the regression guards; the ENQ rc=8 nesting
  contract is measured on the target (TSTIOLK round 1).
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
  #80 stays open for defects 1 and 3, now ranked at items 10 and 2.
- **#107** (PR #137, 2026-08-23) — `cthread_worker_add()` releases the manager
  lock when `cthread_create_ex()` fails. Verified in the generated assembly, there
  being no host harness for the thread manager: `cc370 -O1 -S` before and after
  differ in exactly one instruction, `B @@L3` becoming `B @@L5`.
- **#70** (PR #138, 2026-08-23) — `sleep()` and `__tzset()` declared in `time.h`,
  and both `.c` files include it so the definitions are checked. No consumer
  conflicted: httpd's local declarations took their signatures from the same
  definitions. Generated code byte-identical, as a prototype should be.
- **#108 re-verified, not closed** (2026-08-23) — superseded by the closure
  above. Clean run, but the
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
  list at all. The partial-list half is ranked at 3 above.
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
- **#109 / #110 / #111** — every named suspect behind #108; see its entry above.

Two corrections that shifted this ranking against the original issue text, both
pulled back into the issues on 2026-08-21:

- **#108 is no longer "mechanism unlocated"** — see its entry above.
- **The caller table in #80 was out of date.** httpd's `httpdslp.c` lives under
  `httpd/tbd/` and is not built (`httpd/project.toml:108-113`), and mvsMF moved
  from `dsapi.c:369` to `:540`. Corrected in the issue.

The httpd-side hardening this file used to list as outstanding —
`mvslovers/httpd#238`, the SYSENV DD — **closed 2026-08-22**: SYSENV must name a
data set of its own and never `SYS2.PARMLIB`.
