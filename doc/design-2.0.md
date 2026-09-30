# libc370 2.0 — header and source layout

**Status: draft, 2026-09-30.** Umbrella issue: #245. Nothing is implemented.
This document is the plan; each phase gets its own issue when it starts.

## Why

libc370 grew by accretion, and it shows in three measurable ways (all figures
from `main` at `a3e2e67`):

1. **One flat namespace.** `include/` holds 153 headers with no subdirectories,
   and `sdk/mklibc.py` installs all of them into the sysroot side by side:
   17 ISO C headers, 56 `clib*` headers, 44 IBM control-block mappings, 6 `os*`
   mappings, 7 third-party headers and 23 others, internal ones among them
   (`@@memmgr.h`, `clibprti.h`, `clibthdi.h`). A consumer cannot tell the C
   library from MVS extensions, from IBM's data areas, or from libc370's own
   internals. #140, two diverging copies of `clibthdi.h`, is a symptom.
2. **Standard headers export non-standard names.** A program that includes only
   `<stdio.h>`, `<stdlib.h>`, `<string.h>` and `<time.h>` sees `bcopy`, `memclr`,
   `memcpyp`, `strcpyp`, `stricmp`, `strncmpi`, `sleep`, `vvprintf` and
   `vvscanf`. `sleep` is POSIX `<unistd.h>`, and `strcasecmp` is POSIX
   `<strings.h>`. The rest are libc370's own names, or internals.
3. **No seam between the portable core and the operating system.** `malloc()`
   reports through `wtof()` and `wto_traceback()`. stdio opens DCBs directly.
   A second target such as VM/CMS would have to untangle this first.

A fourth finding came out of the inventory. **Four header families declare code
that is not in `libc.a`:** miniz (#243), a PDF generator (`clibpdf.h`, 28
symbols), `emfile.h` (22) and `ipc.h` (17). miniz and PDF have no
implementation anywhere. `emfile` and `ipc` have sources in `src/wip/orig/`,
which is not built (#248). No consumer includes any of them.

## Principles

- **A standard header contains exactly its standard.** An ISO C99 header
  carries ISO names plus the POSIX additions POSIX assigns to that same header
  (`strdup` in `<string.h>`, `localtime_r` in `<time.h>`). A POSIX header
  carries POSIX names. Nothing else, and no feature-test macros (D2).
- **No header under `libc370/` or `mvs/` has the name of an ISO or POSIX
  header.** A quoted `#include "string.h"` inside `include/libc370/` would find
  the neighbour before the standard one (D1).
- **The top directory says who owns the content:**
  - `libc370/` — our API, portable (it would work on CMS too)
  - `mvs/` — our API, MVS-bound
  - `ibm/<component>/` — IBM's data layout, transcribed as C structs, no functions
  - `s370/` — the architecture and linkage shared by every S/370 target
- **Internals are not installed.** They live in `src/internal/`.
- **Names are plain words.** No `clib` prefix and no abbreviations a reader has
  to decode (`svc99.h` → `mvs/dynalloc.h`). Under `ibm/`, a header is named
  after its IBM mapping macro in lower case (`IHAACEE` → `ibm/mvs/ihaacee.h`),
  so the IBM manual leads straight to the header.
- **2.0 is a hard cut.** No compatibility headers and no deprecation period:
  the old names are gone in 2.0.0. Consumers migrate once, driven by a script
  generated from the mapping table below.
- **Function names do not change in 2.0.** Moving a header is mechanical and
  provably changes no code. Renaming a function changes symbols in every
  consumer. That is a separate project (#246).

## Target layout

```
include/                        installed into the sysroot
  assert.h ... wchar.h          ISO C99; to add: stdbool.h, inttypes.h, iso646.h
  strings.h unistd.h            POSIX subset, only for functions that exist (D2)
  sys/socket.h netinet/in.h arpa/inet.h
  libc370/                      portable extensions: array.h, time64.h, version.h, strutil.h, int64.h
  mvs/                          MVS API: wto.h, dynalloc.h, jes2.h, racf.h, smf.h, vsam.h, thread.h, recovery.h, ...
  s370/                         savearea.h, ccw.h
  ibm/mvs/                      IBM data areas: cvt.h, ihaacee.h, ikjtcb.h, iefjfcbn.h, dcbd.h, ...
  ibm/jes2/                     JES2 data areas: jct.h, jqe.h, hct.h, ...
src/
  internal/                     private headers, never installed
  stdio/ string/ stdlib/ time/ ctype/ math/ ...    portable core      (phase 3)
  s370/                         architecture
  mvs/                          the operating-system layer              (phase 4)
```

VM/CMS is **not** designed here. The deliverable is the seam. A later `cms/`,
`ibm/cms/` and `src/cms/` sit beside their MVS counterparts, and `s370/` is
already shared.

## What 2.0 is, and what it is not

**2.0.0 is the interface cut.** It carries phases 1 and 2 (new header paths,
internals out of the sysroot, dead headers gone, crypto out) and the **relink
round**: the struct and signature growth that was already waiting for a
coordinated rebuild of the consumers (#79, #50, #51, #71, #172, and #80 defect
1's `max` parameter). A major version is the one moment consumers expect to
change code, so they migrate once instead of twice. Phases 3 and 4 reorganise
sources and draw the OS seam. They change nothing visible and can land in 2.x
without breaking anyone.

**2.0 comes first (decided 2026-09-30, D8).** 1.0.8 is the last 1.x release and
the consumers are pinned to it, so no fix that lands on `main` reaches a
running system before 2.0.0 ships. By the ranking rule of `TODO.md`, measured
impact on running systems, that puts the 2.0 critical path ahead of every
other item. Header moves and parallel fix PRs would also collide on the same
`#include` lines. Fixes resume on 2.x afterwards, with normal releases.

## Phases

| # | what | gate |
|---|---|---|
| 0 | libc370 CI (#249); crypto370 released (#244; httpd and mvsMF adopt it in their 2.0 migration); **1.0.8 released**, the last 1.x; consumers' **build** CI held on `v1.0.8`, which needs a `libc370_ref` input in mbt's `build.yml` (mvslovers/mbt#121; release CI already pins) | every consumer green against 1.0.8 |
| 1 | on the integration branch `2.0` (#256): move and rename headers per the table; split the five mixed headers; drop dead ones. `install` copies subdirectories and clears the sysroot's `include/` first, since the first moved header needs it. Absorbs #39 step 2 (missing `#include`s) and #68 step 1 (the `clibsa.h` inline), since every `#include` line is touched anyway, and #250 items 1–3 | **byte-identical assembler for every TU** against the commit before the move, the method PR #242 used; `sdk/gate.py` checks it in CI, with the archive's symbols and the warnings |
| 2 | internals to `src/internal/` | the migration script resolves every consumer include |
| — | **the relink round**: #79, #50, #51, #71, #172, #80 defect 1 | per change, tests; CHANGELOG entry for each layout or signature change |
| — | **release 2.0.0**; one migration PR per consumer | each consumer green against 2.0.0 |
| 3 | sources by area; basenames stay unique (all objects share one directory, `mklibc.py:161`) | byte-identical assembler |
| 4 | the OS seam: core calls a narrow internal interface, not MVS services | per change, tests |

Before phase 1, cheap preparation that makes the move safer: restore the five
host tests CI cannot run (#241, #251), remove the live duplicate header (#140),
and drop the dead headers now (#243, #248). Nothing uses them, so they need not
wait for the cut.

Order within phase 0 matters. Consumers' build CI floats on libc370 `main`
(`doc/ci-and-pinning.md`), so the cut must not land on `main` before they are
pinned. Otherwise httpd, mvsMF and ftpd go red the same day.

## Release concept (decided 2026-09-30, D6)

**Today:** a release is a tag and a GitHub release with notes, made by hand.
v1.0.0 to v1.0.7 exist, with no assets, and the notes point at `CHANGELOG.md`
(Keep a Changelog). libc370 has no CI. Consumers' release CI checks out the
pinned tag (`[toolchain] libc370`) and builds libc370 from source. Their build
CI always takes libc370 `main`. The installed version is readable: the build
stamp in `libc.a` (`LIBC370 <version> (<rev>)`), which
`mbttoolchain.py --check` compares with the pin.

**Decided:**

- **SemVer for a C runtime.**
  - MAJOR: a header, symbol or signature removed or changed, or a struct layout
    a consumer can see changes.
  - MINOR: additions and behaviour changes a consumer can observe.
  - PATCH: fixes with no change a caller can see.

  v1.0.7's notes warn "This is not a drop-in relink"; under this rule it would
  have been a minor.
- **CI now, before 2.0** (#249): `build.yml` on PRs and `main`, `release.yml`
  on tags. The release notes come from the tag's `CHANGELOG.md` section.
- **cc370 gets releases** (mvslovers/cc370#523). Desirable before 2.0.0, **not
  blocking** (D8): without them, 2.0.0 names its minimum cc370 by commit, as
  1.0.7 and 1.0.8 did. A `libc.a` fits only certain compilers: v1.0.7 needs
  cc370 at `f3f7e21` or later. Until cc370 has versions, every libc370 release
  names the minimum cc370 commit and the one it was built with. The **sysroot
  tarball** as a release asset follows once cc370 releases exist, because
  before that it would be reproducible only as far as an unnamed compiler.
- **1.0.8 is the last 1.x release.** No maintenance branch after 2.0: the
  consumers migrate directly. **Emergency exit (D8):** a serious defect found
  before 2.0.0 ships gets a **1.0.9**, built from the tag `v1.0.8` with only
  that fix. It is a single release, not a branch that lives on.
- **Cadence:** a release whenever something consumer-relevant lands; `edge` in
  between, as now.
- **No version file in the sysroot.** The build stamp already answers it.

## Documentation (decided 2026-09-30, D7)

2.x gets public documentation on Read the Docs, set up the way brexx370 does it
(`.readthedocs.yaml` → Sphinx, `sphinx_rtd_theme`). It has two parts:

- **Guides, written by hand**, for what no header can explain: program
  start-up, stdio on data sets (record mode, `+` streams), recovery, the MVS
  layer, and the **migration guide 1.x → 2.0**.
- **The API reference, generated from header comments.** Candidates are
  Doxygen + Breathe, or Hawkmoth (a Sphinx extension that reads C comments
  through libclang, without Doxygen). The choice follows a **spike on three
  real headers**, because it is untested whether libclang copes with `asm("@@X")`
  labels and `#pragma pack`. The comment convention is fixed in phase 1, since
  every public header is touched then anyway. The newer comments already
  follow `name() - summary`, which is how a kernel-doc comment starts.

**Two directories, two audiences:** `docs/` is the public documentation that
Read the Docs builds. `doc/` stays for internal development notes (this
design, measurements, the CI analysis), and is never published.

## Decisions

| | question | state |
|---|---|---|
| D1 | namespace for portable extensions | decided: `libc370/`, and no basename of an ISO/POSIX header under `libc370/` or `mvs/` |
| D2 | POSIX scope | decided: declare only what exists, with POSIX semantics; no feature-test macros; findings in #250 |
| D3 | third-party code | decided: miniz remove (#243); SHA-256/Blowfish/base64 to crypto370 (#244); PDF, emfile, ipc remove, after assessing the emfile/ipc code in `src/wip/orig/` (#248) |
| D4 | compatibility | decided: hard cut, 2.0, no shims |
| D5 | where the plan lives | decided: this document plus the umbrella issue |
| D6 | release concept | decided, see above (#249, cc370#523, mbt#121) |
| D7 | documentation | decided: hand-written guides + generated reference, tool after a spike; `docs/` public, `doc/` internal |
| D8 | scheduling | decided: the 2.0 critical path comes first; the relink round ships in 2.0; emergency 1.0.9 from `v1.0.8`; cc370 releases desirable, not blocking |

## Appendix — every header

Rendered from `sdk/headermap.tsv` by `python3 sdk/headermap.py render`; edit
the TSV, not this table. `headermap.py check` (run in CI) fails when the two
drift, or when `include/` holds a header no row accounts for. Generated from
the inventory. Every row is re-checked in phase 1, and the
`ibm/` names in particular against the macro libraries: `sysmac/` vendors
`SYS1.MACLIB` only, so macros from `SYS1.AMODGEN` (`IHAACEE`, `IHAASVT`, …)
cannot be confirmed from the tree. **users** = how many of the 11 consumer
repos include the header (httpd, mvsmf, ftpd, ufsd, httplua, httprexx, lua370,
rexx370, lstring370 and nsf370 at `origin/main`, brexx370 at `origin/master`,
fetched 2026-09-30).

<!-- headermap:begin -->
Summary:

| target | headers |
|---|---|
| ISO C (name unchanged) | 17 |
| `mvs/` | 36 |
| `ibm/mvs/` | 43 |
| `ibm/jes2/` | 13 |
| `libc370/` | 5 |
| `s370/` | 2 |
| internal | 11 |
| split across several | 5 |
| removed or moved out | 21 |
| **total** | **153** |

| today | 2.0 | users | note |
|---|---|---|---|
| `assert.h` | `assert.h` | 1 | ISO C, name unchanged; non-standard names move out |
| `ctype.h` | `ctype.h` | 10 | ISO C, name unchanged; non-standard names move out |
| `errno.h` | `errno.h` | 8 | ISO C, name unchanged; non-standard names move out |
| `float.h` | `float.h` | 2 | ISO C, name unchanged; non-standard names move out |
| `haspcmb.h` | `ibm/jes2/cmb.h` |  | JES2 data area ($CMB) |
| `hasphct.h` | `ibm/jes2/hct.h` |  | JES2 data area ($HCT) |
| `haspiot.h` | `ibm/jes2/iot.h` |  | JES2 data area ($IOT) |
| `haspjct.h` | `ibm/jes2/jct.h` |  | JES2 data area ($JCT) |
| `haspjoe.h` | `ibm/jes2/joe.h` |  | JES2 data area ($JOE) |
| `haspjot.h` | `ibm/jes2/jot.h` |  | JES2 data area ($JOT) |
| `haspjqe.h` | `ibm/jes2/jqe.h` | 1 | JES2 data area ($JQE) |
| `hasppddb.h` | `ibm/jes2/pddb.h` | 1 | JES2 data area ($PDDB) |
| `hasppso.h` | `ibm/jes2/pso.h` |  | JES2 data area ($PSO) |
| `haspsjb.h` | `ibm/jes2/sjb.h` |  | JES2 data area ($SJB) |
| `haspsvt.h` | `ibm/jes2/svt.h` |  | JES2 data area ($SVT) |
| `hasptab.h` | `ibm/jes2/tab.h` |  | JES2 data area ($TAB) |
| `hasptgm.h` | `ibm/jes2/tgm.h` |  | JES2 data area ($TGM) |
| `cvt.h` | `ibm/mvs/cvt.h` | 2 | IBM data area |
| `osdcb.h` | `ibm/mvs/dcbd.h` | 4 | IBM data area |
| `osdecb.h` | `ibm/mvs/ihadecb.h` |  | IBM data area; IHADECB (checked: its 4 fields are all in IHADECB) |
| `racheck.h` | `ibm/mvs/racheck.h` |  | RACHECK parameter list; no mapping macro, the RACHECK macro builds it |
| `racinit.h` | `ibm/mvs/racinit.h` |  | RACINIT parameter list; no mapping macro, the RACINIT macro builds it |
| `safp.h` | `ibm/mvs/ichsafp.h` |  | IBM data area |
| `safv.h` | `ibm/mvs/ichsafv.h` |  | IBM data area |
| `ieebasea.h` | `ibm/mvs/ieebasea.h` |  | IBM data area |
| `ieecdcm.h` | `ibm/mvs/ieecdcm.h` |  | IBM data area; IEECDCM (checked: 66 of 68 fields) |
| `ieecucm.h` | `ibm/mvs/ieecucm.h` |  | IBM data area |
| `ieezb806.h` | `ibm/mvs/ieezb806.h` |  | IBM data area |
| `iefjesct.h` | `ibm/mvs/iefjesct.h` |  | IBM data area |
| `osjfcb.h` | `ibm/mvs/iefjfcbn.h` | 2 | IBM data area |
| `iefjssib.h` | `ibm/mvs/iefjssib.h` | 1 | IBM data area |
| `iefsscs.h` | `ibm/mvs/iefsscs.h` |  | IBM data area; IEFSSCS (checked: 21 of 21 constants) |
| `iefssobh.h` | `ibm/mvs/iefssobh.h` | 1 | IBM data area |
| `iefssso.h` | `ibm/mvs/iefssso.h` |  | IBM data area; IEFSSSO (checked: 25 of 25 constants) |
| `ieftiot.h` | `ibm/mvs/ieftiot1.h` | 1 | IBM data area |
| `ieftxtft.h` | `ibm/mvs/ieftxtft.h` |  | IBM data area; IEFTXTFT (checked: 31 of 31 constants) |
| `iecvucb.h` | `ibm/mvs/iefucbob.h` |  | IBM data area; IEFUCBOB (checked: 17 of 24 fields) |
| `iefvkeys.h` | `ibm/mvs/iefvkeys.h` |  | IBM data area; IEFVKEYS (checked: 142 of 142 constants) |
| `rb99.h` | `ibm/mvs/iefzb4d0.h` |  | IBM data area |
| `txt99.h` | `ibm/mvs/iefzb4d2.h` |  | IBM data area |
| `iezbits.h` | `ibm/mvs/iezbits.h` |  | IBM data area |
| `osdeb.h` | `ibm/mvs/iezdeb.h` |  | IBM data area |
| `osiob.h` | `ibm/mvs/ieziob.h` |  | IBM data area; IEZIOB (checked: its 35 fields are all in IEZIOB) |
| `iezjscb.h` | `ibm/mvs/iezjscb.h` |  | IBM data area |
| `acee.h` | `ibm/mvs/ihaacee.h` | 3 | IBM data area |
| `ihaasvt.h` | `ibm/mvs/ihaasvt.h` | 2 | IBM data area |
| `cde.h` | `ibm/mvs/ihacde.h` | 1 | IBM data area |
| `ihadsab.h` | `ibm/mvs/ihadsab.h` |  | IBM data area |
| `ihadva.h` | `ibm/mvs/ihadva.h` |  | IBM data area |
| `ihalpde.h` | `ibm/mvs/ihalpde.h` |  | IBM data area |
| `iharb.h` | `ibm/mvs/iharb.h` |  | IBM data area |
| `ihascvt.h` | `ibm/mvs/ihascvt.h` | 1 | IBM data area |
| `clibsdwa.h` | `ibm/mvs/ihasdwa.h` | 1 | IBM data area |
| `ihasrb.h` | `ibm/mvs/ihasrb.h` |  | IBM data area |
| `ihaxtlst.h` | `ibm/mvs/ihaxtlst.h` |  | IBM data area |
| `ikjcppl.h` | `ibm/mvs/ikjcppl.h` | 1 | IBM data area |
| `ikject.h` | `ibm/mvs/ikject.h` |  | IBM data area |
| `ikjpscb.h` | `ibm/mvs/ikjpscb.h` |  | IBM data area |
| `ikjtcb.h` | `ibm/mvs/ikjtcb.h` |  | IBM data area |
| `ikjupt.h` | `ibm/mvs/ikjupt.h` |  | IBM data area |
| `@@75.h` | — |  | byte-identical copy of __75.h, no includer |
| `@@memmgr.h` | — |  | PDPCLIB memmgr, never built: USE_MEMMGR undefined, no implementation; its branches in malloc/free/realloc dropped |
| `__75.h` | internal |  | dyn75 parameter list (PL75, __75()); the copy in use, @@75.h dropped |
| `clibjpa.h` | internal |  | @@JPA anchor mapping |
| `clibjs.h` | internal |  | JES spool internals, included by clibjes2.h |
| `clibprtf.h` | internal |  | printf engine |
| `clibprti.h` | internal |  | printf engine |
| `clibres.h` | internal |  | resident-function dispatch (RES/RESFUNC); no includer, kept for a later review |
| `clibsock.h` | `libc370/socket.h` | 1 | socket bookkeeping; httpd walks grt->grtsock as CLIBSOCK, so it stays public (#264 A); internal once libc370 closes sockets itself |
| `clibspl.h` | internal |  | MVCL inline helpers (spl_*); no includer, kept as a Metal C candidate; guard lacks its #define |
| `clibsvc.h` | internal |  | @@SVC work area |
| `clibthdi.h` | `mvs/thread.h` | 2 | thread manager; httpd and ftpd use its API, so public, merged into mvs/thread.h (#264 B); absorbs #140 |
| `clibwsa.h` | internal |  | work-save-area internals |
| `enqpl.h` | internal |  | ENQ parameter list |
| `get3.h` | — |  | GET3/SET3, no includer; modmap.h carries its own GET3 |
| `modmap.h` | internal |  | load module map, used by __loadhi() |
| `clibary.h` | `libc370/array.h` | 7 | dynamic arrays; portable |
| `clib64.h` | `libc370/int64.h` | 2 | 64-bit helpers; review against cc370 long long |
| `time64.h` | `libc370/time64.h` | 6 | y2038 time; portable |
| `clibver.h` | `libc370/version.h` | 3 | libc370_version() |
| `limits.h` | `limits.h` | 6 | ISO C, name unchanged; non-standard names move out |
| `locale.h` | `locale.h` | 2 | ISO C, name unchanged; non-standard names move out |
| `math.h` | `math.h` | 2 | ISO C, name unchanged; non-standard names move out |
| `clibauth.h` | `mvs/apf.h` | 1 | __autask/__austep |
| `clibcib.h` | `mvs/console.h` | 4 | MODIFY/STOP (CIB) |
| `clibcrt.h` | `mvs/crt.h` | 6 | task anchor; 6 consumers read it, review |
| `clibgrt.h` | `mvs/crt.h` | 5 | process anchor; 5 consumers, review |
| `clibppa.h` | `mvs/crt.h` | 8 | program anchor, __ppahrv(); 8 consumers |
| `trkcalc.h` | `mvs/dasd.h` |  |  |
| `clibdsab.h` | `mvs/dd.h` | 1 | DD lookup |
| `clibtiot.h` | `mvs/dd.h` | 1 | DD lookup |
| `clibdscb.h` | `mvs/dscb.h` | 3 | DSCB read + structs |
| `cliblist.h` | `mvs/dslist.h` | 4 | catalog, PDS directory, DASD volume lists |
| `svc99.h` | `mvs/dynalloc.h` | 5 | SVC 99 |
| `clibecb.h` | `mvs/ecb.h` | 4 |  |
| `clibenq.h` | `mvs/enq.h` | 2 |  |
| `clibenv.h` | `mvs/env.h` | 5 | environment from a data set |
| `clibispf.h` | `mvs/ispf.h` |  |  |
| `clibjes2.h` | `mvs/jes2.h` | 3 |  |
| `clibcp.h` | `mvs/jes2ckpt.h` | 1 | JES2 checkpoint access |
| `cliblink.h` | `mvs/link.h` | 4 |  |
| `cliblock.h` | `mvs/lock.h` | 4 |  |
| `clibmtt.h` | `mvs/mtt.h` | 3 | master trace table |
| `clibmutx.h` | `mvs/mutex.h` |  |  |
| `osio.h` | `mvs/osio.h` | 1 | BSAM/QSAM via DCB |
| `racf.h` | `mvs/racf.h` | 6 |  |
| `clibstae.h` | `mvs/recovery.h` | 4 | ESTAE |
| `clibtry.h` | `mvs/recovery.h` | 5 | try() |
| `rfile.h` | `mvs/rfile.h` |  | record I/O |
| `clibsmf.h` | `mvs/smf.h` | 2 |  |
| `clibssct.h` | `mvs/subsys.h` | 1 | SSCT/SSVT/SSIB |
| `clibssib.h` | `mvs/subsys.h` | 1 | SSCT/SSVT/SSIB |
| `clibssvt.h` | `mvs/subsys.h` | 1 | SSCT/SSVT/SSIB |
| `clibthrd.h` | `mvs/thread.h` | 6 |  |
| `clibtmr.h` | `mvs/timer.h` |  |  |
| `clibtso.h` | `mvs/tso.h` |  |  |
| `clibvsam.h` | `mvs/vsam.h` | 1 |  |
| `clibwto.h` | `mvs/wto.h` | 9 |  |
| `clibccw.h` | `s370/ccw.h` |  | channel command words, architecture |
| `clibsa.h` | `s370/savearea.h` |  | S/370 linkage, shared with CMS |
| `setjmp.h` | `setjmp.h` | 3 | ISO C, name unchanged; non-standard names move out |
| `signal.h` | `signal.h` | 2 | ISO C, name unchanged; non-standard names move out |
| `clibio.h` | split | 6 | standard part -> stdio.h; record-mode API -> mvs/rfile.h or mvs/stdio.h; __fp* -> internal |
| `clibos.h` | split | 9 | 34 functions of mixed purpose (BLDL, LOAD, ...); split by topic into mvs/ |
| `clibstr.h` | split | 2 | ISO part -> string.h; strcasecmp/strncasecmp -> strings.h; stricmp/strcpyp/... -> libc370/strutil.h (#250) |
| `mvssupa.h` | split | 4 | public API -> mvs/bsam.h; __aopen & co. internal. 4 consumers include it today |
| `socket.h` | split | 3 | POSIX: sys/socket.h, netinet/in.h, arpa/inet.h (POSIX review, D2) |
| `stdarg.h` | `stdarg.h` | 7 | ISO C, name unchanged; non-standard names move out |
| `stddef.h` | `stddef.h` | 9 | ISO C, name unchanged; non-standard names move out |
| `stdint.h` | `stdint.h` | 3 | ISO C, name unchanged; non-standard names move out |
| `stdio.h` | `stdio.h` | 10 | ISO C, name unchanged; non-standard names move out |
| `stdlib.h` | `stdlib.h` | 11 | ISO C, name unchanged; non-standard names move out |
| `string.h` | `string.h` | 11 | ISO C, name unchanged; non-standard names move out |
| `time.h` | `time.h` | 8 | ISO C, name unchanged; non-standard names move out |
| `wchar.h` | `wchar.h` | 1 | ISO C, name unchanged; non-standard names move out |
| `blowfish.h` | — | 1 | moves to crypto370 (#244) |
| `clib.h` | — | 1 | umbrella include, one user (lua370); drop |
| `clibb64.h` | — | 2 | moves to crypto370 (#244) |
| `clibmz.h` | — |  | removed: miniz, no implementation in libc.a, no user (#243) |
| `clibmzi.h` | — |  | removed: miniz, no implementation in libc.a, no user (#243) |
| `clibpdf.h` | — |  | removed: 28 declared symbols, none in libc.a, no user (PDFGEN) |
| `clibpdfi.h` | — |  | removed: 28 declared symbols, none in libc.a, no user (PDFGEN) |
| `clibsrb.h` | — |  | moved out to `src/wip/mvs/srb.h`, not installed: no user; its FREEMAIN fixed and the 2 declared symbols without code dropped (#248). Becomes `mvs/srb.h` once something schedules an SRB, including `ibm/mvs/ihasrb.h` |
| `emfile.h` | — |  | removed: 22 declared symbols, none in libc.a, no user |
| `emfilei.h` | — |  | removed: 22 declared symbols, none in libc.a, no user |
| `ipc.h` | — |  | removed: 17 declared symbols, none in libc.a, no user |
| `ipci.h` | — |  | removed: 17 declared symbols, none in libc.a, no user |
| `miniz.h` | — |  | removed: miniz, no implementation in libc.a, no user (#243) |
| `miniz_common.h` | — |  | removed: miniz, no implementation in libc.a, no user (#243) |
| `miniz_tdef.h` | — |  | removed: miniz, no implementation in libc.a, no user (#243) |
| `miniz_tinfl.h` | — |  | removed: miniz, no implementation in libc.a, no user (#243) |
| `miniz_zip.h` | — |  | removed: miniz, no implementation in libc.a, no user (#243) |
| `sha256.h` | — | 1 | moves to crypto370 (#244) |
<!-- headermap:end -->
