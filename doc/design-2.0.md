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
that does not exist:** miniz (#243), a PDF generator (`clibpdf.h`, 28 symbols),
`emfile.h` (22) and `ipc.h` (17). None of their `asm()` symbols is in
`libc.a`, and no consumer includes them.

## Principles

- **A standard header contains exactly its standard.** ISO C99 names in ISO
  headers, POSIX names in POSIX headers, nothing else.
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
  libc370/                      portable extensions: array.h, time64.h, version.h, string.h, int64.h
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

Only phases 1 and 2 change what a consumer sees. **2.0.0 is the interface cut:**
new header paths, internals out of the sysroot, dead headers gone, crypto out.
Phases 3 and 4 reorganise sources and draw the OS seam. They change nothing
visible and can land in 2.x without breaking anyone.

## Phases

| # | what | gate |
|---|---|---|
| 0 | crypto370 released and adopted by httpd and mvsMF (#244); consumers' **build** CI pinned to libc370 1.x (release CI already is) | every consumer green against 1.x |
| 1 | move and rename headers per the table; split the five mixed headers; drop dead ones | **byte-identical assembler for every TU** against the commit before the move, the method PR #242 used |
| 2 | internals to `src/internal/`; install copies subdirectories (`mklibc.py:226` globs `include/*.h` today) | the migration script resolves every consumer include |
| — | **release 2.0.0**; one migration PR per consumer | each consumer green against 2.0.0 |
| 3 | sources by area; basenames stay unique (all objects share one directory, `mklibc.py:161`) | byte-identical assembler |
| 4 | the OS seam: core calls a narrow internal interface, not MVS services | per change, tests |

Order within phase 0 matters. Consumers' build CI floats on libc370 `main`
(`doc/ci-and-pinning.md`), so the cut must not land on `main` before they are
pinned. Otherwise httpd, mvsMF and ftpd go red the same day.

## Release concept (proposal, to discuss)

Today a release is a tag and a GitHub release with notes. v1.0.0 to v1.0.7
exist, with no assets. The notes point at `CHANGELOG.md` (Keep a Changelog).
There is no CI (`doc/ci-and-pinning.md`, parked 2026-08-05), and consumers'
release CI checks out the tag and builds libc370 from source. The sysroot
carries no version, so which libc370 is installed has to be inferred.

Proposal for 2.0:

- **SemVer defined for a C runtime.**
  - MAJOR: a header, symbol or signature removed or changed, or a struct layout
    a consumer can see changes.
  - MINOR: additions, and behaviour changes a consumer can observe.
  - PATCH: fixes with no change a caller can see.

  v1.0.7's notes warn "This is not a drop-in relink". Under this rule that release
  would have been a minor.
- **Release artifacts:** a sysroot tarball (`include/`, `lib/libc.a`,
  `crt0/1/m.o`, `macros/`) plus SHA-256, attached to the GitHub release. That
  is the shape `make deps` already uses for every other dependency, so
  consumers download instead of building libc370 themselves.
- **CI in libc370** (`build.yml` on PR and `main`, `release.yml` on tags): step 1
  of `doc/ci-and-pinning.md`. Once #39 step 3 lands, the build also fails on new
  warnings.
- **An installed version stamp:** the install writes the version into the
  sysroot, so `mbttoolchain.py --check` reads it instead of inferring it.
- **`edge`** keeps its meaning: `main` at a measured point, between releases.

## Documentation

2.x gets generated documentation on Read the Docs, set up the way brexx370
does it (`.readthedocs.yaml` → Sphinx, `docs/source/conf.py`,
`docs/requirements.txt`). Open question: is the API reference written by hand
per header, or generated from header comments (Doxygen + Breathe)? Generating
it needs one comment convention across all public headers, and phase 1 is the
moment to set it, since every public header is touched then anyway.

## Decisions

| | question | state |
|---|---|---|
| D1 | namespace for portable extensions: `libc370/` | proposed here; `mvs/` and `ibm/mvs/` are decided |
| D2 | POSIX scope: only headers for functions that exist | agreed in principle, review after the plan |
| D3 | third-party code | miniz: remove (#243). SHA-256/Blowfish/base64: crypto370 (#244). PDF, emfile, ipc: remove (dead, see above) |
| D4 | compatibility | decided: hard cut, 2.0, no shims |
| D5 | where the plan lives | decided: this document plus the umbrella issue |
| D6 | release concept | proposal above, to discuss |
| D7 | API reference: hand-written or generated | open |

## Appendix — every header

Generated from the inventory. Every row is re-checked in phase 1, and the
`ibm/` names in particular against the macro libraries: `sysmac/` vendors
`SYS1.MACLIB` only, so macros from `SYS1.AMODGEN` (`IHAACEE`, `IHAASVT`, …)
cannot be confirmed from the tree. **users** = how many of the 11 consumer
repos include the header (httpd, mvsmf, ftpd, ufsd, httplua, httprexx, lua370,
rexx370, lstring370 and nsf370 at `origin/main`, brexx370 at `origin/master`,
fetched 2026-09-30).

Summary:

| target | headers |
|---|---|
| ISO C (name unchanged) | 17 |
| `mvs/` | 35 |
| `ibm/mvs/` | 43 |
| `ibm/jes2/` | 13 |
| `libc370/` | 4 |
| `s370/` | 2 |
| internal | 16 |
| split across several | 5 |
| removed or moved out | 18 |
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
| `osdecb.h` | `ibm/mvs/decb.h` |  | IBM data area; macro name to verify |
| `racheck.h` | `ibm/mvs/ichrchk.h` |  | IBM data area; macro name to verify |
| `racinit.h` | `ibm/mvs/ichrinit.h` |  | IBM data area; macro name to verify |
| `safp.h` | `ibm/mvs/ichsafp.h` |  | IBM data area |
| `safv.h` | `ibm/mvs/ichsafv.h` |  | IBM data area |
| `ieebasea.h` | `ibm/mvs/ieebasea.h` |  | IBM data area |
| `ieecdcm.h` | `ibm/mvs/ieecdcm.h` |  | IBM data area; macro name to verify |
| `ieecucm.h` | `ibm/mvs/ieecucm.h` |  | IBM data area |
| `ieezb806.h` | `ibm/mvs/ieezb806.h` |  | IBM data area |
| `iefjesct.h` | `ibm/mvs/iefjesct.h` |  | IBM data area |
| `osjfcb.h` | `ibm/mvs/iefjfcbn.h` | 2 | IBM data area |
| `iefjssib.h` | `ibm/mvs/iefjssib.h` | 1 | IBM data area |
| `iefsscs.h` | `ibm/mvs/iefsscs.h` |  | IBM data area; macro name to verify |
| `iefssobh.h` | `ibm/mvs/iefssobh.h` | 1 | IBM data area |
| `iefssso.h` | `ibm/mvs/iefssso.h` |  | IBM data area; macro name to verify |
| `ieftiot.h` | `ibm/mvs/ieftiot1.h` | 1 | IBM data area |
| `ieftxtft.h` | `ibm/mvs/ieftxtft.h` |  | IBM data area; macro name to verify |
| `iecvucb.h` | `ibm/mvs/iefucbob.h` |  | IBM data area; macro name to verify |
| `iefvkeys.h` | `ibm/mvs/iefvkeys.h` |  | IBM data area; macro name to verify |
| `rb99.h` | `ibm/mvs/iefzb4d0.h` |  | IBM data area |
| `txt99.h` | `ibm/mvs/iefzb4d2.h` |  | IBM data area |
| `iezbits.h` | `ibm/mvs/iezbits.h` |  | IBM data area |
| `osdeb.h` | `ibm/mvs/iezdeb.h` |  | IBM data area |
| `osiob.h` | `ibm/mvs/iezdeb/iob.h` |  | IBM data area; macro name to verify |
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
| `@@75.h` | internal |  | dyn75 parameter list |
| `@@memmgr.h` | internal |  | allocator internals |
| `__75.h` | internal |  | duplicate of @@75.h |
| `clibjpa.h` | internal |  | @@JPA anchor mapping |
| `clibjs.h` | internal |  | JES spool internals, included by clibjes2.h |
| `clibprtf.h` | internal |  | printf engine |
| `clibprti.h` | internal |  | printf engine |
| `clibres.h` | internal |  | resident structs; no user, review |
| `clibsock.h` | internal | 1 | socket bookkeeping; httpd includes it, review |
| `clibspl.h` | internal |  | inline helpers; no user, review |
| `clibsvc.h` | internal |  | @@SVC work area |
| `clibthdi.h` | internal | 2 | thread manager internals; absorbs #140. ftpd and httpd include it: review what they use first |
| `clibwsa.h` | internal |  | work-save-area internals |
| `enqpl.h` | internal |  | ENQ parameter list |
| `get3.h` | internal |  | GET3/SET3 macros; no user, review |
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
| `clibstr.h` | split | 2 | ISO part -> string.h; strcasecmp/strncasecmp -> strings.h; stricmp/memclr/strcpyp/... -> libc370/string.h |
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
| `clibsrb.h` | — |  | removed: 2 declared symbols, none in libc.a, no user |
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
