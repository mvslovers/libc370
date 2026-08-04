# Issue #4 - jesprint() cannot read the active SYSLOG from spool

Analysis of `src/jes/jesprint.c` for mvslovers/libc370#4 (blocks mvslovers/mvsmf#145).

**Result: this is not a `jesprint()` parser bug.** Measured on the live system,
the block chain `jesprint()` is told to follow for the open SYSLOG data set does
not lead to SYSLOG data at all - and a validated scan of the whole spool volume
finds no SYSLOG *data* block on it (only that job's JCT and its two IOTs). The instrument is `test/mvs/tstjeslg.c` (+
`jcl/tstjeslg.jcl`); the raw output is in
[`jes-syslog-issue4-measurements.txt`](jes-syslog-issue4-measurements.txt).

---

## 1. Why the issue was stuck: four silent exits, all `rc = 0`

`jesprint()` sets `rc = 0` at `jesprint.c:101`, before the block loop. Every way
the loop can end early leaves that 0 in place, so the caller sees
*"success, nothing printed"* and cannot tell which of these happened:

```c
/* jesprint.c:105-111 */
for (block = (PRBLOCK*)buf, mttr = jesdd->mttr; mttr; mttr = block->next) {
    if (spool_read(js, mttr, buf, hct->_BUFSIZE)) break;   /* (1) I/O failed   */
    if (job->jobkey != block->jobkey)             break;   /* (2) wrong job    */
    if (jesdd->dsid != block->dsid)               break;   /* (3) wrong dsid   */
    ...                                                    /* (4) parses to 0  */
```

The probe reports each of these instead of breaking silently. That is the whole
measurement: **branch (2), on the very first block.**

---

## 2. What was measured

System: MVS 3.8j (MVS/CE, node MVSC) under Hercules, JES2, 2026-08-04.
`HCT: bufsize=3664 maxjobs=1024 numjoes=3000 numtgv=3330 numda=2`.

### 2.1 SYSLOG (the broken case)

```
JOB SYSLOG   STC00254 owner=SYSTEM   type=50 f1=01 f2=81
    jobkey=E2FC8F85 iotmttr=00002803 spinmttr=00002805
  DD JES00101 dsid=101 mttr=0001E001 records=0 lrecl=130 recfm=C0 class=A
     blk 0    mttr=0001E001 M=00 TT=480 R=1 spool_read rc=0
       hdr next=D1C3E340 jobkey=0EA80000 (job E2FC8F85 MISMATCH) dsid=58102 (dd 101 MISMATCH)
       +000 D1C3E3400EA80000E2F6F91500000000 |JCT  y  S69     |
       +030 000040400000000000273004E2E3C340 |            STC |
       +040 40F1F4F1E9E3C9D4C5D9404040404040 | 141ZTIMER      |
     STOP: jobkey MISMATCH  [jesprint.c:110]
```

`spool_read()` **succeeds** - the MTTR is a valid spool address (M=0, TT=480,
R=1). What it reads is a **`JCT` record belonging to a different job**
(ZTIMER, STC00141). `jesprint()` refuses it, correctly.

### 2.2 Controls: the code path is fine

Same walk over two control jobs (`jcl/tstjeslg.jcl` S2/S3):

* **HTTPD STC00348** - an *active* STC: all 7 DDs match on jobkey and dsid and
  parse (`4`, `244`, `32`, ... records). Its spin data sets (dsid 102/103) have
  `records=0` in the PDDB too and still read correctly.
* **MBTDEPL** - 31 finished batch jobs, all DDs read correctly.

Across 264 DD walks: 234 ended with `next=0`; the 5 `jobkey MISMATCH` stops are
all **active** data sets, where the last written block's `next` already points
at the next allocated - not yet written - track. That is the normal end of an
open data set, not a defect.

**Therefore: "the job is still executing" and "`records=0`" are ruled out as
the cause.** They apply to HTTPD too, and HTTPD works.

### 2.3 The SYSLOG PDDB is the odd one out

From the raw IOT dump (`PARM='SYSLOG,IOT'`):

```
    IOT #0      mttr=00002803 flag1=08 pddbp=1620   PDDB dskey=1..6, all mttr=00000000
    SPIN IOT #0 mttr=00002805 flag1=18 pddbp=1100 sjb=00BCD908
      PDDB#0 +908 dskey=101 mttr=0001E001 recct=0 lrecl=130 recfm=C0 class=A f1=05 f2=88
```

* the dsid-101 PDDB sits in an IOT flagged `IOT1SPIN|IOT1ALOC` (0x18) - the
  live *allocation* IOT of the currently open log data set (`IOTSJB != 0`)
* `PDBFLAG1 = 0x05` = `PDB1SPIN | PDB1PSO` - "data set may be accessed by
  **PSO**"
* `PDBFLAG2 = 0x88` = **`PDB2TCEL`** (data set is TRAKCELL'ed) `| PDB2JFMS`

`PDB2TCEL` is set on **no other PDDB** in the sample - HTTPD's and MBTDEPL's are
all `f2=08`. So SYSLOG's data set is addressed differently from every data set
`jesprint()` reads successfully today.

### 2.4 The decisive one: SYSLOG has no data block on this volume

`PARM='SYSLOG,SCAN'` brute-forces the whole spool volume and looks for the job
key at every fullword offset in the first 64 bytes of each block - data blocks
carry it at +4, `JCT`/`IOT` control records at +8:

```
  SCAN of the spool for jobkey=E2FC8F85 ...
    HIT mttr=00002801 TT=40 R=1 key@+8  +0=D1C3E340 |JCT |     <- its JCT
    HIT mttr=00002803 TT=40 R=3 key@+8  +0=C9D6E340 |IOT |     <- its IOT
    HIT mttr=00002805 TT=40 R=5 key@+8  +0=C9D6E340 |IOT |     <- its SPIN IOT
  SCAN done: 83100 block(s) read up to TT=16649, 3 block(s) carry jobkey E2FC8F85
```

83,100 blocks - the entire `SYS1.HASPACE` extent, 5 records/track over 16,620
tracks - and SYSLOG owns exactly **three**: its JCT and its two IOTs.
**Not one data block.** There is no MTTR to fix and no decoding of `PDBMTTR`
that would find the data, because on this volume the data does not exist.

**Positive control for the scan** (`PARM='HTTPD,SCAN'`, same run) - without one,
a zero result and a broken matcher look identical:

```
  SCAN of the spool for jobkey=E3032DAD ...
    HIT mttr=0001CC01 TT=460 R=1 key@+8  |JCT |
    HIT mttr=0001CC02 TT=460 R=2 key@+8  |IOT |
    HIT mttr=0001CC04 TT=460 R=4 key@+4  dsid=3   records=32
    HIT mttr=0001CC05 TT=460 R=5 key@+4  dsid=1   records=4
    HIT mttr=0001CD02 TT=461 R=2 key@+4  dsid=2   records=49
    ...
  SCAN done: 83100 block(s) read up to TT=16649, 61 block(s) carry jobkey E3032DAD
```

The scan finds the control job's control records **and** its data blocks. The
SYSLOG zero is real.

> An earlier run read only R=1..4 per track and reported 66,480 blocks; R=5 does
> occur (`0001CC05`, `00002805`), so that run missed a fifth of the volume. The
> numbers above are from the corrected R=1..8 scan.

---

## 3. What that leaves

**Scope of the measurement.** This system has **one** SYSLOG data set (dsid
101), currently open, with no `WRITELOG` since IPL. Issue #4 describes dsids
**101-105 after WRITELOG/WRITELOG H** - i.e. *spun* generations, which were not
measured here and may behave differently: HTTPD's spun data sets (dsid 102/103,
`records=0` in the PDDB) **do** read correctly in the control above. "Only the
open generation is unreadable" is therefore still open, and it would change the
fix.

Candidates, in the order the evidence supports them:

| | Hypothesis | How to settle it |
|---|---|---|
| **B** | The open log data set's records are **not in spool blocks reachable this way**. `PDB1PSO` is set - JES2 intends it to be read through PSO / the SSI - and `PDB2TCEL` (TRAKCELL) is set on this PDDB and on no other in the sample. The checkpointed `PDBMTTR` is then not a data-block MTTR at all: it points at TT=480 R=1, which holds a ZTIMER **JCT** whose own `JCTIOT` is `0001E003` - i.e. into another job's *control record* area, not a recycled data area. | Read it via PSO / the SYSOUT-writer interface. libc370 already has the pieces: `jesxwrtr()` / `jesxdone()` (`clibjes2.h:165-168`) and `hasppso.h`. |
| **C** | The hardcopy log is **not being spooled** on this system right now, so the open data set genuinely has no blocks yet. | One `WRITELOG`, then re-run the probe: if blocks with the SYSLOG job key appear, this was it. |
| **A** | The log data lives on the **second spool volume**, invisible to libc370: `HCT._NUMDA = 2`, while `jesopen()` opens only `DD:HASPACE1` (`jesopen.c:46`), everything uses `jes->js[0]` (`jesprint.c:75`), and `__jsrd4()` builds MBBCCHHR from TT/R while **discarding the M byte** (`@@jsrd4.c:34-45`). | Does a second HASPACE data set exist? The evidence is *weak*: every MTTR seen in 264 walks and in all IOT dumps has `M=00`, SYSLOG's included - nothing observed encodes a second volume. |

A is last on purpose: `NUMDA=2` makes D7 a real latent bug, but nothing in the
data points at it as the cause here.

**What is settled:** no change to `jesprint()`'s record parser can fix #4;
`records=0` and "the job is still executing" are red herrings; the failure is
that the address in the checkpointed PDDB does not lead to SYSLOG data.

---

## 4. Defects found while reading the code

Independent of the SYSLOG question. D1 is why this issue took a probe to
diagnose at all; D3/D4 are memory-safety bugs.

### D1 - Silent failure exits (`jesprint.c:107,110,111`) *[diagnosability]*

Three distinct failures all `break` with `rc = 0`. An MVS I/O error is swallowed
and reported as success - the "Optimistic Path" pattern CLAUDE.md forbids.
Callers (mvsmf, httpd) cannot tell "empty data set" from "I/O error" from
"foreign block". Fix: distinct rc or a diagnostic out-parameter.

### D2 - Unbounded, unvalidated chain follow (`jesprint.c:105`) *[robustness]*

`mttr = block->next` is taken from the buffer with no iteration cap, no
self-loop check and no cycle detection. The SYSLOG case shows exactly how a
foreign block gets into that buffer; only the jobkey check stopped the walk.

### D3 - Over-read in the record loop (`jesprint.c:116`) *[memory safety]*

```c
for (p = &buf[10], line = (PRLINE*)p; line->len != EOB && p < eob; line = (PRLINE*)p)
```

`line->len` is dereferenced **before** `p < eob` is evaluated. `p` advances by
up to `3 + 255` bytes per iteration from a position only required to be
`< eob`, so `p` can reach `buf + bufsize + 254` and the next `line->len` reads
past the `calloc()`ed buffer. The conditions are in the wrong order; a real fix
also checks `p + 3 + line->len <= buf + bufsize`.

### D4 - Spanned-record heap overflow (`jesprint.c:150`) *[memory safety]*

```c
memcpy(&prbuf[linelen], p, sp->len2);
linelen += sp->len2;
if (sp->flags & FLAG_LAST || linelen > blksize) { ... }
```

`prbuf` is sized from the 2-byte total length carried in the `FLAG_FIRST` part
only, and is only ever grown (`:134`). `MIDDLE`/`LAST` parts are copied in
unchecked and the overflow test happens **after** the copy. A block whose first
record is a `MIDDLE`/`LAST` part - e.g. the first block after a failed
`spool_read` - writes past `prbuf`.

### D5 - `FLAG_FIRST` length accounting (`jesprint.c:129-159`) *[unverified]*

For the first part of a spanned record `p` is advanced past the 2-byte total
length and the optional carriage control byte, but both the copy length and the
final `p += sp->len2` use `sp->len2` unadjusted. If `len2` counts from
`sp->data`, the copy runs 2-3 bytes long *and* `p` lands short. Needs a real
spanned record to settle - a host test case (§5.2).

### D6 - Return value is the callback's (`jesprint.c:154,167,176`)

`rc = esc_print(...)` inside the loop, and `rc` is what `jesprint()` returns; on
success the caller gets whatever the last `prt()` returned, mixed into the same
int as 503 / 404 / 0. There is no way to report "printed N lines" - which is
why "zero lines" is invisible.

### D7 - One spool volume; the MTTR M byte is discarded (`jesopen.c:46`, `jesprint.c:75`, `@@jsrd4.c:34-45`)

See §3. No longer theoretical: this system has `NUMDA=2`.

### D8 - PDDB scan bound (`jesjob.c:81,178,198`) *[minor]*

`pddbend` is the end of the *buffer*, not `iotbuf + iot->IOTPDDBP`; the scan
relies on finding a `PDBDSKEY == 0` terminator.

---

## 5. Test cases

### 5.1 On-target probe - `test/mvs/tstjeslg.c` + `jcl/tstjeslg.jcl` *(done)*

Read-only, needs no library change, uses only public API
(`checkpoint_open`/`spool_open`/`jesjob`/`spool_read`). Three modes:

```
PARM='SYSLOG'        walk every DD's block chain, report each decision + hex dump
PARM='SYSLOG,IOT'    + raw IOT chain, PDDB fields and track-group map
PARM='SYSLOG,SCAN'   + brute-force scan of the spool volume for the job's blocks
                     (~83k reads; ALWAYS pair it with a positive control)
```

Build and run:

```
cc370 -Iinclude test/mvs/tstjeslg.c -flinker-output=iebcopy -o TSTJESLG
ld370 --pack TSTJESLG.iebcopy -o probe -xmit --dsn <LOADLIB>   # then TSO RECEIVE
```

Notes for re-running: needs `REGION=4M` (the checkpoint buffer is 140 KB here;
the 512 K default fails), `TIME=1440` for `,SCAN` (~83 k reads), and the
`HASPCKPT`/`HASPACE1` DDs from `jcl/tstjeslg.jcl`. It deliberately does **not**
call `jesopen()`: that wraps everything in `try()`/ESTAE and reports failures
with `wtof()`, i.e. to the console - which on this system means the SYSLOG we
cannot read.

### 5.2 Host tests for the record parser - **requires a small refactor**

Not needed for #4 any more (the parser is exonerated), but D3/D4 are real and
untested. `jesprint.c` cannot be compiled on the host: file-scope `__asm__`
(`&FUNC SETC`, `TRLINE`, `PRTXLATE` at `:179,246-247`) plus `spool_read()` is a
BDAM `READ`/`CHECK` - the same wall documented in `test/host/tstcmtt.c`, which
had to hand-mirror the code under test and says itself that a mirror is not
durable.

The durable route is to extract the block/record parser into its own asm-free
translation unit - buffer in, line callback out, no I/O, no `EX`/`TR` - so a
host test links and executes *the real code*:

```c
int __jesprb(const void *blk, unsigned blklen, JESPRST *st,
             int (*emit)(const char *line, unsigned len, void *arg), void *arg);
```

`jesprint()` keeps the I/O loop and passes an `emit` that does the `EX`/`TR`
translate and calls `prt()` - the translate **must stay on the caller's side of
the callback**, or the new TU inherits the assembler and nothing is gained. The
spanned-record state (`prbuf`, `blksize`, `linelen`) moves into `JESPRST`
because it legitimately spans blocks.

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

## 6. Recommended order

1. **D1 + D2** - make the failure modes visible and the walk bounded. Worth
   doing on its own; mvsmf#145 needs it regardless of the root cause.
2. **Settle A vs B** (§3): does a second spool volume exist on this system?
   That single answer decides whether D7 is the root cause or a separate bug.
3. **The actual fix** - multi-volume spool support (D7) and/or reading open
   data sets through PSO. Both are their own issues, not a `jesprint()` patch.
4. **D3 + D4** memory-safety fixes, with the §5.2 extraction + host tests.
