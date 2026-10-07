# Migrating to libc370 2.x

Most of this document is about the step from 1.x to 2.0. What a 2.x release
after 2.0.0 asks of a program is under [Since 2.0.0](#since-200).

libc370 2.0 reorganises the public headers. Function names, behaviour and
the code a program links are, with a short list of exceptions below, what 1.x
had — but almost every program that includes a libc370 header has to change
its `#include` lines. This document says what changed, how to stay on 1.x
while you are not ready, and how to migrate one source file at a time.

2.0 is a **hard cut**: there are no compatibility headers and no deprecated
names. A 1.x include either resolves to its 2.0 header or does not compile.

## Staying on 1.x

**1.0.8** is the last 1.x release. Its tag `v1.0.8` and the branch `1.x`
stay. `main` is 2.0 from the 2.0.0 release on.

- **mbt projects:** declare `[toolchain] libc370 = "1.0.8"` in
  `project.toml` (`release.yml` builds against the tag `v1.0.8`). mbt's build
  CI follows libc370 `main` unless told otherwise — add
  `libc370_ref: v1.0.8` to the `with:` of the mbt build job in
  `.github/workflows/build.yml`.
- **Installed by hand:** `git checkout v1.0.8 && make install`.

## Getting 2.0

- **mbt projects:** `[toolchain] libc370 = "2.0.0"`; drop any
  `libc370_ref` line, the build CI then builds against `main`.
- **Installed by hand:** `git checkout v2.0.0 && make install`. It installs
  into the sysroot of the first `cc370` on `PATH` (`<prefix>/cc370/`) and
  **replaces** the headers there: a 1.x program does not compile against it
  any more.

**Both on one machine.** cc370 finds its sysroot next to its own binary, and
mbt takes the sysroot of the `cc370` on `PATH`. Copy the toolchain's cc370
parts — `bin/cc370` (a copy, not a link), the `bin/*370` tool links,
`lib/cc370/`, `libexec/cc370/` and the sysroot `cc370/` — to a second prefix,
install the other libc370 there with that prefix's `bin/` first on `PATH`,
and build each project with the `PATH` it needs. mbt refuses a sysroot older
than a project's `[toolchain]` pin, so a wrong one is caught.

## What changed

- **Headers** moved into a tree: `mvs/` (the MVS runtime and services),
  `ext/` (portable extras: arrays, 64-bit helpers, y2038 time, …), `ibm/`
  (IBM control blocks, data layout only), `s370/` (architecture), and the
  POSIX headers (`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`, `strings.h`,
  `unistd.h`, …). Headers that mixed purposes were **split**; internal ones
  left the sysroot.
- **Names**: 880 of 1.x's 5481 public names are gone from the public headers
  — almost all of them declared and never built, or internal. The few a
  program can have used are in `sdk/removed.tsv` with their replacement.
- **Crypto** (SHA-256, Blowfish, base64) moved to
  [crypto370](https://github.com/mvslovers/crypto370): add
  `"mvslovers/crypto370" = ">=1.0.0"` to `[dependencies]`. `clibb64.h` is
  `base64.h` there.
- **A few interfaces changed on purpose** — see *Changed meaning* below, and
  the CHANGELOG's 2.0.0 section.
- libc370 itself is now compiled as C99 (`-std=gnu99`).

### The headers you most likely include

The 1.x headers most included across the mvslovers projects, and where they
went (`sdk/headermap.tsv` has all 153):

| 1.x | 2.0 |
|---|---|
| `clibwto.h` | `mvs/wto.h` |
| `clibos.h` | **split:** `mvs/pds.h`, `s370/atomic.h`, `mvs/xmem.h`, `mvs/apf.h`, `mvs/link.h`, `mvs/storage.h`, `mvs/thread.h` |
| `clibary.h` | `ext/array.h` |
| `clibcrt.h`, `clibgrt.h`, `clibppa.h` | `mvs/crt.h` |
| `clibecb.h` | `mvs/ecb.h` |
| `clibtry.h`, `clibstae.h` | `mvs/recovery.h` |
| `racf.h` | `mvs/racf.h` |
| `time64.h` | `ext/time64.h` |
| `clib64.h` | `ext/int64.h` |
| `clibthrd.h` | `mvs/thread.h` |
| `clibenv.h` | `mvs/env.h` |
| `cliblock.h` | `mvs/lock.h` |
| `clibjes2.h` | `mvs/jes2.h` |
| `cliblist.h` | `mvs/dslist.h` |
| `cliblink.h` | `mvs/link.h` |
| `svc99.h` | `mvs/dynalloc.h` |
| `clibio.h` | **split:** `stdio.h`, `mvs/dynalloc.h`, `mvs/pds.h`, `mvs/file.h` |
| `clibstr.h` | **split:** `string.h`, `strings.h`, `ext/strutil.h`, `unistd.h` |
| `mvssupa.h` | **split:** `mvs/clock.h`, `mvs/storage.h`, `mvs/dynalloc.h`, `mvs/idcams.h` |
| `socket.h` | **split:** `sys/socket.h`, `netinet/in.h`, `sys/select.h`, `arpa/inet.h`, `netdb.h`, `mvs/socket.h` |
| `cvt.h`, `ihascvt.h`, `osdcb.h`, … | `ibm/mvs/…` |
| `clibb64.h` | removed — crypto370 `base64.h` |

For a split header, include only the headers that declare the names the file
**uses** — the procedure below finds them.

## The maps

All three are tab-separated, one row per entry, comments start with `#`.
`sdk/headermap.py check` keeps them consistent with the tree and runs in CI.

| file | one row per |
|---|---|
| `sdk/headermap.tsv` | 1.x header: `today  target  users  note` |
| `sdk/names.tsv` | public name in 2.0: `name  header(s)` (generated from `include/`) |
| `sdk/removed.tsv` | 1.x name no public header declares any more: `name  was  now  note` |
| `sdk/splits/*.json` | where each split header's declarations went |

`sdk/names-1.x.tsv` is the frozen list of every public name in `v1.0.8`,
the other side of the comparison that `sdk/removed.tsv` must cover.

A header's `target` in `sdk/headermap.tsv` is one of

- a path under `include/` — the header moved; include it by that name
- the same name — an ISO C header, unchanged
- `split` — its declarations went to several headers; resolve by name
- `internal` — it left the sysroot; a program cannot include it
- `removed` — gone; the note says why

A row's `now` in `sdk/removed.tsv` is one of

- `<hdr> name` — use that name from that header instead. Read the note:
  `bcopy` → `memmove` swaps the first two arguments; `stricmp` →
  `strcasecmp`; the `__getcrt` family is now `__crtget` & co.
- `crypto370 <hdr> name` — the name moved to crypto370
- `internal` — still in libc370, but private: a program that uses it needs a
  decision, not a rewrite
- `-` — gone with nothing in its place (dead code, never implemented)

## The procedure, per source file

1. **Collect** the libc370 headers the file includes, and the identifiers
   it uses (after stripping comments and string literals).
2. **Rewrite each include** by its `sdk/headermap.tsv` row:
   a path target replaces the name; an unchanged ISO header stays; a
   `split`, `internal` or `removed` header is dropped, and step 3 supplies
   what the file needs from it.
3. **Resolve by name.** For every identifier the file uses that
   `sdk/names.tsv` lists, make sure the file includes one of its headers.
   This covers split headers and names the file only saw through another
   header (1.x `socket.h` pulled in every POSIX socket header; 2.0
   `mvs/socket.h` does not). Prefer the header the file already includes;
   otherwise the first one listed.
4. **Replace removed names.** For every identifier in `sdk/removed.tsv`:
   apply `now`. Rows with `internal` or `-` need a person, with the note.
5. **Keep the delimiter convention**: public headers as `<…>`. No program
   includes `"src/…"` — those paths exist only in libc370's own tree.

A program's own header that shares a basename with a libc370 header is not
libc370's: resolve includes against the program's own tree first.

## Changed meaning — a person decides

The maps place names; they cannot say that a name kept its place and changed
what it means. These do; find the uses and decide, do not rewrite blindly.

| name | 1.x | 2.0 | uses met in mvslovers projects (2026-10-01) |
|---|---|---|---|
| `in_addr_t` | `struct in_addr` | `unsigned long`, the address itself | httpd `credentials/src/credtest.c:47` casts to `in_addr_t *` for `inet_aton()`: becomes `struct in_addr *`; brexx370's own `inet_addr()`, now libc370's |
| `inet_aton()` | 0 = address, -1 = none; second argument `in_addr_t *` | 1 = address, 0 = none (BSD); `struct in_addr *` | httpd `credtest.c` ignores the result; brexx370 already expected BSD's |
| `__listpd()` | storage running out returned the records collected so far | NULL with `errno` `ENOMEM` | ftpd `LIST`/`NLST` reads NULL as "No data sets found" -- better moved to `__walkpd()` |
| `JESJOB`, `DSLIST` | 80 / 98 bytes | 96 / 104 bytes, new fields appended; every 1.x offset unchanged | libc370 allocates both: a program only rebuilds |

## Met in the first ports

- **A private copy of a name libc370 now provides** collides with it:
  brexx370 had its own `socklen_t`, `inet_addr()` and `inet_ntoa()` and
  dropped them for libc370's. `inet_ntoa()` returns NULL when the runtime has
  no process anchor; `inet_ntop()` into a buffer of your own does not.
- **A library you publish** that is built against 2.0 cannot be used by a
  program still on 1.x when one of its public headers includes a libc370
  header (ufsd's `libufs.h` includes `ext/time64.h` now). Publish it as a
  **prerelease** until its users are on 2.0: mbt's dependency resolver offers
  a prerelease only to a range that names one (`">=1.4.0-dev"`), so a user
  with `">=1.3.0"` keeps the 1.x build.

## New in 2.0 — optional

Nothing here has to change for a program to build against 2.0; each entry is
something a program may have worked around, and may now drop.

| new | replaces |
|---|---|
| `__walkpd()` in `<mvs/dslist.h>`: a PDS directory member by member through a callback, nothing allocated, the callback may stop | `__listpd()`, one allocation per member — a large PDS exhausts the region |
| `idcams_sysprint()` in `<mvs/idcams.h>`: every SYSPRINT line with its IDC message number, through a callback; `idcams()` is unchanged | reading `idcams()`'s condition code, which cannot tell "not found" (IDC3012I) from "refused" (IDC3203I) |
| `JESJOB.submit_time64` and `JESJOB.sysid` in `<mvs/jes2.h>`: when a job was submitted, and on which system | — |
| `inet_addr()`, `inet_pton()`, `inet_ntop()`, `inet_ntoa()` in `<arpa/inet.h>` | `sscanf("%u.%u.%u.%u")`, own copies |
| `DSLIST.catnm` in `<mvs/dslist.h>`: the catalog `__listds()` found each entry in; NULL when unknown; owned by the list, freed by `__freeds()` | — |
| `fopen(dsn, "w…,unit=…,volser=…")` and `DATASET_UNIT` / `DATASET_VOLSER`: a data set `fopen()` creates goes to the unit and volume named; one that is not mounted is refused at once instead of waiting on `IEF238D` | `__dsalcf()` with `UNIT=`/`VOLSER=`, `__dsfree()`, then `fopen()` by name |

## Verifying a migrated program

- It builds against the 2.0 sysroot with `-Wall -Werror`. An implicit
  declaration means step 3 missed a name.
- Its generated assembler is identical to the 1.x build, TU by TU, except
  - TUs that differ **only** in the numbering of compiler-generated labels
    (`@@Fn`, `@@Ln`): a 1.x header carried a `static` function the 2.0 one
    does not. Not a code change — assemble both sides and compare the objects
    to be sure.
  - TUs that use a changed interface (above, and the CHANGELOG), or where
    step 4 replaced a name on purpose.

  Any other difference is a missed include, not noise. This is libc370's own
  method (`sdk/gate.py`).

## Since 2.0.0

What a 2.x release changed that a program or a build may have to follow. The
CHANGELOG has everything else.

| Release | Change | What to do |
|---|---|---|
| 2.1.0 | Needs cc370 1.1.0 or later. Every public header stops with `#error` otherwise (#315) | Update cc370 |
| 2.1.0 | The compiler helpers (`@@DIVDI3`, ...) moved to cc370's `libcc370rt.a` (#313) | A build that runs the linker itself adds `-lcc370rt` |
| 2.2.0 | `setbuf()` returns `void` (#339) | Stop using its return value |
| 2.4.2 | stdio skips the stream's ENQ while no thread was created through `cthread_create()` (#453) | Nothing for a program without threads or with `cthread_create()` threads. A program that ATTACHes its own subtasks outside `cthread_create()` and shares a `FILE` with them serializes it itself, e.g. with `lock(fp, 0)` around each use |
| 2.4.2 | `fprintf()` writes results of 8192 characters and more in full, and `printf()`/`fprintf()`/`vfprintf()` return a negative value on an output error (#385). Before, `fprintf()` cut at 8192 bytes with a X'00' as the last one and returned the bytes written | Nothing, unless a program relied on the cut, or treats every return value of `printf()` as a count |
| 2.4.1 | A text-mode read keeps a X'00' inside a record; only trailing X'00' bytes are dropped (#454). Before, the record was cut at its first X'00' | Nothing, unless a program relied on the cut: one that reads text records holding X'00' now gets the whole record, the X'00' included |
| 2.4.0 | Needs **cc370 1.4.0** or later; `crt0.o` and `crt1.o` are no longer installed (#159). cc370 1.4 names no startfile, and `@@CRT0` comes out of `libc.a`. `crtm.o` stays | Update cc370. A hand-written `ld370` line that names `crt0.o` or `crt1.o` drops it: `--entry @@CRT0` and a `main` pull the startup from `libc.a`. mbt builds need mbt 2.2.0 or later. Old copies in `<sysroot>/lib`: `make install` and an `apt`/`dnf` upgrade remove them (the 2.3.x packages own them), and Homebrew installs each version into its own directory. cc370's `install.sh` and an unpacked tarball leave them behind, harmless but obsolete: delete them by hand |
| 2.3.0 | `memset()`/`memclr()` inlines declare that they write memory (#425); before, the compiler could undo them | **Rebuild** every program that includes `<string.h>` or `<ext/strutil.h>`; the fix is in the header, so a program built against an older version keeps the old code |
| 2.3.0 | One C startup (#159). `@@CRT0` is a member of `libc.a`; `crt0.o` and `crt1.o` are both copies of it. It IDENTIFYs `CTHREAD` exactly when the program links the thread driver, i.e. uses `cthread_create()`. A program without threads no longer carries `CTHREAD` | Nothing for now. An IDENTIFY of `CTHREAD` in the application is no longer needed; it does no harm (it gets RC 4). Assembler that ATTACHes `EP=CTHREAD` without calling the thread API must reference `CTHREAD` (`EXTRN CTHREAD`) |
| 2.3.0 | `maclib/` no longer ships `PDPMAIN`, `PDP370`, `PDP380`, `PDP390` or `PDPORIG` (#377) | Assembler with `COPY PDPMAIN` came from a pre-cc370 compiler: regenerate it with cc370. Remove stale copies from `<sysroot>/macros` by hand, because `make install` does not delete them |

## For libc370 developers

A change that moves, renames or removes a public header or name updates the
maps in the same PR: `sdk/headermap.tsv` by hand, `python3 sdk/names.py
render`, and a row in `sdk/removed.tsv` for each name that leaves the public
headers. CI refuses the PR otherwise — a name that disappears without a row,
a row whose replacement does not exist, or a stale `sdk/names.tsv`.

## Measured

Against `v1.0.8`, on 2026-10-01: 5481 public names in 1.x, 4610 in 2.0, 880
gone — 603 with nothing in their place (miniz, PDF, emfile, ipc, memmgr, the
module-map API: declared, never built), 256 now internal, 11 to crypto370, 10
replaced by another name. A sweep of the mvslovers projects (httpd, mvsmf,
ftpd, ufsd, ufsd-utils, httplua, httprexx, lua370, lstring370, nsf370,
brexx370, rexx370, crypto370) found no call of a gone name outside the crypto
functions. The first two ports, ufsd and brexx370, needed 29 and 12 files
changed; brexx370's 128 TUs compared identical apart from label numbering and
its own `inet_*` change.
