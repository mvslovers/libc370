# libc370 — Open Work, Ranked

**State lives on GitHub, not here.** `gh issue list --repo mvslovers/libc370` is
the source of truth for what is open, closed or newly filed. What this file adds
is the part the tracker cannot hold: the **order**, the reason for it, and which
items wait on a decision rather than on code.

Ordered by **measured impact on running systems** — not by age, and not by effort.
libc370 is the base library of the whole ecosystem, so a defect here is a defect
in httpd, mvsMF, ftpd, ufsd and every other consumer at once; that is what puts
some cheap items high and some expensive ones low.

**Since 2026-09-30 that rule puts the 2.0 critical path first (Tier 0).**
1.0.8 is the last 1.x release and the consumers are pinned to it, so a fix that
lands on `main` now reaches no running system until 2.0.0 ships. Everything
from Tier 1 on keeps its order and resumes on 2.x. A serious defect found
before 2.0.0 gets an emergency 1.0.9 from the tag `v1.0.8` (D8 in
`doc/design-2.0.md`).

*Last reconciled against the tracker: **2026-09-30**, 53 issues open (#140,
#243 and #248 closed by PR #255 and #241/#251 by PR #253, all the same day;
#254 filed out of rexx370's measurements; before that 57 — #249 closed; #240
and #241 filed out of #39, #243–#246 and #248–#250 out of the 2.0 plan, #251
out of #249), all 53 accounted for below.* The pass before was **2026-09-29** at 48 open (#182, #228, #229, #231, #232, #235 and #236
closed, #228, #229, #231, #232, #235 and #236 filed the same day). That pass found **#181** and **#182** (filed 2026-09-13/14) in
no rank at all, fifteen days after they were filed — #182 is now rank 1 and
#181 rank 39. The pass before was 2026-09-13 at 41 open, all 41 ranked. #149 was fixed and released the same day (PR #180, **v1.0.6**)
and #178/#179 were filed out of that work; both are ranked at the end. The previous pass was 2026-08-30 at 29 open, and the gap it left
is the reason this note now says "all": six issues filed on 2026-09-06
(#160–#165) never reached this file at all, and the four it had parked as *not
yet ranked* — #142, #143, #144, #149 — were still parked thirteen days later,
alongside #159 (filed on the day of that pass and missed by it), #169, #171,
#172, #173, #174 and #176. A parked item is invisible, and that has a cost:
**#160 is the send-side mirror of #154**, which was rank 1 until it was fixed,
and nothing in this file said so for a week. This pass places every one of them.
(#151 and #155 are closed — see the updates below.)

**#167 was filed, fixed and merged on the same pass (PR #170, 2026-09-13) and
never needed a rank.** `__txrlse()` had been dead code since it was written: a `DALRLSE` text
unit builder with a prototype and no caller, so no consumer could get unused
space released for anything written through `fopen()`. The fix is a mode-string
keyword — `fopen(dsn, "wb,rlse")` — wired into `__fpold()` and `__fpnew()`, opt
in, skipped for a PDS member. It matters because **mvslovers/ftpd#100 /
ftpd#127 are waiting on it**: an FTP STOR has no size at allocation time, so
allocating large enough for a big upload strands that space on every small one.
The scope call that made it not-a-one-liner was deliberate — unconditional RLSE
would change `fclose()` for httpd, mvsMF, ufsd and ftpd at once. **The MVS half
is measured**, which was the whole gate and the reason PR #170 carried two
commits: mvsdev JOB00229, CC 0000, 2026-09-13.
`TRK(30,5)` + one record + `fclose()` retains **30 tracks without the keyword and
1 with it**, on both the DISP=OLD and the DISP=NEW path — so SVC 99 does accept
`DALRLSE` with DISP=OLD and no space keys, ftpd's exact shape, and CLOSE really
releases. **ftpd#100 / ftpd#127 can proceed** once the consumer is relinked
against a sysroot carrying this libc (it is unversioned — see the installed
`libc.a` date). What the run also surfaced, and is NOT part of #167: **#173**,
`clibdscb.h` models DSCB key presence three different ways behind one accessor.
`struct dscb4` carries a leading `key[44]` that OBTAIN SEARCH does not return,
so `d4.dscb4.dstrk` reads 44 bytes past the field and comes back 0 (JOB00223).
The probe works around it — and so, independently, does `@@listds.c:192`, which
is the point: two consumers, one header defect, the same hand-rolled offset
twice. `dscb4` is a one-line fix, the `@@listds.c` cleanup follows it, and
format-3 extent access is an open design question recorded there, not answered.
Also filed off the back of this work: **#171** (overlapping `strcpy(p, p+1)` in
`@@fpnew.c`/`@@dsalc.c` — benign on target, aborts every ASAN host run) and
**#172** (`__fpnew()` sends no UNIT text unit).

**#184 was merged on PR #186, and it is the second issue in a row whose
filed shape was not its real one.** It reads "fopen of an instream DD abends
S013", and the fix is not in the OPEN at all: JES2 refuses a second
*concurrent* DCB on a spool SYSIN by design — `HASPSSSM`'s `HO300`, already
open and not an execution batch monitor means `HOERR`, unconditionally — so
`fopen()` now answers `NULL` + `EBUSY` instead of letting the address space
die. Plain SYSOUT never reaches that code; `HO200` keeps an open count and is
re-entrant, which is why the check has to look at the direction of the stream
*already holding* the DD rather than the caller's mode.

**Three things from it are worth more than the fix.**

The `ieftiot.h` comment points at `TIOESYIN` (X'04') for "spooled SYSIN" and
X'04' is never set on 3.8j; the marker is `TIOESSDS` (X'02'). A check written
from the header comment compiles, runs and never fires. **A vendored header's
prose is not a measurement** — the code now masks the pair, as
`IFG0RR0B.asm:500` does.

A review claim about `fseek()` was reasoned, plausible, agreed by two readers
and wrong, and **the test caught it, not either reader**: the assertion went
red (`JOB00434`) because `@@fseek.c` satisfies an in-buffer backward seek
without reopening. Writing the claim down as an assertion and running it is
what closed it.

And a run nearly reported a stale pass. `JOB00437` showed REALDS 8/8 with
nothing else printed: IEBCOPY replace does not reclaim the old member's space,
`TEST.LINKLIB` had filled up, MERGE abended `IEC032I E37-04`, SPOOL was
`NOXEC`, and REALDS ran on a bare `COND=EVEN` against the **previous** member.
**Gate every step on the one that installs the thing under test**, and compress
before an IEBCOPY re-deploy. Written into `jcl/tstsysin.jcl`.

Gate: `JOB00438` 13/13 + 8/8, every step 00000; proven red by the same source
on the pre-fix libc, `JOB00439`, `IEC141I 013-C0`. **One stand** — all of it
mvsdev, nothing on TK5.

Open and named, not fixed: `@@start.c:77` opens `dd:SYSIN` unconditionally and
is what creates the collision; a lazily-opened stdin would retire the class.
`ropen.c:103` calls `__aopen()` directly, so `ropen("dd:SYSIN",...)` still
abends — the `grtfile` walk cannot see it.

**#183 was merged on PR #185 and never needed a rank either — it turned out
not to be the issue it was filed as.** It reads "No strncasecmp", and the POSIX
names were indeed missing; what it did not know is that the capability has been
in the archive all along under the MS-style names `stricmp`/`strncmpi`, and that
**nothing declared either of them in any header**, so those were unreachable too
without writing the prototype by hand. A naming and declaration gap, not a
missing implementation, and most of the fix is eight lines of `clibstr.h`.

Worth keeping is how wide the demand already was: three projects had
independently worked around it before anyone filed anything — rexx370
(`src/irx#init.c:210`, *"without strcasecmp (not in crent370)"*), ftpd
(`src/ftpd#adr.c:18`, *"Spelled out instead of `strcasecmp()`"*) and the cobc370
port that eventually did file. **A gap with no ticket is not a gap nobody hit;
it is a gap everybody routed around.** The sweep that found them costs one grep
across the ecosystem checkouts and is worth running before deciding a libc
addition is speculative.

The MVS gate is met: `test/mvs/tststrci.c` is 37/37 on mvsdev, JOB00422,
CC 0000, and **proven red** at 11 of the 37 against a deliberately broken build
(JOB00423). A host run would have folded through the host's ASCII table and
observed none of it.

**And the sweep above has a date, which is the part worth carrying.** It read
`*/src` and `*/include` across the local checkouts and concluded no consumer
carries a private copy of any of the four names — reported as a property of the
ecosystem when it was a property of *what each checkout happened to be pinned
at*. cobc370 was a month stale locally; its `main` does carry
`#define strncasecmp cobc_strncasecmp`. Harmless here only because the `#define`
sits after `#include <string.h>`, so the new declaration is seen first and every
use is renamed past the archive member — collision-safe **by ordering, not by
the absence that was measured**. A `#define` before the include, or a plain
`static int strncasecmp(...)`, would have collided and the sweep would have
missed it either way. Fetch before sweeping, and date the result.

**#168 was filed, fixed and merged on the same pass and never needed a rank
either — but it carries an unpaid gate.** There was no way to close a `FILE`
whose last write failed: `fclose()` flushes first, so when the pending block is
what could not be written it re-drives the failing WRITE, abends inside the
close, and never reaches `__fpfree()` — the DD then stays allocated for the
life of the job and the partial data set can be scratched neither by the
program that created it nor by its user. Measured on mvsdev 2026-09-09
(mvslovers/ftpd#129), an FTP STOR into `SPACE=TRK(1,0)`. The fix is
`__fabandon(FILE *)`: discard instead of flush, `__adisc()` so the DCB agrees
there is nothing pending, CLOSE under `try()` so a close that fails anyway is
reported rather than propagated. **ftpd#129 is waiting on it** — everything
else in that issue is already fixed.

Two things worth keeping from the work. The issue's own account of the
mechanism was wrong in a way that changed the fix: it put the crux on the DCB's
buffer state, and for ftpd's `fopen(dsn,"wb")` shape the culprit is the **C**
buffer — `@@ATROUT` clears `IOFLDATA` *before* the WRITE, so the DCB side is
already quiet when the abend lands. And `lock()` is ENQ `RET=HAVE` keyed on the
pointer value, so the abended `fwrite()`'s hold survives the ESTAE retry;
`fclose()` reads the resulting rc=8 as "an outer caller owns it" (#145) and
leaves a CLIBLOCK ENQ standing on storage it then frees. `__fabandon()` DEQs
unconditionally; **`fclose()` still does not** — a smaller version of the same
leak, now **#174**, deliberately left out of the #168 PR because `fclose()` is
on every consumer's path and #145 is what put the conditional there.

**The gate is paid: mvsdev JOB00245, 2026-09-13, job CC 0000.** All three
steps green, 7/7 checks. The unmeasured question is answered — **BSAM CLOSE
with nothing pending completes** on a data set that is out of space,
`__fabandon()` answers 0 and `remove()` answers 0 from the same address space
that just took the D37. **ftpd#129 can proceed** once the consumer is relinked
against a sysroot carrying this libc (it is unversioned — check the installed
`libc.a` date).

The run also settled two things the issue had guessed at. **Point 1 is the
crux on the target**, not just by reading the source: the SPLIT step drives
`fflush()` and `__aclose()` under separate `try()`s and it is the flush that
abends, with CLOSE straight afterwards clean. And **the second abend is a
program check, not a second D37** — `0x0C4` and `0x0C6` both appeared across
runs for identical code, so re-driving a WRITE against a DCB that has taken an
x37 walks into wild storage. That is now **#176**, and it is the one item here
that is worse than it looks: `__fabandon()` only helps a caller who knows to
call it, while `@@exit.c` walks `grt->grtfile` at termination and `fclose()`s
every survivor **with no ESTAE**, so a consumer that recovers an x37 and simply
returns from `main()` takes the program check in teardown. #176 also carries
the concrete motivation for the x37 exit — it turns the abend into a return
code before any re-drive can happen, the way #147 did with SYNAD, and would
make the abandon dance unnecessary for the common case. **It is `EXLST` type
X'08', not the X'11' ABEND exit this file said at first** — see rank 1. **#176 is rank 1** as of the 2026-09-13 pass — a latent crash in
every consumer, not a missing feature — and #149 sits next to it at rank 24,
because the two are one design question and not two patches.

One measurement kept because it cost four runs, and kept as a correlation
rather than a cause: rescuing a FILE *after* its `fclose()` abended answers 0
in a fresh address space and -2 in one where other cases have already run.
Every -2 had both an earlier IDCAMS DELETE (#127's ENQ escalation) and an
earlier D37 in the same step, and nothing measured separates them — so the
probe runs one case per step and reports that path without asserting it. The
supported use is `__fabandon()` INSTEAD of `fclose()`.

**Deferred, deliberately: `test/mvs/tstfprls.c` does not echo S99ERROR/S99INFO.**
`fopen()` only ever hands back NULL, so a rejected `DALRLSE` would arrive without
reason codes. Getting them means the probe issuing its own SVC 99 with a
hand-rebuilt copy of `__fpold()`'s text units — forty lines that never fire on a
green run, drift silently from what `__fpold()` actually builds, and become the
first suspect the day something really breaks. **Write it against a real failure,
when there is one.** Until then the probe says which of the three things happened
(OPEN FAILED / NO MEASURE / MEASURED), which is what a red run needs first.

**Tier 1 was emptied by seven closures and then refilled by an issue that had
been sitting in the tracker the whole time.** #107, #70,
#80 defect 2, **#11** (PRs #137, #138, #139, #141, merged 2026-08-23 on top of
the CHANGELOG backfill `3e9c15b`), then #145, #147 and now **#108** are all
closed. #11 was the only *observed and recurring* production failure on this
list since July, and #108 was the last open crash hunt — after which, for three
days, nothing ranked here was a failure anyone was seeing on a running system.
**#154 ended that**, and it was not new: filed 2026-08-27, missed by that day's
reconciliation, ranked 2026-08-30 as item 1 — **and fixed 2026-09-07 by PR #166**,
after `mvslovers/ftpd#122` arrived as an outside sighting of it. Tier 1 was empty
for six days, **#176 refilled it on 2026-09-13 and PR #177 emptied it again
the same day** — stdio re-driving a WRITE
on a DCB whose last write abended, measured as a program check and reachable
through `@@exit.c` with no ESTAE and no API call. Below it the picture is
unchanged — two **campaigns**, a **relink round**, and a set of traps that have
not bitten yet, now joined by a **socket measurement set** that is not libc370
defects at all. **The numbering is still left as it stands** — ranks 2 to 23 are
referenced from prose in this file, so nothing is renumbered. Rank 1 was a gap
and is now #176; everything the 2026-09-13 pass added takes **24 and upward** in
tier order, so the number says when an item arrived and the tier says how much
it matters.

**Update 2026-08-26: #145 briefly refilled Tier 1 — and is closed again.**
`vvprintf()`'s nested public `fputs()`/`putc()` released the FILE lock at the
first conversion, so practically the whole printf line ran unserialized — the
measured faces being ftpd#117's S001-1 (reproduced in seconds, JOB02235) and a
writer silently wedged in the corrupted QSAM state (JOB02237, 8 of 400 lines).
**PR #146 merged same day** (fix: internal writers + ownership-aware wrappers;
red/green tests host and MVS, green run JOB02239 CC 0000). Shipped in
**1.0.4**; every multitasking consumer wants that relink. Its known
neighbours are **#147**:
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
deliberate API decision, not a defect. With #145/#147 done and **shipped in
1.0.4**, every multitasking consumer owes a relink — now for four reasons, not
one.

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

**Update 2026-09-04: v1.0.4 is tagged and released** — 31 commits, 5 PRs and 5
issues since 1.0.3, no breaking change. It carries the whole stdio locking chain
(#145 plus #147's four items) and the duplicate-external fix (#151), so the
relink this file has been calling for twice is now available rather than
pending. The one contract change consumers must read is #147 item 3: an
uncorrectable I/O error is `ferror()` + `errno EIO` instead of an
address-space-killing S001, and `feof()` is deliberately *not* set — so a reader
that never checks `ferror()` now gets a short file quietly where it used to die
loudly. That asymmetry is the whole of **#149**, which stays open as a
deliberate API decision. Nothing in Tier 1 or below moved: **#154 is still item
1**, and its probe branch is unmerged.

---

## Tier 0 — the 2.0 critical path (decided 2026-09-30, D8)

**libc370 2.0.0 is released (2026-10-01):** `2.0` merged into `main` (#312,
`460b379`), tagged `v2.0.0`, published as Latest with the migration guide
linked in its notes. 1.x stays reachable as tag `v1.0.8` and branch `1.x`
(`77712b9`). What remains is step 7, the consumer ports.

In this order. The plan and the gates are in `doc/design-2.0.md`; the
checklist is #245.

1. **Prep, cheap, before anything moves.**
   - ~~#241 and #251~~ — done, PR #253, 2026-09-30. `run.sh` runs all 26
     host tests, and the #182/#228 last-block paths and #126's job-list
     teardown are now covered too.
   - ~~#140~~ — done, PR #255, 2026-09-30. All 729 `.s` byte-identical.
   - ~~#243 and #248~~ — done, PR #255, 2026-09-30. miniz, PDF, `emfile` and
     `ipc` removed; `clibsrb.h` fixed and parked in `src/wip/mvs/srb.h`.

   **Step 1 is complete.**
2. ~~**#244: crypto370 released.**~~ — done 2026-09-30:
   [mvslovers/crypto370](https://github.com/mvslovers/crypto370) v1.0.0,
   built against libc370 v1.0.8, known-answer tests 72/72 on mvsdev
   (JOB00956). `clibb64.h` became `base64.h` and the base64 symbols lost
   their `@@`; SHA-256 no longer needs `<clib64.h>`, so it builds against
   1.x and 2.0.

   **Adoption is not a prerequisite of the cut** (decided 2026-09-30). The
   consumers' release builds pin 1.x and step 3 holds their build CI, so the
   cut cannot redden them; they switch to crypto370 in their 2.0 migration
   PR (step 7), which touches their includes anyway. What the cut needed was
   only a released crypto370 for that PR to depend on. The files leave
   libc370 in phase 1.

   **Step 2 is complete.** Next is step 3, right before phase 1 merges.
3. ~~**Hold the consumers' build CI**~~ -- **not done, on purpose** (Mike,
   2026-10-01): an unported consumer is pushed only by its own port, so its
   build CI meets 2.0 exactly when it is migrated. Release builds pin tags
   and were never at risk. Original plan: `libc370_ref: v1.0.8` in each consumer's
   `build.yml`. The cut lands on `main` with phase 1, not with the release, so
   this goes in right before phase 1 merges, and not earlier, or it silences
   the early warning for longer than necessary.
4. ~~**Phase 1: header moves**~~ (#256) — **done on `2.0`, 2026-09-30.** It
   runs on the integration branch `2.0`, so no consumer sees it until `2.0`
   merges into `main`; step 3's hold goes in right before that merge. Every
   PR passed `sdk/gate.py` (identical assembler unless `gate-allow.txt` names
   the reason, identical archive, no new warning or implicit declaration).
   - #257 tooling; #258–#262 the 97 moves (four `ibm/mvs/` names corrected
     against the IBM sources); #263 crypto + `clib.h` removed
   - #264 split proposal (agreed A–D); #265–#268 the five splits; #269
     `clibthdi.h` into `mvs/thread.h`
   - #270 #68 step 1 (a real bug: user abends were reported as `UD`); #271
     #39 step 2 — implicit declarations 62 → 0
   - headers: 99 moved, 5 split, 18 removed, 17 ISO unchanged, 14 internal
     left for phase 2. Leftover: `modmap.h` includes the internal `bsam.h`
     until phase 2 moves it
5. **Phase 2: internals out of the sysroot.**
   On the branch `2.0` (#274), 14 rows reviewed 2026-09-30:
   - #275: `@@75.h` and `get3.h` removed; `__75.h`, `clibres.h`, `clibspl.h`
     moved to `src/internal/` (the last two stay for a later review;
     `clibspl.h` is a Metal C candidate)
   - #276: `@@memmgr.h` and the never-built `USE_MEMMGR` branches removed
   - #279: layout decisions recorded in `doc/design-2.0.md` — D1 `ext/`
     instead of `libc370/`, D10 internal includes by path from the root,
     D11 `libc370/socket.h` → `mvs/socket.h`; `clibwsa.h` becomes public
     (`mvs/wsa.h`: httpd and lua370 call `__wsaget()`), `clibjs.h` too
     (`mvs/jes2spool.h`)
   - #280: private headers included by their path from the root (D10);
     #281: `libc370/` → `ext/`, `libc370/socket.h` → `mvs/socket.h`
   - #282: the remaining rows — `printf.h`, `crtanchor.h`, `clibsvc.h`,
     `enqpl.h`, `x75.h`, `calendar.h` internal; `mvs/wsa.h`,
     `mvs/jes2spool.h` public; `modmap.h` trimmed to `loadmod.h`
   - **phase 2 done** (summary on #274); the migration script is still owed
   - #284: name maps for it — `sdk/names.tsv` (generated), `sdk/removed.tsv`
     (871 names, each with its replacement; CI-enforced), procedure in
     `doc/migration-2.0.md`. Every later move updates them in the same PR
5a. ~~**Phase 3 in 2.0 (#278, D9):** sources by area~~ **done** (#285–#293,
   summary on #278): `src/` mirrors `include/`, `sdk/srcmap.tsv` records
   every move, `attic/` holds what is not built; the three `ibm/` headers
   that declared functions followed in #296. #256, #274 and #278 closed
   2026-10-01. Phase 4 (the OS seam) stays in 2.x.
6. **Interface changes and consumer wishes (Tier 5), now** (decided
   2026-10-01). Formerly called "the relink round" -- the name was
   misleading. What these share is that consumers wait for them; three of
   them also change an interface (a struct grows, a function gains a
   parameter), which needs every consumer *recompiled* against the new
   headers, and 2.0 forces exactly that rebuild anyway. Done after 2.0.0, they
   would be the next major break.
   - interface changes: **#80 defect 1** (`__listpd()` `max`), **#79**
     (`JESJOB` submit time), **#50** (catalog name in `DSLIST`; decide the
     method first)
   - consumer wishes, no interface break, taken along: **#51**
     (`inet_addr`/`inet_ntoa`), **#71** (`idcams()` keeps IDCnnnn),
     **#172** (`__fpnew()` UNIT)
   - ~~**investigate the three `ibm/` headers that declare functions**~~
     done, #296: `iefssreq()` → `mvs/subsys.h`, `rdjfcb()`/`__rdjfcb()` →
     `mvs/dd.h`, `__getpso()` → `mvs/jes2.h` (Process SYSOUT research: #295)
   - **done on `2.0`:** #51 (#297: `<arpa/inet.h>` on POSIX/BSD terms --
     `in_addr_t` an integer, `inet_aton()` 1/0, new `inet_addr`/`ntoa`/
     `pton`/`ntop`); #71 (#298: `idcams_sysprint()`, measured on MVS --
     IDCAMS's own msgno is not the IDC number, JOB01058/JOB01060); name-scan
     fixes on the way (#287 `#else`, #299 function-pointer typedefs)
   - #79 (#302): `JESJOB.submit_time64`/`sysid` at 0x50, measured with a
     held job (JOB01065/01066); found #301 (`jesjob()` uses `strtok()`)
   - #300: libc370 compiles as C99 (`-std=gnu99 -trigraphs`); eight TUs
     differ in code generation only (analysed in the PR)
   - #80 (#303): `__walkpd()` (callback, no allocation -- SYS1.SMPCDS's
     23018 members walked, JOB01076); `__listpd()` NULL/ENOMEM instead of a
     short list. #61 (`__listvl()`, same silent truncation) to follow the
     same convention
   - #172 (#304, migration row #306): `fopen()` takes `unit=`/`volser=`
     and `DATASET_UNIT`/`DATASET_VOLSER`; `S99NOMNT` when either is named
     -- without it an unmounted volser waited on IEF238D for the operator
     (JOB01082), with it NULL at once (JOB01084). Found on the way: the
     mode string never upper-cased a parenthesised list. Same wait still in
     `__dsalc()` `VOLSER=` (#305, ftpd's STOR path)
   - #50 (#307): `DSLIST.catnm`, a shared `const char *` from LISTCAT's
     per-entry `IN-CAT` line (JOB01086) -- 104 bytes, still 128 of GETMAIN
     where a `char[45]` would cost 192; measured through `__listds()`
     (JOB01088). Found #308 (an entry without VOLSER swallows the next)
   - **Tier 5 done.** Left over from it: #305 (`__dsalc()` `VOLSER=` waits
     on IEF238D), #308, #61 (`__listvl()` truncation)
   Each with a test and a CHANGELOG entry; the gate shows the intended code
   changes (`gate-allow.txt`); anything touching MVS behaviour is tested on
   MVS (Mike's OK per run).
7. **Consumer migration -- by an agent, not a script** (decided
   2026-10-01, #309): `doc/migration-2.0-agent.md` is the brief, with
   `doc/migration-2.0.md` as the specification. Consumers port against the
   rolling tag `v2.0.0-dev` until the release; **since 2.0.0 they pin
   `[toolchain] libc370 = "2.0.0"` and drop any `libc370_ref`** (the build CI
   follows `main`, which is 2.0). ufsd and brexx370 switch from `2.0.0-dev`
   the same way. `edge` stays on its 1.x commit until brexx370#274 merges.
   **Locally there is one sysroot, on 2.0** (2026-10-01: libc370 `v2.0.0`
   installed into `~/.local/cc370`, the second toolchain removed): nothing is
   built against 1.0.8 any more, so an unported project does not build here
   until it is ported. The brief `doc/migration-2.0-agent.md` is kept out of
   the tree on purpose (maintainer's working copy, updated for this state;
   it compares against 1.x with `-nostdinc -I <v1.0.8>/include`).
   - **ported:** ufsd (ufsd#82, merged 2026-10-01, prerelease `v1.4.0-dev`),
     brexx370 (brexx370#274, open; moves its pin off `edge` -- after it,
     nothing pins `edge`). Findings folded into the brief (#310).
   - **next:** ftpd, httpd, mvsMF -- they take ufsd as `">=1.4.0-dev"`;
     httpd and mvsMF also crypto370. ufsd publishes **prereleases only**
     until all three are ported (a stable release would reach their `>=`
     ranges; a prerelease does not).
8. ~~**Prerelease `v2.0.0-rc1`**~~ -- skipped (2026-10-01): ufsd and brexx370
   were ported and tested against `v2.0.0-dev` instead, and 2.0.0 was
   released directly.
   ~~Before it, run the stdio MVS probes once on the `2.0` build~~ --
   **not needed** (2026-10-01, #300 comment): compiled gnu89 vs gnu99 and
   assembled, `@@fpswt` and `@@fseek` are byte-identical objects (label
   numbering only). Of the six TUs whose code did change, `@@listds` ran on
   MVS (JOB01088) and `@@loadhi` runs at every start of the ported ufsd (its
   SSI router into CSA); `bsearch`, `strtoul`, `vvscanf`, `@@tmthrd` rest on
   #300's analysis.
   `main` stays 1.x and the consumers' build CI stays green. Check first
   that `release.yml` and the consumers' `make deps` / `[toolchain]` accept a
   prerelease tag off `main`.
9. ~~**First consumer on 2.x against the rc**~~ -- done as ufsd and brexx370
   against `v2.0.0-dev` (step 7). -- ftpd proposed (smaller than
   httpd/mvsMF, gains from #80 and #71, no crypto370): migrated by the
   script on a branch, rebuilt (that rebuild is the relink), tested on MVS.
   Defects in libc370 or in the maps make `rc.2`, and so on.
10. ~~**Release 2.0.0**~~ -- **done 2026-10-01** (#311, #312, `v2.0.0`),
    without the hold PRs (step 3) and without the rc (step 8). Requires the
    current cc370 (cc370@latest; built with `47b3545`) -- added to the
    CHANGELOG and the release notes afterwards. Original plan: step 3's hold PRs in the consumers (Mike's go), `2.0`
    → `main` (Mike's go), tag 2.0.0 with the CHANGELOG migration section and
    the minimum cc370 commit; then one migration PR per consumer, each
    removing its `libc370_ref` line again. httpd's and mvsMF's also add
    `mvslovers/crypto370` to `[dependencies]` (httpd: all three; mvsMF:
    base64) and change `clibb64.h` to `base64.h` (httpd: one file; mvsMF:
    three), saying why the dependency is added.

Not blocking: cc370 releases (mvslovers/cc370#523). Without them, 2.0.0 names
its minimum cc370 by commit, as 1.0.7 and 1.0.8 did. After 2.0.0: phase 4
(phase 3 moved into 2.0, D9), #39 step 3 (`-Wall` with the three `-Wno-` flags), #68 steps 2–3, and
#246 (function naming, 3.0).

---

## Tier 1 — after 2.1.0

**libc370 2.1.0 released 2026-10-03** (tag `v2.1.0` on `e65d317`), with its
artifacts (#326): sysroot tarball, `.deb`, `.rpm`, metadata.json,
SHA256SUMS, built with cc370 v1.1.0. cc370's `install.sh` picks it by its
metadata and links a program against it (checked end to end on macOS).

1. ~~**#325**~~ — the C99 audit, done 2026-10-03 (PR #343):
   `doc/c99-audit.md`, rerun with `sdk/c99audit.py`. 18/24 headers, 476/893
   names declared, 154/463 functions in `libc.a`, nothing declared that does
   not link. Its gaps, ranked:
   1. ~~#336~~ PR #353, ~~#337~~ PR #354, ~~#338~~ PR #356 (mvsdev JOB01310/
      01311), ~~#339~~ PR #357 (JOB01313), all 2026-10-03, unreleased (2.2.0).
      The audit now: 20/24 headers, 547/893 names, 158/463 functions, none
      declared differently from C99.
   2. ~~**#355**~~ printf `0`/`-` flags and sign width for `%f/%e/%g` - fixed
      2026-10-03, PR #358 (mvsdev JOB01315 19/19, 2.1.0 fails 13).
   5. **#340** `<math.h>` C99 additions - large; HFP has no NaN/inf; five
      8-character name-collision groups need `asm` labels.
   6. **#341** wide characters (`<wchar.h>` functions, `<wctype.h>`) - only
      on demand.
   7. **#342** `<complex.h>`/`<fenv.h>`/`<tgmath.h>` - decided 2026-10-03:
      left out, documented (PR #352); open only as a resubmission.
2. ~~**#344**~~ -Os is the default since PR #346 (2026-10-03, unreleased,
   goes into 2.2.0): -8,117 text bytes (-2.9%); libc370 55/55, rexx370 and
   brexx370 identical on MVS. `LIBC370_OPT=-O1` goes back. #345 (tstanchr
   SA03, also in 2.0.0) came out of the series - fixed by PR #359: tmr_stop()
   deleted the timer thread before it had ended (mvsdev JOB01320).
2. ~~**#326 leftover**~~ — closed 2026-10-03: `pair.yml` (PR #335) installs
   the pair under apt (bookworm amd64/arm64) and dnf (fedora), run
   37077154678 all green; it runs after every release.

### Tier 1 before — empty since #182 (PR #227, 2026-09-29)

### ~~1 · #182~~ — fixed, PR #227, 2026-09-29

Fallout of #176, found relinking mvsMF against v1.0.6 (mvslovers/mvsmf#366).
#176's EXLST X'08' exit turns an out-of-space on a WRITE into a return code,
but only for the writes that go out through `@@AWRITE`. The last block is
written by `FIXWRITE` inside `@@ACLOSE` (`asm/@@aclose.asm:24`), which ends
`FUNEXIT RC=0` unconditionally (`:44`, untouched since the initial commit);
`fclose()` ignores both `__fflush()` and `__aclose()` and returns 0
(`src/clib/fclose.c:30,33,42`). No return code, no `_FILE_FLAG_ERROR`, no
`errno`: **before #176 this was an abend, now it is silent data loss**
for any consumer whose final block meets a full data set. Measured with a mvsMF PUT into a
`TRK(1,0)` PS on mvsdev (2026-09-14). Next: a red MVS probe where exactly the
final short block does not fit, then carry the condition out of `@@ACLOSE`
through `__aclose()` into `fclose()`'s return value.

**Merged as PR #227 (2026-09-29, `14edfa7`), `edge` moved there, sysroot installed from main:** `@@ACLOSE` returns 12/8 like `@@AWRITE`, `fclose()` returns
`EOF` + `ENOSPC`/`EIO`. `test/mvs/tstclspc.c`: R=190 on mvsdev, 195..199 lose
the short block; red JOB00729 (`fclose()` 0), green JOB00730 (EOF, errno 28),
7/7. The other `__aclose()` callers were **#228** (rank 40, PR #230, merged the same day).
Consumers informed 2026-09-29: mvslovers/ftpd#154 (STOR 250 on a lost tail),
mvslovers/mvsmf#371 (PUT 204, closes #366's KNOWN GAP), mvslovers/httplua#10
(STDOUT temp data set). ftpd and mvsMF pin `libc370 = "1.0.6"`, so **#182 only
reaches them with a release**. brexx370 (pins `edge`) is handled by its own
session. cobc370 and RAKF are outside mvslovers and not filed; RAKF links crent370.
**Released in v1.0.7 (2026-09-29, `1fdbde1`)** together with the 20 other PRs
since 1.0.6 - 17 of them had no CHANGELOG entry until the release commit. The
consumer issues carry a comment pointing at the release notes' contract list.

### ~~1 · #222~~ — fixed, PR #224, 2026-09-29

Filed 2026-09-29 out of #209. `__dblcvt()` writes with `strcat` into a caller
buffer it has no length for: 50 bytes on the plain `%f` path (`vsnprint.c`,
`vvprintf.c`), 80 in `__examin()`, and its own `work[125]`. Plain
`printf("%f", 1e41)` is enough, measured under ASAN on the host; on MVS every one
of those buffers is an automatic. A length parameter for the internal
`__dblcvt()` fixes it, and three call sites change.

**Merged as PR #224 (2026-09-29, `7906cee`):** `__dblcvt(..., rsize)`, `numbuf` 96, `__examin()`
`work` 128. Host `tstdblrb` red 18 ASAN / green 79/79; old vs new identical
over 30.6M conversions; mvsdev JOB00722 59/59, pre-fix JOB00724 S0C4 (PSW
00F0F0F6). Found along the way and filed: #225 (the HFP `/10` scaling, 1e60
prints 9999...8) and #226 (`%+8.2f` puts the sign before the padding).
brexx370 (2026-09-29): #222 reaches its TRUNC/FORMAT/ROUND (`%.*f`,
`%*.*lf`, via `__examin()`); nothing to pull forward, `edge` after the merge.
#226 does not touch FORMAT: a plain width puts `-` after the padding
correctly (`%8.2f` of -1 is `   -1.00`, host, main and #224).

### ~~1 · #209~~ — fixed, PR #223, 2026-09-29

The rounding cap was `DBL_MANT_DIG`, 14 *hex* digits on S/370. Now 17 decimal
digits, and zero is not rounded. Red JOB00712, green JOB00714 (29/29, mvsdev);
host `test/host/tstdblcv.c`. mvslovers/brexx370#156 waits for it. Its two
neighbours are ranked at 38: #220 and #221.

### ~~1 · #218~~ — fixed, PR #219, 2026-09-29

`__start()` copied ten words of a CPPL into `grtptrs` (no VL bit to stop at),
six from past the list. The CPPL is now recognised *before* the copy by the
#210 test (`GRTFLAG1_TSO` + PSCB word == `ppapscb`, words 1-2 read only if no
VL bit precedes them) and gets exactly four words. Red JOB00699 (CP and CHILD
n=10), green JOB00701/00704 (n=4; BATCH and CALL stay n=1) - batch IKJEFT01
only. Left as is: a list with no VL bit that is not a CPPL still gets ten
words, documented in `clibgrt.h`. brexx370's `jccompat.c` `>= 4` check holds
at exactly 4.

### ~~1 · #210~~ — fixed, PR #217, 2026-09-29

`tsocmd()` (and `ispexec()` on top of it) always returned 8 with "No CPPL":
`@@CRT0` clears the PPA and nothing wrote +X'2C'. `__start()` now records
`pgmr1` as the CPPL when `GRTFLAG1_TSO` is on **and** the CPPL's PSCB word
equals `ppapscb` from EXTRACT. Red JOB00677/00681, green JOB00683
(`test/mvs/tstcppl.c`: batch, TSO CALL and TSO command, the last LINKing
itself through `tsocmd()`), and in a real TSO foreground session (3270 as
MVSCE01, module briefly in SYS2.LINKLIB). `edge` moved to `832d794`; brexx370
told its `jccompat.c` CPPL workaround can go once it builds against it. Side datum for
#105: a second `__start()` in one address space does happen - a C program
LINKed by `tsocmd()` (the CHILD in JOB00683) - and the CHILD's `grtptrs` start
fresh (n=10, its own CBUF at [0]), not appended to the parent's, and the
caller's CPPL and `grtptrs` are unchanged after it returns (cell c4,
JOB00686). The `grtptrs` loop reading 10 words for a CPPL was **#218**, fixed
above.

### ~~1 · #197~~ — fixed, PR #215, 2026-09-29

The `MODESET` now happens only for an APF-authorized caller that is not
already in supervisor state. Red on mvsdev JOB00655, green on JOB00659
(`test/mvs/tstracun.c`, not authorized). This unblocks brexx370's `rac/` →
libc370 cleanup (brexx370 #134); `edge` has moved to it. Not measured:
TK5, and the AC=1 probes `tstracau`/`tstracmx` against the new library; cell
(3) covers the same authorized path through JSCBAUTH.

### ~~1 · #211~~ — fixed, PR #212, 2026-09-29

`%zd` printed `d`: `__examin()` (`@@examin.c`) did not know `z`, `t`, `j` or `hh`.
Side finding **#213**: libc370's `ptrdiff_t` was `int` where cc370's is `long` -
fixed together with #195 (PR #214).

### ~~1 · #189~~ — complete, PR #208, 2026-09-27

All six steps are merged (#201, #202, #203, #205, #207, #208); the closing
comment on #189 has the table. Left open by decision: #204 (append to a
member) and #206 (O(1) backward seek via NOTE/POINT).

**#206 is `parked` (2026-09-29).** Its `LINES()` motivation does not hold:
brexx370's `Llines()` reads forward to EOF before it seeks back, so NOTE/POINT
only halves `do while lines(f) > 0`, and nothing in the ecosystem calls it
outside brexx370's own tests. The one case it would fix is the direction
switch on a `+` stream (`__fpswt()`, O(n) per switch). **Trigger:** a measured
direction-switching `+`-stream workload - not `LINES()`, which is brexx370's
to solve with a 0/1 look-ahead.

**Rolling tag `edge`** (since 2026-09-27, now on `7906cee` = #222 merged; the sysroot here is installed from `7906cee`, 2026-09-29): brexx370's MVS CI
clones libc370 at its `[toolchain]` pin, so it tracks `edge` in the meantime.
**Move `edge` after each of these merges once its MVS gate is measured**
(`git tag -fa edge <sha>` + `git push -f origin edge`), and cut a real release
before brexx370 does.

### ~~1 · #176~~ — fixed, PR #177, 2026-09-13

Kept in short form because the *mechanism* has to outlive the diff.

`IFG0554T` — the module named in the `IEC031I D37-04` line — scans the DCB
exit list for an **`EXLST` type X'08'** entry before it abends, and takes
`R15=1` as "rewrite the format-1 DSCB, clear the unit-exception bits, drop
FEOV, and return to the access method to drive the caller's SYNAD with output
error, no space available". `@@AOPEN` now plants that exit, so an
out-of-space write arrives as `ferror()` + `ENOSPC` on machinery #147 already
built, and `__fflush()` reaches its `reset:` label — leaving nothing for
`fclose()` to re-drive.

**Not the X'11' ABEND exit**, which this file and the issue said first.
`EXLDCBAB EQU X'11'` appears exactly once in the whole MVS 3.8j source tree,
in its own definition in `IHAEXLST`, and no module consults it. Defined and
never taken.

Measured mvsdev **JOB00247: CC 0000, 5/5 PASS, and no `IEC031I` line in the
job log at all** (`test/mvs/tstx37.c`, deliberately without `try()`). Cross-
checked by JOB00249: `test/mvs/tstfabnd.c`, whose three steps each produced a
D37 in JOB00245, now report `try()` = 0 in all three.

**Three things this leaves behind.** `errno` is `ENOSPC` and not `EIO` —
`@@AWRITE` answers 12 for the x37 case and 8 for a SYNAD error — which is the
distinction **rank 24 (#149)** needs and the reason to do it next. An x37
raised *by CLOSE* now becomes S001 rather than D37: strictly less legible,
unobserved (JOB00245 measured that CLOSE with nothing pending completes on
`TRK(1,0)`), and the one place this change makes a failure harder to read.
And **rank 25 (#174)** is untouched: `fclose()` still leaves a stale
`CLIBLOCK` ENQ when a caller's `fwrite()` abended for some *other* reason.

The MVS half of #168's probe is superseded and reports retirement instead of
failure — a probe whose defect has been fixed must not read red.
`__fabandon()` itself is unchanged and still the way out for any other abend
a caller's ESTAE recovers mid-write.

---

### #154 is fixed — `recv()` now caps its X'75' chunk at 256 (PR #166, 2026-09-07)

Kept here in short form because the *reason for the number* has to outlive the
diff: 256 is not a tuning choice and cannot be raised again.

X'75' copies in 256-byte segments and the instruction is restartable. The guest
side has architected state to resume from after a nullifying exception — the base
register and R1 — and the host side has none: upstream `x75.c` recomputes its
pointer from `map32[R2]` on every entry, and R2 is a slot index that never
advances. So a copy that faults after a completed segment finishes from the
**start** of the host buffer, and the tail of the read is a replay of its head.
`vstorec()` resolves both page addresses through `MADDRL` before either `memcpy`,
which makes one segment atomic against the exception — that is why 256 or less is
immune by construction rather than merely less likely, and why every larger cap
looked like a fix and then failed: 4096 here since `cd43a70`, then 2048 in mvsMF,
which failed five days later.

**The cap is permanent.** A guest cannot detect a patched emulator — no return
value, status bit or function code distinguishes one — so the host-side fix
(SDL-Hercules-390/hyperion #884 / PR #885, merged 2026-09-06 as `4675e7e1`) never
makes this removable: the same build has to keep working on an unpatched host.

Evidence, for anyone who has to re-open this: the forced-fault probe
`test/mvs/tst75rst.c` (`jcl/tst75rst.jcl`, red `JOB03045` RC=8 under
`gf1f1f9d1`, green `JOB03046` RC=0 under `g392c22c6`, `first_bad` = 256/512/768
at those boundaries and clean at the boundary-0 control), and the production
sighting `mvslovers/ftpd#122` — a 4577-byte ASCII upload received as
`orig[0:2560] + orig[0:1536] + orig[4096:4577]`, first bad byte a multiple of 256,
tail a clean replay of the head, total length exactly right. Read the probe's RC
together with the emulator trace and never alone: a clean run has two causes,
resumed correctly and never faulted, and the guest cannot tell them apart.

**Follow-ups this opens.**

- Every X'75' consumer wants a relink on the next release — now for five reasons,
  not four. ftpd is the one with a filed sighting (#122), and its ASCII dataset
  upload path is the reproducer.
- mvsMF can return `receive_raw_data()` to bulk reads: it has read one byte per
  `recv()` since `4bc1014` purely to stay inside one segment. Not filed yet.
- **`@@75send.c` is still uncapped** (`:46` passes `len` straight through), the
  mirror image with the host buffer losing its leading segments. Left out of #166
  on purpose — `send()` returns a byte count and callers loop on partial writes,
  so capping it changes what every caller sees per call, not just the instruction
  count. Its own decision and its own issue: **#160, now rank 27**. The asymmetry in
  what has been observed fits the mechanism: a send buffer was just written by the
  application and is hot, while a receive buffer can have lain idle across an I/O
  wait — exactly when its pages get stolen.

---

## Tier 2 — campaign: unchecked allocation

### ~~40 · #228~~ — fixed, PR #230, 2026-09-29

#182's other paths: `freopen()`, the `+`-stream turn and `rclose()`. Backward
`fseek()` was never one (the issue's own correction, #200's `ESPIPE`).
`rclose()` returns -1 + `ENOSPC`/`EIO`; a turned `+` stream stays open and
readable with `ferror()` set; `freopen()` still succeeds (C99 7.19.5.4) and
leaves `errno` at `ENOSPC`/`EIO`, documented in `doc/consumer-notes.md`.
`tstclspc.c` 18/18: red JOB00744 (1.0.7), green JOB00745, mvsdev only.
Contract change with live consumers on the turn (brexx370 stream I/O, Lua
`file:seek` on `w+`/`r+`). Not released yet; `edge` and the sysroot follow
the merge.

### ~~1 · #235~~ — fixed, PR #238, 2026-09-29

See "Recently landed". cc370#483 (PR cc370#512) no longer breaks the libc370
build. `edge` moved to the merge (`9ed55db`), sysroot installed from main.

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

### ~~3 · #249~~ — fixed, PR #252, 2026-09-30

See "Recently landed". The warning count is reported, not gated: #39 step 3
turns it into a gate. `release.yml` has not run yet; its first run is v1.0.8.

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

### 4 · #39 — implicit declarations: step 1 landed (PR #242), 133 in 108 TUs left

*Step 2 is now part of Tier 0, step 4 (phase 1); step 3 follows 2.0.*

**Step 1 is done** (PR #242, 2026-09-30): the 8 routines with no prototype are
declared, and the 3 calls that reached `@@ARCOU`/`@@ARFRE`/`@@WTOTB` only through
the `__` → `@@` mapping call the declared names. Byte-identical assembler in all
23 touched TUs. Re-measured numbers and the full lists are in the
[2026-09-30 comment on #39](https://github.com/mvslovers/libc370/issues/39#issuecomment-5904862790);
the issue text's 161/129/712 are stale.

**What is left is step 2, 133 hits in 108 TUs — and half of it is one line.**
`include/clibsa.h:65` (the `sprintf` in `sa_get_epname()`) accounts for 66 of
them, because `clibwto.h` pulls it into every TU that reports to the console.
That line is **#68 step 1** — the libc370-only part, which reddens no consumer
(the attributes are step 3 there) — so doing it first takes half of #39 step 2
with it. The other 67 are TU-local missing `#include`s.

**Step 3 is bigger than the issue says.** `-Wall` prints ~6690 lines today, and
~4660 of them are three header classes: `"/*" within comment` (`txt99.h` alone
2136), `ignoring #pragma pack`, and the `??!` trigraph in `socket.h`. Unless the
headers are cleaned first, step 3 needs `-Wno-comment -Wno-unknown-pragmas
-Wno-trigraphs` (cc370 honours all three), or any new warning is lost among them.
Header cleanup is a natural part of the 1.1.x restructure.

`strncmpi.c` was one of the missing-`#include` cases: it called `tolower()` with no `<ctype.h>` in
scope, so it resolved to the out-of-line function in `tolower.c` instead of the
`__tolow[c]` macro — two `L 15,=V(TOLOWER)` call sequences per character where
there should be one table load. It computed the right answer throughout, and it
had **no in-tree caller and no test**, which is how it kept that for as long as
it did. The pattern is worth naming for step 2: the TUs that keep a missing
`#include` longest are the ones nothing calls.

### 5 · #68 — `format(printf)` for `wtof()`/`wtodumpf()`/`wtorf()`

*Step 1 is now part of Tier 0, step 4 (phase 1). Steps 2–3 follow 2.0.*

Cheap here, **expensive across the ecosystem**: consumers clone libc370 `main`
unpinned, so the attribute turns httpd, mvsMF and ftpd CI red — with the breakage
in *their* code. Keep the order the issue prescribes:

1. fix the two "too many" cases and the `clibsa.h` inline here,
2. sweep the consumers with locally attached attributes (needs no libc370 change),
3. only then land the attributes in `clibwto.h`.

---

## Tier 4 — structural traps

### ~~6 · #140~~ — fixed, PR #255, 2026-09-30

See *Recently landed*.

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

### ~~24 · #149~~ — fixed, PR #180, 2026-09-13

Fail-fast landed on every stdio entry that can reach the access method, and
`ferror()`/`feof()` as macros now answer 1/0 instead of the raw flag value.

The issue's premise was wrong twice. `clearerr()` has been in the tree since
the initial commit (`48111ed`, 2024-09-12) — nothing to write. And the case
against fail-fast that #149 itself raises, *"ftpd/httpd/ufsd log through these
streams"*, does not hold for this ecosystem: all four log through **WTO**
(`wtof()`, 394 call sites), hold no long-lived `FILE*` for a log, and not one
of them calls `clearerr()`. Every stdio stream in them is per-request — open,
transfer, check `ferror()` once, `fclose()`.

What decided it was `test/mvs/tstnospc.c` on mvsdev. Before (JOB00252): after
one `ENOSPC`, **46 of the next 50 `fwrite()` calls returned the full 80 bytes
and not one of those records reached the disk**. After (JOB00254): all 50
refused at the call, at no measurable cost — 253 µs against a 254 µs
measurement floor, where a write that actually reached the access method and
failed cost 2355 µs.

`_FILE_FLAG_ENOSPC` (0x0004) is the new bit that lets a refused call report
`ENOSPC` rather than a stale `errno`; the FILE keeps no errno and `@@AWRITE`
clears `IOSFLAGS` before returning. It rides with `_FILE_FLAG_ERROR` and is
cleared wherever that is cleared.

Two things the work turned up and deliberately did **not** touch — see #178
and #179: `@@fseek.c` clears `_FILE_FLAG_ERROR` on *any* seek (C says `fseek`
clears EOF only, `rewind` clears both), and `@@reopen.c` sets the error flag on
a **healthy, still-open** stream when `freopen()` fails to open the new one.

---

### 25 · #174 — `fclose()` leaves a stale `CLIBLOCK` ENQ on freed storage

Split out of #168 and deliberately left out of PR #175. `lock()` is ENQ
`RET=HAVE` keyed on the pointer value, and rc=8 means **this task** already
holds the resource. `fclose()` reads that as "an outer caller owns it and I must
not DEQ" — right for #145, and wrong for the case rank 1 describes: an
`fwrite()` that abended took the FILE lock and the ESTAE retry never released
it. `fclose()` then skips the `unlock()` and `free()`s the FILE anyway, leaving
an ENQ standing on storage that is back on the free chain. The next FILE
`malloc()`ed at that address gets rc=8 from its own `lock()`, believes an outer
caller owns it, and runs unserialized; a subtask that locks it waits on an ENQ
nobody will release.

`__fabandon()` already DEQs unconditionally — there is no legitimate outer
holder of a FILE that is being destroyed — so the argument is settled. What is
not settled is applying it to `fclose()`, which is on every consumer's path and
where #145 is the reason the conditional is there in the first place.

Cheap to make red: `test/host/tstfcls.c` already models `RET=HAVE` and asserts
`hold_n == 0` at the end, and `test/host/tstfabnd.c` case (F) already pre-locks
the FILE to cover exactly this for `__fabandon()`. One line.

---

### 26 · #165 — does a partial X'75' RECV consume what it returned?

A measurement, and the only one in the socket set whose answer could be a live
corruption in every consumer rather than a property of an unexercised path.
`@@75recv.c` loops on partial reads, as every guest must; if a partial RECV does
*not* consume what it returned, that loop replays bytes.

It comes from a corruption report in `twinslow/mvs_nfsd`, `socktest/`: first bad
byte at offset **256**, tail carrying the message from byte 0. The #154 restart
mechanism cannot produce that — under restart the first call moves a single
segment, which is immune, and the second moves two segments to `buf+256`, so a
fault puts the first bad byte at **512**. The report was cited in
SDL-Hercules-390/hyperion#884 and has since been retracted *there*, which is
what makes it an independent suspect rather than a duplicate.

Above the rest of the set because it is cheap and load-bearing:
`test/mvs/tst75rst.c` already carries the loopback pair the program owns both
ends of, a byte pattern that makes a replay unmistakable, and the low-level
`__75()` path — so one measurement is one pair of X'75' instructions rather than
a retry loop. Their own harness is not the route: it wants `<sys/socket.h>` and
five other headers libc370 does not have, and porting 461 lines plus a Python
driver is more work than a yes/no question warrants.

---

### 27 · #160 — `@@75send.c` caps nothing: the mirror of #154 on the send side

The line Tier 1's #154 entry called "still not filed". `@@75send.c:46` passes
`len` straight through. Same restart mechanism, opposite direction: on SEND the
guest buffer is the *source*, so a fault mid-copy leaves the **host** buffer
missing its leading segments, with the tail shifted down over them. 256 bytes or
less is immune by construction, and a guest cannot detect a patched emulator —
no return value, status bit or function code distinguishes one — so the
host-side fix (hyperion #884 / PR #885) never makes the cap removable.

Not a mirror of #154's *effort*, which is why it is here and not beside it.
`recv()` loops internally, so #154 was one line; `send()` returns a byte count
and callers loop on partial writes, so a cap turns one call into several and
changes what every caller sees. Three options, none chosen: cap and loop inside
`send()` (matches `recv()`, changes the meaning of a short return in a way
callers may already depend on); cap and return the short count (honest, audits
every consumer's send loop); document it and do nothing (rejected for `recv()`,
and the same argument applies).

**What it needs first is a measurement, not a patch.** `test/mvs/tst75rst.c`
inverts: put a page boundary at a chosen multiple of 256 inside the *send*
buffer, release the page beyond it immediately before the send, have the peer
verify what arrived. Every sighting in the ecosystem so far has been on the
receive side, which fits the mechanism — a send buffer was just written by the
application and is hot, a receive buffer can have lain idle across an I/O wait,
exactly when its pages get stolen — but that is an argument about likelihood,
not about correctness.

---

## Tier 5 — consumers waiting (interface changes and wishes, now Tier 0 step 6)

*The whole tier ships in 2.0 (Tier 0, step 6): #80 defect 1, #79 and #50
change an interface and need every consumer recompiled, which 2.0 forces
anyway; #51, #71 and #172 are taken along (decided 2026-10-01).*

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

### 28 · #172 — `__fpnew()` sends no UNIT text unit

Every `fopen()` that takes the DISP=NEW path allocates on whatever the system
default hands it. Filed off #167's target run, where it is why cases (4) and (5)
of `test/mvs/tstfprls.c` have to report "could not measure" instead of failing:
if the default is not what the probe expected, the `fopen()` simply does not
happen.

In this tier rather than lower because it changes allocation behaviour for every
consumer that creates a data set through `fopen()` — the kind of change that
wants the coordinated rebuild this tier exists for. A mode-string keyword in the
same comma-separated family as #167's `rlse` is the obvious shape, and #167
already settled the argument about where such a keyword has to live.

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

### 29 · #173 — `clibdscb.h` models DSCB key presence three different ways

OBTAIN SEARCH returns the 96-byte DATA portion of a DSCB; the 44-byte key is the
search *argument*, not part of the answer. `struct dscb1` models that and starts
at `fmtid`. `struct dscb4` carries a leading `key[44]` and does not — so
`d4.dscb4.dstrk` reads 44 bytes past the field and comes back 0. Measured
2026-09-13, JOB00223.

Two consumers already hand-roll around it independently: `test/mvs/tstfprls.c`
finds the format id byte and reads relative to it, and `@@listds.c:192` does the
same thing. That is the whole argument — one header defect, the same workaround
written twice. `dscb4` is a one-line fix and the `@@listds.c` cleanup follows
it. Format-3 extent access is an open design question recorded in the issue, not
answered there.

---

### 30 · #171 — overlapping `strcpy(p, p+1)` in `@@fpnew.c` and `@@dsalc.c`

Benign on the target — the generated MVC walks left to right — and it aborts
every ASAN host run, which is how it was found. Two sites, `memmove()` each.
Cheap enough that it belongs in whatever PR next touches those files rather than
in one of its own.

---

### 31 · #159 — `@@crt0` and `@@crt1` are 319 lines that differ in three

The ecosystem has already settled the question the variants exist to ask: 132
modules link `crt1`, one links `crt0`, **none links `crtm`**. The functional
delta is a single `IDENTIFY EPLOC` for `CTHREAD`, and the two copies have
already drifted once in `@@CTEXIT` — harmless, because `@@CRTGET`'s `PDPEPIL`
restores R1, but it is an edit that landed in one copy and not the other, which
is the argument for merging in one line of diff. The mechanism sits three lines
above it in the same file: `WXTRN` + `ICM` + skip, exactly what `@@STKLEN`
already does. A weak external drives no autocall, so a program without threads
never pulls the thread driver in and one that uses the thread API has the
address anyway.

**Two questions to settle before merging, not after**, and the second has teeth.
`crt0`/`crt1` *create* a C environment for the task; `crtm` *joins* one — it
GETMAINs a stack and calls `@@CRTGET`, which only looks the `CLIBCRT` up for the
current TCB and abends `U0801` if there is none. That is the shape of a program
LINKed into a task whose runtime is already up: an httpd CGI or display module.
Those are linked with **`crt1`**. So either `crtm` is obsolete, or **the CGI
path builds a second environment on a TCB that already has one** — and on a
system where memory is priority #1 that is worth answering before the cleanup,
not as part of it. (The first question is smaller: several `project.toml` files
call `crt1` "the threading runtime" while `@@crt1.asm`'s own header says it is
the copy *without* the CTHREAD IDENTIFY. The comments and the code disagree
about which variant is which.)

Read with `mvslovers/cc370#10` (startup customization via a weak `__premain()`
hook — same mechanism, same file) and `cc370#99` (ld370 and a weak external when
a hard ER for the same name exists; not a blocker, since nothing declares a hard
`EXTRN CTHREAD`, but it is the same mechanism and worth having correct first).

---

### 32 · #169 — the authorized probe recipes pack a bare `.lm`

Five places — `jcl/tstracau.jcl`, `jcl/tstracmx.jcl`, `test/mvs/tstracmx.c`,
`test/mvs/tstracfl.c`, `doc/consumer-notes.md` — pack `NAME=NAME` instead of
`NAME=NAME.iebcopy`. A bare `.lm` carries no PDS directory, so `--pack` has to
default what only the directory holds. `--ac` given again on the pack command
does reach it, which is why those probes really are authorized; the **entry
point is packed as 0** and cannot be supplied at all. They work because `crt0.o`
links first and `@@CRT0` lands at offset 0. A probe whose entry is not `@@CRT0`
would pack at 0 and abend on whatever sits at the start of the module — the same
"no output, just an abend" signature the AC half already cost two deploy cycles
for.

Since `cc370#37` the bare form warns on every run, and a warning that is always
noise here teaches the reader to ignore one that will not be. One line each, and
libc370 already does it correctly in 19 other `test/mvs` files.

---

### 33 · #142 — `jesopen()` should dynalloc the checkpoint and spool

Measured 2026-08-27, and **the issue text understates it both ways.**
`__cpopen()`/`__jsopen()` *already* dynalloc when the argument is not `DD:`, so
the minimum is two literals in `jesopen.c:37,47`. But the data set name is not a
constant: JES2 builds it from `$DSNPRFX` (init parameter, default `SYS1`) plus
the assembled literal `.HASPACE` / `.HASPCKPT`, and that prefix lives in the
HCT, which is unreachable from another address space. So the cheap version works
on a default system and silently opens the wrong thing on a customised one —
which is worse than not doing it. See the measurements in the issue.

Same family as rank 34 below: both are "resolve it without the catalog", and the
OBTAIN-by-volume scan recorded in the issue is the shared route.

---

### 34 · #143 — no volume-addressed SCRATCH/RENAME

`remove()` and `rename()` resolve only through the catalog, so an uncataloged
data set can be neither deleted nor renamed. The OBTAIN-by-volume scan that
#142's discovery work established is the mechanism; this is the API end of it.

---

### 35 · #144, #161, #162, #163, #164 — the socket measurement set

Five probes, one family, and **none of them a libc370 defect**: each asks what
the *emulator* does when a guest pushes on a path nothing has pushed on, and
each is written to need no Hercules change, no diagnostic build and no IPL — so
it runs against any deployed version and survives as a regression test if the
defect is ever fixed.

- **#161 — can GETERROR return another address space's error?** `CerrGen` in
  Hercules' `tcpip.c` is a single global, not per conversation and not per
  address space, so the error a guest reads back need not be its own. Cheapest
  of the five, and the single-address-space variant is simpler still: two
  sockets, two errors that cannot be confused, read back for the first.
- **#144 — does `select()` silently drop sockets from its set?** Same shape,
  filed earlier, and it belongs here rather than alone in a "not yet ranked"
  note, which is where it sat for a fortnight.
- **#162 — slot exhaustion and `talk` aliasing.** `find_slot()` stops at
  `Ccom-1` and assigns anyway, so two `talk` structures alias one slot — the
  structure that carries the buffer pointers and lengths for a transfer in
  flight. The question is not *can it alias* (it plainly can, by inspection) but
  *can a guest get there*. A 26.7-hour soak moved the emulator's descriptor
  count exactly once, and never from the probe: **normal open/close does not
  leak**, so the test has to force the abnormal path — a task that dies between
  the two X'75' instructions of one call.
- **#163 — does `gethostbyname()` block the emulated CPU?** Same class as the
  SEND defect fixed upstream as #863 / PR #864, where a blocking host call froze
  the CPU until the Hercules watchdog killed the emulator by design. Measure it
  from a *different* task: subtask A loops on a fine clock recording its largest
  gap, subtask B looks up a name whose resolver has to time out. The gap is the
  measurement; a fast lookup is the noise floor to compare it against.
- **#164 — the unchecked `aux2` bound in the SELECT path. Read the caution in
  the issue before writing or running this one** — it is the only one here
  expected to be able to *damage* the emulator rather than observe it. `aux2`
  comes from a guest register and indexes `Ccom_han[]` with no check against
  `Ccom`; the result subcodes additionally write the guest's bitmap without
  validating the length. A guest-controlled out-of-bounds read, and on the
  result path an out-of-bounds write, in the emulator's address space. Escalate
  in small documented steps and record where it stops working — the job is to
  say which value does what, not to push until something breaks.

#165 is at rank 26 and not here, because its answer could be a live corruption
in every consumer rather than a property of a path no guest reaches.

---

### 36 · #178 — `fseek()` clears the error indicator, and `rewind()` is only `fseek()`

C splits these and libc370 does not: `fseek` clears the **eof** indicator and
leaves the error indicator standing (C99 7.19.9.2); `rewind` clears **both**
(7.19.9.5), which is the only thing that distinguishes it from
`fseek(f, 0L, SEEK_SET)`. `@@fseek.c` clears both on any seek, and `rewind.c`
is literally that one `fseek()` call, so the two are indistinguishable.

Harmless while nothing consulted the flag. Since #149 it is a trap: a seek is
now a second, undocumented `clearerr()`, and a program that seeks after a
failed write resumes writing into a data set that is still full.

The edit is two lines. The risk is the sweep that has to come first — anything
relying on a seek to clear an error changes behaviour, and no consumer calls
`clearerr()` at all, so nobody would notice the stream had gone quiet.

---

### 37 · #179 — a failed `freopen()` marks the surviving stream as errored

`@@reopen.c` sets `_FILE_FLAG_ERROR` on the **old** stream when `fopen()` of
the new data set fails. Nothing failed on the old one — it was flushed a few
lines earlier and its DCB is fine — and `freopen()` returning `NULL` is already
the report.

Since #149 that flag is a refusal rather than a note, so a failed `freopen()`
now silently kills a healthy stream. Smallest fix is to drop the line. Worth
deciding at the same time whether keeping the old stream open on failure should
survive at all: C99 7.19.5.4 closes it either way, and libc370 does not.

### ~~41 · #229~~ — fixed, PR #233, 2026-09-29

`rclose()` unallocates the DD `ropen()` allocated by name (-1 + `EIO` if that
fails), and `ropen()` does the same when `__aopen()` fails after the
allocation. `tstrfree.c` counts the DSAB chain: red step against the installed
sysroot 6 -> 28 over 22 opens, green step 10/10, both in mvsdev JOB00768.
Not covered: `__fildef()`'s `DISP=NEW` fallback without a normal disposition
(comment on #229). Not released; `edge` moved to the merge (`ad516f1`),
sysroot installed from main.

### ~~41 · #231~~ — fixed, PR #234, 2026-09-29

See "Recently landed". Not released; `edge` moved to the merge (`ed2a9e7`),
sysroot installed from main.

### ~~41 · #232~~ — fixed, PR #237, 2026-09-29

See "Recently landed". Not released; `edge` moved to the merge (`34301b2`),
sysroot installed from main.

### ~~41 · #236~~ — fixed, PR #239, 2026-09-29

See "Recently landed". Not released; `edge` moved to the merge (`15c5041`),
sysroot installed from main. Left open on purpose: a successful record-mode
`fwrite()` answers 1, not `nmemb` - no caller, no issue filed.

### ~~38 · #195 (+ #213)~~ — fixed, PR #214, 2026-09-29

See "Recently landed". Follow-up outside libc370: cc370#484 (`L'a'` is ASCII 97).

---

### 38 · #220, #221 — `%g` output does not follow C99

Filed 2026-09-29 out of #209, both in `@@dblcvt.c`, both output-only. #220:
`%g` counts its precision as fractional digits (`%.6g` of 0.000123456 is
`0.000123`), and `%.2f` of 0.004 prints `0.009`. #221: `%g` in e-style keeps
its trailing zeros, and the exponent is always `E`. Best done together, now
that #222 has given `__dblcvt()` its length parameter.

### 38 · #225, #226 — `__dblcvt` accuracy on HFP, float flags in `__examin()`

Filed 2026-09-29 out of #222, both on `main` before it. #225: the `/10`
scaling loop truncates on HFP, so 1e60 prints `999999999999998046...` and
`%e` of 1e-30 prints `9.99...E-31` - a wrong exponent (JOB00720). Host IEEE
does not show it; the gate has to be MVS. #226: `+`/space goes in front of the
already padded result, `-`/`0` are ignored for floats - derived from the code,
not measured. brexx370 waits on neither (FORMAT uses a plain width, which is
correct). **#225 is visible from REXX** (brexx370, mvsdev JOB00726/00734,
edge 7906cee): `say 1e40*1` gives `9.999999999999983124...E+39`, and TRUNC
(brexx370#72, `%.*e`) of `1e-30*1` gives `...099` for `...100`. Computed
values only, since string arguments no longer go through a double - no
priority from brexx370's side.

### 39 · #181 — `__dsalc()` without `S99NOMNT` waits on the operator

`src/clib/@@dsalc.c` sets `S99NOCNV` only, so an allocation naming a volume
that is not mounted does not fail: SVC 99 raises `IEF238D` and the caller's
task sits there until someone replies. A hang instead of a return code, but
only on a path that names a `VOLSER=` explicitly — hence below the
campaigns and not in Tier 1. Small: one flag plus a probe against an
unmounted volser.

### 40 · #240 — `ssvt_set()`/`ssvt_funcmap()` return no value on success

Filed out of #39's `-Wall` run. Both fall off the end after the key switch; the
generated code leaves `0x100 | key<<4` in R15 (384 for a key-8 caller), never 0.
Latent: the only consumer, ufsd, ignores the result at every call. Small fix
(`return 0;`), but the test needs the `IPK`/`SPKA` asm stubbed or an MVS run.

### 40 · #254 — `@@start` blames a missing SYSIN for a failed stdin open

Found by rexx370 on MVSCE-LAB (JOB01424, libc370 1.0.8): at a REGION just
large enough for the C stack, the `'NULLFILE'` fallback open fails and the
step ends CC 12 with `SYSIN DD not defined` in a dynamic SYSOUT, while the
JCL is fine. The cause is inferred (storage), not measured. The step does
fail, so nothing runs wrong; only the message misleads and lands where nobody
looks. Small: name the failed open, add errno, and consider the U0801-style
WTO + abend of the stack guard. Resumes on 2.x.

### ~~40 · #241, #251~~ — fixed, PR #253, 2026-09-30

See *Recently landed*.

### 42 · #244 — SHA-256, Blowfish and base64 move to crypto370

*Tier 0, steps 2 and 7.* crypto370 1.0.0 is released (2026-09-30), which is
all the cut needs. httpd and mvsMF adopt it in their 2.0 migration PRs, and
the files leave libc370 in phase 1. The issue stays open until they are gone.

The original order here - released *and adopted* before libc370 drops the
files - predates the 1.0.8 hold (D8). With release builds pinned and build
CI held, a consumer does not see the cut until it migrates.

### ~~42 · #243, #248~~ — fixed, PR #255, 2026-09-30

See *Recently landed*.

### 42 · #250 — POSIX surface for 2.0

*Items 1–3: Tier 0, step 4 (phase 1).*

D2's findings: `usleep()` is in `libc.a` with no prototype; POSIX functions
sit in the wrong headers (`sleep` in `<time.h>`, `setenv` in `clibenv.h`, the
sockets in `socket.h`); `stricmp`/`strncmpi` are copies of
`strcasecmp`/`strncasecmp`. Items 1–3 are resolved in phase 1. The missing
functions (`strtok_r`, `getopt`, #51's `inet_addr`) wait for a caller.

### 43 · #246 — function naming (after 2.0)

Three naming styles, reserved `__` names used as public API, and 23 symbols
with several C names. It changes symbols, so every consumer's code changes too:
3.0 material, or additive in 2.x. Not before 2.0.0 has shipped.

---

## Six campaigns instead of forty-one tickets

- **Unchecked allocation** — #61, #80 defect 3, #157 and #158. The convention is
  settled (NULL + guaranteed `errno`, 2026-08-30, recorded in #61) and covers all
  four list builders; what is open is one implementation pass over them, plus
  three consumer follow-ups (`ftpd#118`, `mvsmf#360`, `lua370#15`) that are filed
  and blocked on it. PR #139 made this the
  campaign's remaining libc370-side content: it bounded the walk but deliberately
  did not signal the shortfall.
- **Compiler visibility** — #125, #39, #68 (#104, #70 and #39 step 1 landed). The goal is
  `-Wall` in the SDK build. #125 is the one with a measured failure and is
  independent of the rest; #68 goes last and in its own three-step order, or it
  reddens consumer CI.
- **2.0 restructure** — #245 (umbrella), with #244 feeding it (#243, #248 and
  #140 landed, PR #255). The plan is `doc/design-2.0.md`: a hard cut to a
  standard-shaped header layout (`libc370/`, `mvs/`, `s370/`, `ibm/mvs/`,
  `ibm/jes2/`), with internals out of the sysroot. Phase 0 comes first:
  crypto370, and consumers' build CI pinned to 1.x, so the cut does not redden
  them on `main`. The release concept (D6) is decided and #249 has landed.
  **1.0.8 was released on 2026-09-30 as the last 1.x**, and `main` is at
  `2.0.0-dev`. D1, D2 and D7 were decided the same day, so no decision is
  left open.
  **Phase 0 as of 2026-09-30:**
  - done: CI (#249), v1.0.8, and mvslovers/mbt#121 (the `libc370_ref` input).
    mbt `f88fd03` is in every maintained consumer.
  - release pins: `libc370 = "1.0.8"` in httplua, httprexx, lstring370,
    lua370 and nsf370; ufsd, ftpd, httpd and mvsMF are still on 1.0.6 and
    raise it at their next release; rexx370 (1.0.2) has been asked to move;
    brexx370 pins `edge`.
  - crypto370 v1.0.0 released (#244); httpd and mvsMF adopt it when they
    migrate to 2.0, not before.
  - open: cc370 releases (mvslovers/cc370#523); and
    `libc370_ref: v1.0.8` in each consumer's `build.yml`. That hold goes in
    **right before the cut**, not earlier, or it silences the early warning.
- **Relink round** — #79, #50, #51, #71, #172 and #80 defect 1's `max` parameter.
  Land struct and signature growth in one batch, with a CHANGELOG entry and a
  coordinated rebuild of httpd, mvsMF and ftpd. **Folded into 2.0** (Tier 0,
  step 6).
- **stdio after an abend** — ~~#176 and #149~~ landed together in **v1.0.6**,
  which was always the point: they were one design, not two patches. #168 had
  shipped the caller's escape hatch first (`__fabandon()`, PR #175). What is
  left of the campaign is **#174**, the lock half of the same failure, plus the
  two deviations the work exposed and left standing, **#178** and **#179** —
  both harmless while nothing consulted the error flag, both traps now that it
  is a refusal.
- **Socket measurements** — #144 and #161–#164 at rank 35, #165 at 26, #160 at 27.
  None is a libc370 defect; each asks what the emulator does on a path no guest
  has pushed on, and none needs a Hercules change to run. Order: #165 first,
  because its answer could be a live corruption in every consumer, then #160's
  send-side arm, which decides whether an API change every consumer uses is
  worth making.

---

## Recently landed

Pointers only. The reasoning lives in the closing comments and the PRs.

- **#314 steps 1 and 2** (all merged 2026-10-02, unreleased) - PR #317
  `strtoll`/`strtoull`/`atoll`/`llabs`/`lldiv` + `LLONG_*` (JOB01155,
  64/64); #319 `isblank` (JOB01157); #320 `strtof`/`strtold`, with a guard
  against the S0CC a plain `(float)` of a value above `FLT_MAX` ends in
  (JOB01159); #323 `<inttypes.h>`, `SCN*` only for 16/32/PTR (JOB01167);
  #324 `_Exit`, handlers skipped, teardown kept (JOB01169).
- **#316, #318** (PRs #327-#330, merged 2026-10-03, unreleased) - scanf
  `hh`/`ll`/`j`/`z`/`t`/`L` (JOB01171); `strtol`/`strtoul` to C99 with a
  shared EBCDIC digit table (JOB01173); `strtod` checks the HFP range
  first instead of ending S0CC past ~1e75 (red JOB01177, green JOB01179);
  scanf's digits and floats follow (JOB01181).
- **#321** (PR #322, merged 2026-10-02) - `%lld`/`%jd` printed every
  negative value unsigned; found by the `<inttypes.h>` test. JOB01165
  17/17, 2.0.0 11 of 17 failing.

- **#140, #243, #248** (PR #255, merged 2026-09-30) - the duplicate
  `src/thdmgr/clibthdi.h` is gone; all 729 generated `.s` byte-identical but
  the version stamp. miniz, PDF, `emfile` and `ipc` removed, headers and the
  `src/wip/orig/` sources: none of their symbols was in `libc.a`, no consumer
  used them. `clibsrb.h` kept for a later SRB user, moved to
  `src/wip/mvs/srb.h` (not installed): its FREEMAIN freed from subpool 0
  (`SP+` for `SP=`, silently accepted by the macro), and its two out-of-line
  declarations had no code. The fix is proven by listing, not on MVS.

- **#241, #251** (PR #253, merged 2026-09-30) - `test/host/run.sh` runs all
  26 host tests again; the exclusion list is gone. The `__aclose()` stubs
  return `int`, and new cases pin #182 (`tstfcls`: `fclose()` answers
  `EOF`+`ENOSPC`/`EIO` and still tears down) and #228 (`tstplus`: a turn that
  loses the last block reopens but answers -1 with `ferror()` set).
  `tstjesop` links the real `jesjobfr.c`/`jesjobf1.c`; case (7) pins #126's
  job-list teardown. Each new check goes red against the pre-fix source.

- **v1.0.8** (released 2026-09-30, `c3f4141`): the last 1.x release, and the
  first one `release.yml` published. The notes are the CHANGELOG section plus
  the cc370 commit it was built with (`4c60aa2`). It needs cc370 `f3f7e21` or
  later, unchanged. Contents: #228, #229, #231, #232, #235, #236, #242, #249.
  The sysroot is installed from the tag; `main` is at `2.0.0-dev`. Pinned to
  it the same day: httplua, httprexx, lstring370, lua370 and nsf370. httplua
  and httprexx also moved off deleted httpd/ufsd prereleases, which had kept
  their CI red since July.

- **#249** (PR #252, merged 2026-09-30) - CI. `build.yml`: the library on
  Linux, with cc370 cached per commit and saved right after install; the host
  tests in a second job on **macOS**, because on Linux ELF reads the `@@` of
  the `asm("@@…")` labels as a version separator (measured: 8/21 fail under
  gcc, 7/21 under clang, 0 on macOS). `release.yml`: `VERSION` must match the
  tag, and the notes come from `CHANGELOG.md`. `make test-host` /
  `test/host/run.sh`: 21 of 26 tests at the time; the five broken ones were
  #241 and #251 (all 26 since PR #253).

- **2.0 plan** (PR #247, merged 2026-09-30) - `doc/design-2.0.md`: target
  layout, the phases with their gates, the release concept and a mapping for
  all 153 headers. Umbrella #245; filed with it: #243, #244, #246, #248, #249,
  mvslovers/cc370#523, mvslovers/mbt#121. D6 decided the same day.

- **#39 step 1** (PR #242, merged 2026-09-30) - prototypes for `__tzget`,
  `vwtorf`, `vvprintf`, `vvscanf`, `__fptmp`, `__fpfree`, `rdjfcb`, `initssob`;
  `@@freepd.c`/`malloc.c` call `arraycount`/`arrayfree`/`wto_traceback`. `-Wall`
  implicit declarations 155 in 127 TUs -> 133 in 108, none new; the 23 touched
  TUs assemble byte-identical. #39 stays open for steps 2 and 3. Filed out of
  it: #240, #241. Not released; `edge` moved to the merge (`6b95432`), sysroot
  installed from main.

- **#236** (PR #239, merged 2026-09-29) - `fwrite()` in record mode refuses
  with `EINVAL`, error flag clear, what exceeds LRECL (LRECL-4 spanned, a
  wrapping `size*nmemb` included) or, on V, a bad RDW; `size`/`nmemb` 0 write
  nothing. Contract at `_FILE_FLAG_RECORD` in `clibio.h`. mvsdev `tstfwrec.c`
  JOB00805: red U1234/U0002/U0002 and FB 5 records for 1, green 19/19. Every
  record-mode user in the ecosystem only reads.

- **#235** (PR #238, merged 2026-09-29) - `jesiropn.c`'s `&FUNC` for
  `__alloc_intrdr` had two EN DASHes where `__` belongs. `make build` with the
  as370 of cc370 PR #512: `main` 1 failure (rc 8), the fix 0; no other
  non-ASCII byte in the tree reaches the assembler.

- **#232** (PR #237, merged 2026-09-29) - `rwrite()` refuses with `EINVAL`
  what `@@AWRITE` cannot take (size > LRECL, LRECL-4 spanned; on V an RDW
  that is short, differs from size or has bytes 2-3 set) and maps `__awrite()`
  failures to `ENOSPC`/`EIO`. The RDW stays the caller's, `rfile.h` says so.
  mvsdev `tstrwrv.c` JOB00800: red per group (U0002 x2, U1234 on the
  read-back of an oversized VB record), green 16/16. #236 filed on the way.

- **#231** (PR #234, merged 2026-09-29) - `ropen()` stops the DSN at the
  closing quote of a quoted name without a member (it was copied in, SVC 99
  `X'035C'`). mvsdev `tstrfree.c` (11)/(12): red rc=12 errno=860, green 12/12,
  both JOB00789.

- **#228** (PR #230, merged 2026-09-29) - `freopen()`, the `+`-stream turn
  and `rclose()` report a lost last block like `fclose()` does since #182:
  `rclose()` -1, the turn `ferror()`, `freopen()` errno only. mvsdev
  `tstclspc.c` red JOB00744, green JOB00745 18/18.

- **#195 + #213** (PR #214, merged 2026-09-29) - `wchar_t` is `__WCHAR_TYPE__`
  (int), `ptrdiff_t` is `__PTRDIFF_TYPE__` (long); new types-only `<wchar.h>`
  (`wint_t`, `WEOF`, `WCHAR_*`/`WINT_*`); `mbstowcs`/`wcstombs` rewritten for an
  int `wchar_t`, `mblen`/`mbtowc`/`wctomb` to C99 7.20.7. Consumer sweep
  2026-09-29: no wide-character use on MVS. Host tstwchar 31/31; mvsdev
  JOB00649 18/18, old libc 11 of 18 failed. cc370#484 filed on the way.

- **#211** (PR #212, merged 2026-09-29) - the C99 length modifiers `z`, `t`,
  `j` and `hh` in `__examin()`, the one parser behind the printf family
  (`vsnprint.c`'s `examine()` is `#if 0`). A missing modifier also left its
  argument on the va_list, so later conversions read shifted slots. Host
  tstvsnp 21/21; MVS tstzjt, mvsdev JOB00640: 24/24, pre-fix libc 20 of 24
  failed. `%jd` of a negative value still prints unsigned, as `%lld` does.

- **#189 slice 2** (PR #208, merged 2026-09-27, closes #189) - `r+`/`w+`
  overwrite in place through UPDAT: on F a `'\n'` blank-fills the rest of the
  record, a record never grows or shrinks, and a refusal is `EOPNOTSUPP`. Two
  defects in the inherited UPDAT assembler are fixed: the read after a rewrite
  skipped the rest of the block and a second rewrite landed on the wrong record
  (JOB00559), and a rewrite in a block read to its end was dropped (JOB00561).
  `sizeof(FILE)` stays 192 (`xflags` taken from `mode[]`). mvsdev JOB00576-00580.

- **#189 slice 1** (PR #207, merged 2026-09-27; #189 stays open) - `r+`, `w+`,
  `a+`: read anywhere, write at the end. One DCB, turned round by `__fpswt()` on
  the same DD, never truncating (`_FILE_FLAG_EXTEND`). `a+` counts its size at
  open. A seek to the current position is free and does not flush. **`ftell()`
  on FB text writers now counts in the byte view** (81 per FB 80 record), which
  supersedes #200's 3 6 8. mvsdev JOB00553 15/15. Follow-ups: #206 (O(1)
  backward seek via NOTE/POINT), #204 (append to a member). GitHub closed #189
  twice on keywords (`fix(#189)`, and "does not close" before the number) -
  write "Part of".

- **#198** (PR #205, merged 2026-09-27) — `"a"` is OPEN EXTEND and appends. An
  existing PDS member is refused (`EOPNOTSUPP`), whether it is named in the file
  name or in the JCL, and a new one is created. `@@aopen` returns -45 for EXTEND
  on a member: without that, a member named in the JCL abended SB14-04 at CLOSE
  (JOB00538). Red JOB00536, green JOB00540. Appending to a member: #204.

- **#189 direction check** (PR #203, merged 2026-09-27; #189 stays open) —
  `fgetc`/`fread`/`fputc`/`fwrite` in the wrong direction answer `EOF`/`0` +
  `EBADF`, and set no error indicator. Before, a read on `"w"` served the write
  buffer (`LL2`, JOB00528) and, past it, abended S400. Green JOB00533.

- **#200** (PR #202, merged 2026-09-27) — `ftell()` on a write stream counts from
  the start of the file (a flush no longer resets `filepos`). `fseek()` on a
  stream not open for reading only "seeks" to where it already is: anything else
  is `ESPIPE`, and nothing is read. Before, it re-emitted the stale buffer, and a
  backward seek on `"w"` truncated by reopening. Red JOB00495, green JOB00522.

- **#199** (PR #201, merged 2026-09-27) — an empty line on a text stream is a
  record again: `fputs("a\n\nb\n")` wrote two. The new `__fflnl()` is the newline
  path, and `__fflush()` still writes nothing when nothing is pending. Red
  JOB00495, green JOB00497 on FB and VB. Blank lines now appear in consumers'
  SYSOUT. Not covered: a TSO terminal (TPUT skips length 0), and `printf("\n")`
  (cc370#477).

- **#192** (PR #196, merged 2026-09-27) — the rest of `<stdint.h>`: `INT32_MAX` is
  `long`, the `INTn_C` macros are no longer casts (`INT8_C(200)` is 200, was -56),
  `SIZE_MAX` and `SIG_ATOMIC_MIN/MAX` work in `#if`. `WCHAR_*`/`WINT_*` wait on
  #195, the `wchar_t` disagreement between libc370 (`char`) and cc370 (`int`).

- **#190** (PR #194, merged 2026-09-27) — the other sixteen libgcc helpers:
  float/double ↔ `long long`, `__cmpdi2`, popcount/parity/ffs/clz/ctz. The
  conversions copy the semantics of cc370's inline 32-bit ones (measured,
  JOB00464). Eight names exist only with cc370 at `f3f7e21` or later. Found on
  the way: cc370#477, `printf("\n")` folds to `putchar(10)`, which is ASCII.

- **#188** (PR #193, merged 2026-09-27) — every signed `*_MIN` in `<stdint.h>`
  is negative, of the promoted type and usable in `#if`; `INT64_MIN` no longer
  warns on every use. The rest of the header is #192.

- **#187** (PR #191, merged 2026-09-27) — the six `long long` helpers cc370
  calls (`@@MULDI3`, `@@DIVDI3`, `@@MODDI3`, `@@UDIVDI`, `@@UMODDI`, `@@NEGDI2`)
  and `(u)intptr_t`. Reaches a consumer only after `make install` into the
  sysroot and a relink. **Not fixed by it:** `ll / <const>` (cc370#467) and
  `<<` dropping bit 63 (cc370#468, merged in cc370 but only once the installed
  toolchain is rebuilt). A zero divisor abends S0C9, where brexx370's
  `compat/libgcc64.c` returned all-ones.

- **v1.0.6** (2026-09-13) — #176 and #149, both on what a `FILE` does when a
  write cannot be written, released together so consumers relink once.
  **It is a behaviour change, not only a fix:** a consumer that writes in a
  loop and checks `ferror()` once at the end now gets short returns mid-loop
  instead of on every tenth call, and the `ferror()`/`feof()` macro correction
  is a header change. Relink tickets filed and open in every consumer that
  reads `ferror()` or writes through stdio — ftpd#135, mvsmf#366, ufsd#72,
  httpd#265, lua370#16, httplua#8, rexx370#220. httprexx, ufsd-utils and
  lstring370 need nothing beyond a routine rebuild.

  `rexx370#220` is the one worth reading: `irxinout()` writes SAY output to a
  `stdout` redirected to `DD:SYSTSPRT` for the life of an exec and checks
  neither return value nor `ferror()`. That is the long-lived output stream
  #149's argument against fail-fast assumed existed, and the sweep that decided
  the issue covered ftpd/httpd/ufsd/mvsmf only — all four of which log through
  WTO. It does not change the decision (SYSTSPRT is spool, and the stream dies
  with the job step) but it is where the argument stops reaching.

  `ftpd#135` is the other one that is not routine, and it is a **regression
  v1.0.6 introduces**: ftpd#129 moved an out-of-space STOR from 451 to 552
  because 4xx tells a conforming client to retry into a data set that is still
  too small and already catalogued. That branch keys on the *abend code*
  (`space_abend()`), and #176 removes the abend — so the ESTAE never runs and
  the transfer falls through to the generic 451. One-line split on
  `errno == ENOSPC` at ftpd's end; the lesson for this file is that removing an
  abend can silently retire a consumer's recovery path, and that is worth
  checking for before the next one.

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
