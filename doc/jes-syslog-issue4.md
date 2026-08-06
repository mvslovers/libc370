# Issue #4 - jesprint() cannot read the active SYSLOG from spool

Analysis of `src/jes/jesprint.c` for mvslovers/libc370#4 (feeds
mvslovers/mvsmf#186, the deep-history follow-up to the delivered
mvslovers/mvsmf#145).

> **Line references below are the code as it stood on 2026-08-04.** PR #40
> (#25) moved the block and record walk into `src/jes/jesprb.c`, so the defects
> this document numbers D3, D4 and D5 now live there — unchanged, which is the
> point of that PR. The findings stand; only the addresses moved:
>
> | here | today |
> |------|-------|
> | D3 over-read, `jesprint.c:116` | `jesprb.c:33` (the loop bounds test) |
> | D4 spanned overflow, `jesprint.c:150` | `jesprb.c:66` (the `memcpy`) |
> | D5 `FLAG_FIRST` accounting, `jesprint.c:129-159` | `jesprb.c:38-80` |
> | buffer + `eob`, `jesprint.c:78,83` | `jesprint.c:64` (`calloc`), `jesprb.c:31` (`eob`) |
> | D6 return value, `jesprint.c:154,167,176` | fixed in #26: `rc` is 0/404/503, the callback's rc is `st->prtrc` |
>
> D1, D2 and D7 refer to code still in `jesprint.c`, shifted by the extraction.

## Result

**`jesprint()` reads the SYSLOG correctly. It was never broken.** Proven on the
live system: after `WRITELOG Y` - which queues the log data set to a *held*
SYSOUT class - the probe walks the chain and finds all 32 records of real log
text.

What happens on that system is a **configuration** effect:

1. The system log is queued to SYSOUT class **A**, which the JES2 deck defines
   as `PRINT,SYSOUT,NOHOLD,TRKCEL` - a printer drains and **purges** it within
   seconds.
2. The **checkpointed** IOT/PDDB that `jesjob()` reads still lists the purged
   data set, with its old `PDBMTTR` and even a non-zero `recct`.
3. Those tracks have long been reallocated to other jobs, so the first block
   `jesprint()` reads is a **foreign `JCT`**. It refuses it - correctly - and
   returns 0 lines with `rc = 0`, which is indistinguishable from "empty".

The bug that remains in libc370 is therefore **not** the parse: it is that all
of this is invisible. Three distinct failures return "success, nothing
printed" (D1).

Instrument: `test/mvs/tstjeslg.c` + `jcl/tstjeslg.jcl`. Raw output:
[`jes-syslog-issue4-measurements.txt`](jes-syslog-issue4-measurements.txt).

---

## 1. Why the issue was unanalysable

`jesprint()` sets `rc = 0` at `jesprint.c:101`, before the block loop. Every way
the loop can end early leaves that 0 in place:

```c
/* jesprint.c:105-111 */
for (block = (PRBLOCK*)buf, mttr = jesdd->mttr; mttr; mttr = block->next) {
    if (spool_read(js, mttr, buf, hct->_BUFSIZE)) break;   /* (1) I/O failed   */
    if (job->jobkey != block->jobkey)             break;   /* (2) wrong job    */
    if (jesdd->dsid != block->dsid)               break;   /* (3) wrong dsid   */
    ...                                                    /* (4) parses to 0  */
```

The probe reports each decision instead of breaking silently. Answer:
**branch (2), on the very first block.**

---

## 2. The measurements

System: MVS 3.8j (MVS/CE, node MVSC) under Hercules, JES2, 2026-08-04.
`HCT: bufsize=3664 maxjobs=1024 numjoes=3000 numtgv=3330 numda=2`.

### 2.1 What `jesprint()` is pointed at

```
JOB SYSLOG   STC00254  jobkey=E2FC8F85 iotmttr=00002803 spinmttr=00002805
  DD dsid=101 mttr=0001E001 records=0 lrecl=130 recfm=C0 class=A
     blk 0  mttr=0001E001 M=00 TT=480 R=1 spool_read rc=0
       hdr next=D1C3E340 jobkey=0EA80000 (job E2FC8F85 MISMATCH) dsid=58102 (dd 101 MISMATCH)
       +000 D1C3E3400EA80000E2F6F91500000000 |JCT  y  S69     |
       +040 40F1F4F1E9E3C9D4C5D9404040404040 | 141ZTIMER      |
     STOP: jobkey MISMATCH  [jesprint.c:110]
```

The read **succeeds**; the block is a `JCT` of a different job (ZTIMER
STC00141). Later, after other jobs had run, the same address held the JCT of
`TSTJESLG JOB00315` - the track is simply in general circulation.

### 2.2 Controls: the code path is fine

* **HTTPD STC00348**, an *active* STC: all 7 DDs match and parse, including its
  spin data sets (dsid 102/103) which also show `records=0`.
* **MBTDEPL**: 31 finished batch jobs, all DDs read correctly.

264 DD walks: 234 end at `next=0`; the 5 `jobkey MISMATCH` stops are all
*active* data sets, where the last written block's `next` already points at an
allocated-but-unwritten track - the normal end of an open data set.
→ **"job is still executing" and "`records=0`" are ruled out.**

### 2.3 The spool scan, with a positive control

`PARM='<job>,SCAN'` reads every block on the volume and matches the job key at
every fullword offset in the first 64 bytes (data blocks carry it at +4,
`JCT`/`IOT` control records at +8):

```
SYSLOG jobkey E2FC8F85: 83100 blocks read, 3-4 hits - its JCT and its IOTs,
                        and NOT ONE data block
HTTPD  jobkey E3032DAD: 83100 blocks read, 61 hits - JCT, IOTs AND data blocks
                        for dsid 1,2,3,4,5,102,103
```

The control finds what it should, so the SYSLOG zero is a real absence.
(An earlier run read only R=1..4 per track and missed a fifth of the volume;
`&BUFSIZE=3664` in the JES2 deck is annotated "5 PER TRACK ON 3350".)

### 2.4 TRKCELL is *not* the cause

The SYSLOG PDDB carries `PDBFLAG2 = 0x88` = `PDB2TCEL | PDB2JFMS` -
TRAKCELL'ed - and no other PDDB in the first sample had it. The JES2 deck
explains why: `TRKCEL` is an attribute of **output classes** A, B, L and M, and
SYSLOG is queued to class A, while everything libc370 read successfully was
class Y or H.

Controlled experiment (job `TCELB`): the same five lines written twice in one
job, differing only in output class - class A (`TRKCEL`, kept with `HOLD=YES`
so a printer cannot purge it) and class Y (no `TRKCEL`).

```
  DD SYSUT2   dsid=104 mttr=00041302 records=5 recfm=90 class=A   (f2=88, PDB2TCEL)
     blk 0  hdr next=00041304 jobkey=E314FBA6 (MATCH) dsid=104 (MATCH)
       +000 00041304E314FBA60068400050E3D9D2 |    T  w    &TRK|
       +010 C3C5D3D340D7D9D6C2C540D3C9D5C540 |CELL PROBE LINE |
     WALK: 2 block(s) accepted, 5 record(s)   STOP: chain end (next=0)
```

A track-celled class-A data set reads **perfectly**. TRKCELL exonerated.

### 2.5 The configuration, and the proof

From `SYS1.PARMLIB(JES2PM00)`:

```
&BUFSIZE=3664   BUFFER SIZE (5 PER TRACK ON 3350)
&NUMDA=2        MAX NUMBER OF SPOOL VOLUMES        <- a maximum, not a count
&SPOOL=SPOOL1   SPOOL VOLUME SERIAL                <- exactly ONE spool volume
$$A  PRINT,SYSOUT,NOHOLD,TRKCEL     STANDARD OUTPUT CLASS
$$L  PRINT,SYSOUT,NOHOLD,TRKCEL     SYSLOG
$$Y  PRINT,SYSOUT,HOLD              HELD STC/TSU
```

and `D C,HC` showed the hardcopy log going to **device 015**, not to the system
log - so the open SYSLOG data set had nothing written to it at all.

Experiment (everything restored afterwards): `VARY SYSLOG,HARDCPY` → generate
messages → `WRITELOG Y` (queue the log to the **held** class Y) → probe:

```
  DD JES00102 dsid=102 mttr=00042901 records=32 lrecl=130 recfm=C0 class=Y
     blk 0  hdr next=00042903 jobkey=E2FC8F85 (MATCH) dsid=102 (MATCH)
       +000 00042903E2FC8F850066570057F4F0F0 |    S  e     400|
       +010 F040F1F64BF4F24BF1F8404040404040 |0 16.42.18      |
       +020 4040404040C9C5C5F0F4F3C940C140E2 |     IEE043I A S|
       +030 E8E2E3C5D440D3D6C740C4C1E3C140E2 |YSTEM LOG DATA S|
     WALK: 2 block(s) accepted, 32 record(s)  STOP: chain end (next=0)
     => jesprint() would print 32 line(s) for this DD
```

**`jesprint()` reads the SYSLOG.** In the same run, dsid 101 and 103 - the
generations that went to class **A** - still fail, because they were printed
and purged while the checkpointed PDDB still advertises them (dsid 101 even
shows `records=32`).

The hardcopy setting was returned to device 015 and verified with `D C,HC`.

---

## 3. What this means for #4 and for mvsmf

Reading the hardcopy log through this code path **works**, under two
operational conditions on the target system:

1. the hardcopy log must actually go to the system log
   (`VARY SYSLOG,HARDCPY`), and
2. the log data set must be queued to a **held** SYSOUT class - `WRITELOG H` /
   `WRITELOG Y`, or by giving class L the `HOLD` attribute in `JES2PM00`.
   In a `NOHOLD` class a printer purges it before anything can read it.

`WRITELOG <class>` is an *operator command*, not a `JES2PM00` change - mvsmf can
issue it through the console API it already has, so condition 2 need not become
a deck change on the target system.

**State of the mvsmf side.** mvslovers/mvsmf#145 is closed: it shipped
`GET /zosmf/restconsoles/v1/log` from the Master Trace Table - the recent window,
not a deep archive - and was closed with the note "the active SYSLOG on spool is
not browsable", which this analysis disproves. The deep-history capability is
therefore still unshipped and is tracked in mvslovers/mvsmf#186.

What libc370 should change is not the parser but the reporting: a caller must
be able to tell "this data set is gone / not ours" from "this data set is
empty". mvsmf can then answer 410/404 instead of returning an empty log.

A second, real consequence of reading the checkpointed IOT: **`jesjob()` lists
data sets that no longer exist.** dsid 101 above is a purged data set still
advertised with `records=32`. There is no way to detect that from the
checkpoint alone - the jobkey check in `jesprint()` *is* the detection, which
is another reason it must be reported rather than swallowed.

---

## 4. Defects found while reading the code

### D1 - Silent failure exits (`jesprint.c:107,110,111`) *[the one that matters]* — #21

Three distinct failures all `break` with `rc = 0`: I/O error, foreign block,
wrong dsid. An MVS I/O error is swallowed and reported as success - the
"Optimistic Path" pattern CLAUDE.md forbids - and a purged data set is
indistinguishable from an empty one.

### D2 - Unbounded, unvalidated chain follow (`jesprint.c:105`) — #22

`mttr = block->next` is taken from the buffer with no iteration cap, no
self-loop check and no cycle detection. The SYSLOG case shows exactly how a
foreign block gets into that buffer; only the jobkey check stopped the walk.

### D3 - Over-read in the record loop (`jesprint.c:116`) *[memory safety]* — #23

```c
for (p = &buf[10], line = (PRLINE*)p; line->len != EOB && p < eob; line = (PRLINE*)p)
```

`line->len` is dereferenced **before** `p < eob` is evaluated. `p` advances by
up to `3 + 255` bytes from a position only required to be `< eob`, so it can
reach `buf + bufsize + 254` and the next `line->len` reads past the `calloc()`ed
buffer. The conditions are in the wrong order; a real fix also checks
`p + 3 + line->len <= buf + bufsize`.

### D4 - Spanned-record heap overflow (`jesprint.c:150`) *[memory safety]* — #24

`prbuf` is sized from the 2-byte total length in the `FLAG_FIRST` part only and
is only ever grown (`:134`). `MIDDLE`/`LAST` parts are copied in unchecked and
the overflow test happens **after** the copy. A block whose first record is a
`MIDDLE`/`LAST` part - e.g. the first block after a failed `spool_read` - writes
past `prbuf`.

### D5 - `FLAG_FIRST` length accounting (`jesprint.c:129-159`) *[unverified]* — #29

For the first part of a spanned record `p` is advanced past the 2-byte total
length and the optional carriage control byte, but both the copy length and the
final `p += sp->len2` use `sp->len2` unadjusted. Needs a real spanned record to
settle - a host test case (§5.2).

### D6 - Return value is the callback's (`jesprint.c:154,167,176`) — #26

`rc = esc_print(...)` inside the loop, and `rc` is what `jesprint()` returns; on
success the caller gets whatever the last `prt()` returned, mixed into the same
int as 503 / 404 / 0. There is no way to report "printed N lines".

### D7 - One spool volume; the MTTR M byte is discarded (`jesopen.c:46`, `jesprint.c:75`, `@@jsrd4.c:34-45`) — #27

`jesopen()` opens only `DD:HASPACE1`, everything uses `jes->js[0]`, and
`__jsrd4()` builds MBBCCHHR from TT/R while **discarding the M byte** (the spool
volume index). Latent: this system has `&SPOOL=SPOOL1`, a single volume
(`&NUMDA=2` is the configured *maximum*), and every MTTR observed has `M=00`.
It would bite on a multi-volume spool - silently, by reading the wrong volume.

### D8 - PDDB scan bound (`jesjob.c:81,178,198`) *[minor]* — #28

`pddbend` is the end of the *buffer*, not `iotbuf + iot->IOTPDDBP`; the scan
relies on finding a `PDBDSKEY == 0` terminator.

---

## 5. Test cases

### 5.1 On-target probe - `test/mvs/tstjeslg.c` + `jcl/tstjeslg.jcl` *(done)*

Read-only, no library change, public API only
(`checkpoint_open`/`spool_open`/`jesjob`/`spool_read`). Modes:

```
PARM='SYSLOG'        walk every DD's block chain, report each decision + hex dump
PARM='SYSLOG,IOT'    + raw IOT chain, PDDB fields and track-group map
PARM='SYSLOG,SCAN'   + brute-force scan of the spool volume for the job's blocks
                     (~83k reads; ALWAYS pair it with a positive control)
```

```
cc370 -Iinclude test/mvs/tstjeslg.c -flinker-output=iebcopy -o TSTJESLG
ld370 --pack TSTJESLG.iebcopy -o probe -xmit --dsn <LOADLIB>   # then TSO RECEIVE
```

**Built by hand, deliberately.** libc370 is the cc370 sysroot, not an mbt
project - no `project.toml`, no `[[test]]` declarations, no CI - so nothing
compiles this translation unit automatically, and neither `make` nor `make
clean` touches it. It is a diagnostic instrument kept next to the analysis, not
a regression test: after a change to `clibjes2.h` or the JES structures it must
be rebuilt by hand to find out whether it still compiles.

Needs `REGION=4M` (the checkpoint buffer alone is 140 KB; the 512 K default
fails), `TIME=1440` for `,SCAN`, and the `HASPCKPT`/`HASPACE1` DDs. It
deliberately does **not** call `jesopen()`: that wraps the opens in
`try()`/ESTAE and reports failures with `wtof()`, i.e. to the console - which
on this system means the SYSLOG we cannot read.

**Reproducing the green case:** `VARY SYSLOG,HARDCPY` → a few console commands
→ `WRITELOG Y` → `PARM='SYSLOG'`. Restore with `VARY <dev>,HARDCPY`.

**Reproducing the red case:** `WRITELOG` (no class → class A, `NOHOLD`), wait
for the printer to purge it, then `PARM='SYSLOG'` - the checkpointed PDDB still
advertises the data set and the walk stops on a foreign block.

### 5.2 Host tests for the record parser - **requires a small refactor** — #25

Not needed for #4 (the parser is exonerated), but D3/D4 are real and untested.
`jesprint.c` cannot be compiled on the host: file-scope `__asm__`
(`&FUNC SETC`, `TRLINE`, `PRTXLATE` at `:179,246-247`) plus `spool_read()` is a
BDAM `READ`/`CHECK` - the wall documented in `test/host/tstcmtt.c`, which had to
hand-mirror the code under test and says itself that a mirror is not durable.

The durable route is to extract the block/record parser into its own asm-free
translation unit - buffer in, line callback out, no I/O, no `EX`/`TR`:

```c
int __jesprb(const void *blk, unsigned blklen, JESPRST *st,
             int (*emit)(const char *line, unsigned len, void *arg), void *arg);
```

`jesprint()` keeps the I/O loop and passes an `emit` that does the `EX`/`TR`
translate and calls `prt()` - the translate **must stay on the caller's side of
the callback**, or the new TU inherits the assembler. The spanned-record state
(`prbuf`, `blksize`, `linelen`) moves into `JESPRST` because it legitimately
spans blocks.

Cases, executing production code:

1. plain records, several per block
2. `FLAG_HASCC` - carriage control stripped, not printed
3. spanned `FIRST`/`MIDDLE`/`LAST` within one block (pins **D5**)
4. spanned record continuing **across** two blocks
5. immediate `EOB` as first byte -> 0 records, no over-read
6. zero-filled block -> 0 records, terminates
7. truncated block: last record's length runs past the end (pins **D3**)
8. `MIDDLE`/`LAST` with no preceding `FIRST` (pins **D4**)
9. a real block captured by 5.1 as a byte-for-byte fixture

---

## 6. Runbook: the hardcopy switch used for the proof

Recorded verbatim so the experiment can be repeated - and undone - exactly.

### State found (and restored to)

```
D C,HC ->
    IEE250I 16.39.31 CONSOLE DISPLAY 369
    WTO BUFFERS:    CURR =     0      LIM =  500
    CONSOLE/ALT     COND    AUTH   ID AREA  NBUF ROUTCD
    015/009     H      CMDS    03            ALL      <- hardcopy: DEVICE 015
    010/009     N       ALL    01 Z,A        ALL
    009/010     M       ALL    02            ALL
    015/009     A,T     NONE   03            ALL
```

Device **015**, alternate 009, `AUTH=CMDS`, `ROUTCD=ALL`. The device *type*
behind 015 was not established: `D U,,,015,1` returns only the `IEE450I ... UNIT
STATUS` header through the mvsMF console API - the continuation lines are not
collected, and the `solmsgs` follow-up returns empty. Look it up in the
Hercules device configuration.

### Switch to the system log

```
VARY SYSLOG,HARDCPY          -> IEE349I HARDCOPY CONSOLE      (accepted)
```

Syntax matters on MVS 3.8j - these were **rejected**:

```
VARY (SYSLOG),HARDCPY,ALL    -> IEE309I VARY  UNIDENTIFIABLE KEYWORD
V SYSLOG,HARDCPY,ALL         -> IEE309I V     UNIDENTIFIABLE KEYWORD
V (SYSLOG),HARDCPY,ALL,CMDS  -> IEE309I V     UNIDENTIFIABLE KEYWORD
```

`IEE349I HARDCOPY CONSOLE` reads like a rejection but is informational -
verify with `D C,HC`, which then shows:

```
    CONSOLE/ALT     COND    AUTH   ID AREA  NBUF ROUTCD
    SYSLOG          H      CMDS                  ALL      <- hardcopy: SYSLOG
```

### Produce and spin a log data set

```
D T                          (a few times, to put messages in the log)
WRITELOG Y                   -> IEE043I A SYSTEM LOG DATA SET HAS BEEN QUEUED
                                TO SYSOUT CLASS Y
```

`WRITELOG` **without** an operand queues to class A - `NOHOLD` here, so a
printer purges it before it can be read. That is the red case. The class
operand (`Y`, or `H`) is what makes the data set survive.

### Restore

```
VARY 015,HARDCPY             -> IEE349I HARDCOPY CONSOLE
D C,HC                       -> back to "015/009  H  CMDS  03  ALL"
```

Verified: the display after restoring is byte-identical to the state found.

---

## 7. Recommended order

1. **D1 (+D2)** - #21, #22: report *why* nothing was printed, and bound the
   walk. This is the actual libc370 fix for #4, and what mvslovers/mvsmf#186
   needs to answer "gone"/"empty" correctly. Its verification is the probe's red
   and green case (§5.1), not a host test: the decisions live in the I/O loop,
   which stays in `jesprint.c` even after the #25 extraction.
2. **D3 + D4** - #23, #24, with the §5.2 extraction (#25) and host tests. Not
   far behind step 1: D4's trigger *is* the #4 red case - "the first block after
   a failed `spool_read`" carrying a `MIDDLE`/`LAST` part. Same walk, same
   trigger.
3. **mvslovers/mvsmf#186** - deep history over the spooled generations, with the
   operational requirements from §3 documented in the endpoint.
4. **D6, D7, D8** - #26, #27, #28 as separate, lower-priority items, and #29
   once a real spanned record settles D5.

Independent of all of the above: **#30**, reading spooled SYSOUT through PSO/SSI
instead of the checkpointed IOT. It removes the stale-pointer failure mode
entirely (JES2 hands over a data set that exists *now*, read by DSN through
normal access methods) and makes held output selectable, but it needs APF
authorization, does not see the still-open log data set - PSO selects from the
output queue, where the open generation has no JOE - and so does not remove the
`WRITELOG` step either.
