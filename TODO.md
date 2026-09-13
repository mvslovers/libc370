# libc370 — Open Work, Ranked

**State lives on GitHub, not here.** `gh issue list --repo mvslovers/libc370` is
the source of truth for what is open, closed or newly filed. What this file adds
is the part the tracker cannot hold: the **order**, the reason for it, and which
items wait on a decision rather than on code.

Ordered by **measured impact on running systems** — not by age, and not by effort.
libc370 is the base library of the whole ecosystem, so a defect here is a defect
in httpd, mvsMF, ftpd, ufsd and every other consumer at once; that is what puts
some cheap items high and some expensive ones low.

*Last reconciled against the tracker: **2026-09-13**, 41 issues open, all 41
ranked below.* #149 was fixed and released the same day (PR #180, **v1.0.6**)
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

## Tier 1 — empty again

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

---

## Five campaigns instead of forty-one tickets

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
- **Relink round** — #79, #50, #51, #71, #172 and #80 defect 1's `max` parameter.
  Land struct and signature growth in one batch, with a CHANGELOG entry and a
  coordinated rebuild of httpd, mvsMF and ftpd.
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
