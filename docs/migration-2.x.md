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
| Unreleased | `oscheck()` returns 4 at end of data and 8 for an I/O error instead of the S337/S001 abend code, and 8 (not 0) when a program's own SYNAD routine returned from an I/O error; `osbdcb()`/`osxdcb()` return `NULL` when the exit list cannot be allocated; new `osxopenm()`/`osdopenm()` open read-only with mode `"r"` (#412) | Test `oscheck()` for 4 and 8 where the code compared with the abend codes; a program whose SYNAD routine returns now sees 8 from `oscheck()`; check the DCB from `osbdcb()`/`osxdcb()` for `NULL` also when it passes an exit list |
| Unreleased | `__dsalc()` accepts `DSN=A.B(MEM)` and `FREE=CLOSE`; `__dynal()` accepts `"A.B(MEM)"` and allocates with `FREE=CLOSE`, so its DD is gone after the data set is closed (#408) | A program that opens a `__dynal()` DD a second time after closing it allocates it again first |
| Unreleased | `struct dscb4` (`DSCB4`) is the data portion `__dscbv()` returns: no leading `key[44]`, `devov` overlays `devi`/`devl`, `amcat` overlays `vsind`/`cra`; every field offset moves, `cra` is `unsigned char[2]` instead of `unsigned short`, `sizeof(DSCB4)` 96 and `sizeof(DSCB)` 140 instead of 146 (#173) | Recompile: a program that read `dstrk`, `devtk`, `devov`, `devk` ... through the structure gets the right values without a source change. Code that read `dscb.work[]` at the IBM offsets is unaffected. Code that read `cra` as a halfword reads `cra[0] << 8 | cra[1]`. A format-4 DSCB from `__dscbav()` is at `(DSCB4 *)(dscb.work + 44)` |
| Unreleased | `<math.h>` declares the 22 C99 `long double` functions (`sqrtl()` ...) as other names of the `double` functions, and the `double` functions with their MVS name as an `asm` label (#340) | Nothing, unless the program declares a `<math.h>` function itself and calls it before it includes `<math.h>`: cc370 then warns that two names map to one MVS name. Include `<math.h>` first, or drop the own declaration. A program that defines its own `sqrtl()` and the like must rename it |
| Unreleased | `bind()`, `connect()`, `accept()`, `getsockname()`, `getpeername()` take their address as a transparent union of `struct sockaddr *` and `struct sockaddr_in *` (#417); the POSIX cast and the uncast `&sin` both compile | Nothing for calls. Code that stores one of these functions in a function pointer of the old type must declare the pointer with `__SOCKADDR_ARG` (`__CONST_SOCKADDR_ARG` for `connect()`) |
| Unreleased | `crtm.o` is no longer installed, and `make install` deletes an old copy from `<sysroot>/lib` (#608); a link line that names it fails with `crtm.o: No such file or directory`, and MBT 3 reports `startup = "crtm"` as a configuration error | Drop `crtm.o` from the link line (with MBT, the `startup = "crtm"` key). A C module that a C program calls with `LINK`, `__link()` or `system()` is linked like any program, with `@@CRT0` from `libc.a`, and builds and releases its own environment; for C code that must run on the caller's environment, see the `CCALL` stub ([startup.md](startup.md); not verified on MVS for a LINKed module) |
| Unreleased | `timegm()` in `<ext/time64.h>`: a broken-down UTC time to `time_t`, what `mktime()` does today (#392). A later release makes `mktime()` read local time | Code that calls `mktime()` on a UTC time switches to `timegm()` now; nothing else changes |
| Unreleased | New `__linkx()`, `__linkdsx()` and `__loadx()` in `<mvs/link.h>` return the completion code (and for LOAD the reason code) of a failed LINK or LOAD (#553) | Nothing; use them instead of `__link()`/`__linkds()`/`__load()` where the reason for a failure matters |
| Unreleased | `malloc(0)`, `calloc(0, n)` and `calloc(n, 0)` return `NULL` without setting `ENOMEM` and without the console message (#394) | Nothing, unless a program took `errno == ENOMEM` after `malloc(0)` for a shortage |
| Unreleased | `fseek()` and `fsetpos()` keep the error indicator; only `rewind()` and `clearerr()` clear it (#178) | A program that seeks after a read or write error and expects to go on reading or writing calls `clearerr()` first |
| Unreleased | The declarations of `__enq()`, `__deq()`, `__sosnam()`, `__sopnam()` are removed (#400); none was ever in the library, so a call never linked | Use `ENQ()`/`DEQ()` from `<mvs/enq.h>`, and `getsockname()`/`getpeername()` |
| Unreleased | `PPAFLAG_TIN`, `PPAFLAG_TOUT`, `PPAFLAG_TERR` (`<mvs/crt.h>`) and `PPATIN`, `PPATOUT`, `PPATERR` (`CLIBPPA`) are removed (#72); the library never set them | Drop the test; it always answered no |
| Unreleased | `__try()` and `__tryrc()` are removed (#17); they were an unused older copy of `try()` and `tryrc()` | Call `try()` and `tryrc()` instead; same arguments, an abend is reported the same way |
| Unreleased | `crtopts` and `CRTOPTS_AUTH` are removed from `<mvs/crt.h>`, and `CRTOPTS` from `clibcrt.copy` (#113). The field was never set, so a test of it was always false | Drop the test, and keep the code of its false branch |
| 2.23.0 | `try()` and the functions built on it return -1, not +1, when no recovery routine can be set up; the timer requests return 0 when the timer thread cannot be started (#476) | A caller that treated +1 as "no ESTAE" tests for a negative value |
| 2.23.0 | `tsocmd()` puts the caller's ECT back after the command (#413); `sleep()` returns the seconds left when its timer could not be set, where it returned 0 | Nothing |
| 2.23.0 | `<mvs/wto.h>` declares `wtof()`, `vwtof()`, `wtorf()`, `vwtorf()` and `wtodumpf()` with the `format(printf)` attribute (#68): the compiler checks the arguments against the format | A call whose arguments do not match its format draws a warning, and fails a `-Wall -Werror` build. The common case is a pointer to `%08X`: cast it to `(unsigned)` |
| 2.23.0 | `exp()` reduces its argument and returns `HUGE_VAL`/0 with `ERANGE` out of range; `pow()` squares, and returns `HUGE_VAL` with `ERANGE` for 0 to a negative power and for overflow; `atan2(0,0)` is 0; `log(0)` is `-HUGE_VAL` with `ERANGE` (was `+HUGE_VAL`, `EDOM`); `frexp(0)` stores 0 (#389). These ended in S0CC/S0CF or were wrong for negative arguments | A program that tested `errno == EDOM` after `log(0)` tests `ERANGE`. Workarounds such as `1.0 / exp(-x)` can go |
| 2.23.0 | `__dsalc()`: `DISP=(...,UNCATLG)` and `UNCAT` uncatalog (they cataloged), the JCL form `SPACE=(TRK,(p,s))` is accepted (it returned 1), and `opts` `NULL` means no keywords (#408) | A request that relied on `UNCATLG` cataloging the data set has to say `CATLG` |
| 2.23.0 | `vsopen()` succeeds for a cluster VSAM opens with a warning (R15 4, for example one an abend left open) and converts the DD name to upper case; `vsclose()` returns CLOSE's return code (#411). Before, the warning failed the open with `errno` 4, a lower-case DD name failed with `EVSOPEN`, and `vsclose()` returned 0 | A program that retried or verified the cluster after `vsopen()` failed with 4 no longer needs to |
| 2.23.0 | `fflush(NULL)` flushes all output streams, `system(NULL)` returns 1, `ungetc()` clears the end-of-file indicator, `freopen()` discards a pushed-back character, `perror()` prints `strerror()`'s text for every value (#395) | Nothing; workarounds such as a `clearerr()` before re-reading a pushed-back character can go |
| 2.23.0 | `fclose()` always releases the stream's lock, also one the task held before the call (#174) | Nothing. A caller that holds the lock around `fclose()` and unlocks afterwards gets 8 from that `unlock()`, as for any lock it no longer holds |
| 2.23.0 | `mktime()` leaves `*timeptr` unchanged when it returns -1, and `difftime()` returns a negative value when `time1` is earlier (#392); `*timeptr` was overwritten with 2106-02-07, and the difference wrapped to about 4.29e9 | Code that compared the two times before calling `difftime()` can drop the comparison |
| 2.23.0 | `strerror()` never returns a null pointer: VSAM error numbers 150-154 have texts, other unknown numbers give `"Unknown error"` (#420). `<mvs/dasd.h>` has `DEV3375` beside the misspelled `DEB3375`; `jesirput()` takes `const char card[80]` | A test for `NULL` after `strerror()` can go. Use `DEV3375` in new code |
| 2.23.0 | `try()` frees the SDWA of every abend it recovers (#640); about 500 bytes of the region per abend stayed allocated | Nothing |
| 2.23.0 | A failed `freopen()` leaves the old stream open without its error indicator (#179); the indicator was set and refused every later read and write | Nothing; a `clearerr()` after a failed `freopen()` is no longer needed |
| 2.23.0 | `osxopen()` and `osdopen()` return 8, "not open", for a `NULL` DCB without issuing OPEN (#412); they passed address 0 to OPEN, and the return code could be left unset | Nothing |
| 2.23.0 | `svcmain()` sets the key of `SVCMAIN_KEY` given in the low four bits, `0x08` as well as `0x80` (#634); `0x08` set key 0 | A scaffold that asked for `0x08` and ran in key 0 because of it runs in key 8 now |
| 2.23.0 | `make install` removes only the headers of the previous LIBC/370 install, listed in `<sysroot>/share/libc370/include-files.txt` (#636); it emptied `<sysroot>/include` | Nothing; headers of other toolchains in the sysroot survive an install |
| 2.22.0 | `__uastep()` in an authorized program leaves the STEPLIB's APF authorization alone unless `__austep()` set it (#125); it cleared it every time | Nothing. A program that relied on `__uastep()` to de-authorize a STEPLIB it had not authorized itself has to stop relying on it |
| 2.22.0 | `fwrite()` with `size` 0 returns 0 on a byte stream, where it returned `nmemb`; `fseek()` past the end of a record stream returns -1 with the end-of-file and error indicators, where it returned 0; a successful `fseek()` on a `+` stream always discards an `ungetc()` character (#474). A temporary (`&&`) data set honours `blksize=` and `space=trk(...)`/`cyl(...)` and is allocated on VIO; a SYSOUT data set honours `blksize=` | A program that seeks record by record until `fseek()` fails, or tests `fwrite()` against `nmemb` with a size of 0, sees the C99 results now. Temporary data sets that went to disk because of `space=` go to VIO |
| 2.22.0 | The `scanf()` functions accept field widths, do not count `*` conversions, return `EOF` when the input ends before the first conversion, match the character after `%[...]`, and push back the character that stops an early end of `fscanf()` (#386, #474). Before, a width made the conversion fail, `*` conversions were counted, empty input returned 0 and the character after `]` was skipped | A format written around these defects - a blank after `]`, a count that includes `*` items, a 0 for end of input - has to be checked: test for `EOF` at the end of the input |
| 2.22.0 | `printf()` formats `%a` and `%A` (#423): `0x1p+0` for 1.0, `0x8p-4` for 0.5, with the leading digit of the System/370 fraction (1 to F). Before, the conversion was printed as written | A program that relied on `%a` being printed literally changes. Output compared with an IEEE system differs in the leading digit and exponent, not in the value |
| 2.22.0 | `time()` and `__getclk()` work up to the end of the TOD clock in September 2042 (#390); they abended S0C9 from January 2038 and ran 0.32 seconds ahead. New `__todsec()` in `<mvs/clock.h>` converts a TOD clock value to seconds since 1970 | Nothing |
| 2.21.0 | `setenv()` and `putenv()` compare the name exactly (#475); when no name matched exactly they replaced a variable whose name differed only in case, and renamed it | A program that set a variable in a different case than it was defined in, to replace it, has to use the same case; `getenvi()` finds a name regardless of case |
| 2.21.0 | `bsearch()` calls `compar(key, element)`, as C99 specifies (#388); it called `compar(element, key)` | A comparison function whose two parameters have the same type works as before. One written for the old order, with the table element first and a key of another type second, has to swap its parameters: it now receives the key first |
| 2.21.0 | `setenv()`, `putenv()` and the SYSENV/ENVIRON loading look a variable up by its name without the surrounding blanks (#475): a second `NAME = value` record replaces the first, where it was added beside it and `getenv()` returned the first value | A SYSENV that set a variable twice and relied on the first value has to drop the second record |
| 2.21.0 | `__listc()` returns the IDCAMS return code, for example 4 for a level with no entries (#410); it returned 0. `next_dsab()` and so `__listal()` take an empty DD name as all DDs; `__listal(tcb, "", opt)` listed only the first DD | A caller that took any nonzero `__listc()` result as a failure to read the listing has to tell -1 (no listing) from a positive IDCAMS code |
| 2.21.0 | `strstr(s, "")` returns `s`, as C99 requires (#393); it returned `NULL` | Code that treated an empty search string as "not found" through the `NULL` has to test for the empty string itself |
| 2.21.0 | `ispunct()`, `isgraph()` and `isprint()` are true for `[`, `]` and `^` (X'BA', X'BB', X'B0'), which had no class (#399) | A program that tested for these three characters itself because the functions missed them can drop the test |
| 2.21.0 | `strftime()`: `%x` is the date as `%m/%d/%y`, `%Z` produces nothing, `%U`/`%W` count the week as C99 does, and `maxsize` 0 writes nothing (#391). Before, `%x` printed pointer bits, `%Z` read through a null pointer, `%U`/`%W` were one too small on some days | A program that computed the week itself because `%U`/`%W` were wrong can use them again |
| 2.21.0 | `<float.h>`: `FLT_MAX`, `DBL_MAX`, `LDBL_MAX`, `*_MIN` and `*_EPSILON` are constant expressions of their own type, with unchanged bit patterns (#419); they were members of library objects, partly of type `float`, and `LDBL_MAX` was the `long` 2147483647. `FLT_DIG` is 6, was 7 | Code that used `__DBL_MAX__` and friends because the macros were not constants can use the macros. Recompile to get the corrected types |
| 2.20.0 | A program's sockets are closed when it ends, also by an abend nothing recovers (an ESTAE established at start, one of `estae()`'s ten levels), and when a LINKed program abends under `try()` (#601); they stayed open. Only sockets the program's `socket()`/`accept()` returned count: `bind()`/`connect()` on a socket from elsewhere no longer adopts it | A program that left a socket open on purpose for a later program has to keep running instead |
| 2.19.0 | `super_do()`, `super_key_do()`, `clib_auth_cde()`/`clib_auth_name()` return `EPERM` and `iefssreq()` -1 (`errno` `EPERM`) to an unauthorized caller, which ended S047 (#402); the state functions return the caller in its own state and key. `jescanj()`, `jesdelj()`, `jesreque()`, `jesxwrtr()`, `jesxdone()` return a negative value when the request did not reach JES - minus the SSI return code, or -1/`EPERM` - where they returned 0 (#406) | Code that tested these JES2 functions for `== 0` keeps working; code that tests `!= 0` for failure now sees the failures that were hidden. A negative value is new |
| 2.19.0 | `wtor()`/`wtorf()`/`vwtorf()` terminate the reply with a null character within `replymax` bytes, so `replymax` is the buffer size and the reply is at most `replymax` - 1 characters (#404); before, `replymax` counted the characters and nothing was terminated. `wtorf()` with a text ending in a newline issues the WTOR. `wto()` takes `const char *` and leaves the text as it is. `wtodump()` limits `chunk` to 1-32 (0 or less: 16) and `wtodumpf()` truncates its title to 79 characters | A program that passed `sizeof(reply) - 1` after clearing the buffer keeps working with a reply one character shorter: pass `sizeof(reply)` |
| 2.19.0 | `arrayaddf()` formats a string of any length (#398; above 1023 characters it overran the stack) and frees its copy when the item cannot be added | Nothing |
| 2.19.0 | `__64_div()` and the functions built on it return quotient 0 with `errno` `EDOM` for a zero divisor (#397; they looped). `__64_mul_i32()`/`__64_div_i32()` with a negative operand multiply/divide by its two's complement (they did the other operation). `__64_from_string()` carries decimal digits and reads binary after `0b` | Code that worked around the swapped operation by calling the other function, or that parsed decimal through `strtoull()` for the carry, can drop the workaround |
| 2.19.0 | `ioctlsocket(FIONBIO)` uses the value at `argp` (#396): a pointer to 0 makes the socket blocking, where any non-NULL pointer made it non-blocking; the word is no longer set to 0, and a number that is not a socket returns -1. `select()` waits the `tv_usec` asked for, rounded up to 0.08 s (it waited about 1250 times longer). `FD_ZERO` no longer clears the byte behind the set. `gethostbyaddr()` writes nothing to `stdout`; `bind()` to port 0 leaves `rand()` alone | A program that passed `&zero` and relied on getting a non-blocking socket passes a nonzero value. Recompile to get the corrected `FD_ZERO` |
| 2.19.0 | `mtxfree()` releases a mutex the task holds and accepts `NULL` (#403). `cthread_create_ex()` raises a stack below 1 KB to 1 KB and refuses 16 MB or more (it kept 20 bits). `cthread_detach()` leaves the thread's return value in `task->rc`. `cthread_queue_add()` returns -1 for a NULL manager or one shutting down or stopped (it returned 0). `__ascb()` accepts the highest ASID | A caller of `cthread_queue_add()` that ignored the result frees or keeps `data` itself on -1. Code that read `task->rc` before detaching can read it afterwards as well |
| 2.19.0 | `__jsopen()` returns NULL when the open fails (#406; it abended S0C4). `__jsopen()`/`__cpopen()` accept a `dd:` name shorter than 8 characters. `__cpread()` returns -1 at the end of the data set and for an I/O error (it abended S337/S001). `jesjob()` uses at most 11 filter characters | A caller that loops on `__cpread()` until an abend stops at -1 |
| 2.18.1 | `idcams()` passes its command record by record within column 72, and `rename()` of a data set to a long name renames it to that name (#590; it was cut at column 72 with condition code 0). A command line that cannot be broken there returns -1 with `errno` `EINVAL` | Nothing. A data set renamed by 2.18.0 or earlier may sit under a shortened name: look for one |
| 2.18.1 | `ssvt_set()`/`ssvt_funcmap()` return `EPERM` for a slot above `ssvtfnum` (`ssvt_funcmap()` also above 255); `ssct_remove(NULL)` returns 4 (#407) | A caller that used slots beyond the `funcmax` it gave `ssvt_new()` creates the SSVT with enough slots |
| 2.18.1 | `racf_logout()` returns -1 with `errno` `EINVAL` for a `NULL` `acee` or `*acee` and deletes nothing (#405); it deleted the address space's resting ACEE | Nothing for a caller that checks the ACEE first, as the ones known do |
| 2.18.1 | `getmain()` returns `NULL` for a size above X'FFFFB8' (#401; it asked for 0 or 64 bytes and the program ended S0C4). `getmain()`/`freemain()` no longer link the printf engine (#283); `freemain()`'s message for an invalid header is one line instead of a dump | Nothing. A tool that parsed `freemain()`'s dump reads the one line |
| 2.18.0 | `printf()` follows C99 (#220, #221, #384, #578). Wrong values are fixed: `%f` below the last place printed (`%.3f` of 0.000123456 is `0.000`, was `0.0006`), `%g` with significant digits (`%.3g` of 123.456 is `123`), the `0` flag after the sign (`%05d` of -1 is `-0001`, was `000-1`). The form follows C99: `e` in `%e`/`%g` (was `E`), no point for `%.0e`, `#` on `o`, `x` of 0, `X` (`0X`) and the floating conversions, `0` ignored with an integer precision, `h`/`hh` narrow. `<ibm/mvs/ihaacee.h>` adds `aceeusrl`/`aceeusri` and `aceegrpl`/`aceegrpn` (#581) | Output compared byte for byte (reference files, fixed-width reports) changes in these cases. Code that parses an exponent accepts `e`; code that relied on `%h` printing the full `int` drops the `h`. New code can read `acee->aceeusri` with `acee->aceeusrl` |
| 2.17.0 | The library's common functions link less (size analysis F1-F5): `SYSENV` is read without `fopen()`, `sprintf()`/`snprintf()` and the standard streams without the stream layer's name handlers, the library's diagnostics without `printf()`. A `getenv()` program links 25.0 KB instead of 72.5 KB. A `SYSENV` record over 1023 bytes is cut there instead of continuing as a line of its own, and a `SYSENV` on a TSO terminal is not read | Nothing, unless `SYSENV` has such records or is the terminal: then call `loadenv("DD:SYSENV")` yourself |
| 2.16.0 | The start-up loads `SYSENV` on the first `getenv()`/`setenv()`/`putenv()`/`unsetenv()` and sets the time zone on first need (#564); an empty `main()` links 14.8 KB instead of 72.7 KB. `estae()` without a routine reports on the console instead of `stderr` | Nothing. A program that relied on `tzset()` having run at start-up gets the same offset on first use |
| 2.15.0 | The standard streams open on first use, not before `main()` (#564). A step gets no SYSOUT for `stderr` and no `NULLFILE` for `stdin` unless they are used; a failing `stdout`/`stderr` is reported and replaced by `NULLFILE` instead of ending the program with RC 12; SYSENV's `SYSOUT_*` now apply to the standard streams' dynamic SYSOUT | Nothing, for most programs. Code that relied on `SYSTERM` being allocated, or on the RC 12, checks the stream itself. Assign a standard stream in `__premain()` to avoid opening the default first |
| 2.14.0 | `fread()` on a record stream returns the number of complete elements read - with `size` 1 the record length - instead of `nmemb` (#559) | Code that read records with `size` greater than 1 and stopped on a return of 0: a record shorter than one element now returns 0, so test `feof()` for the end. Code with `size` 1 can use the return as the record length |
| 2.13.0 | `recfm=`, `lrecl=`, `blksize=` in the mode of a `DD:` open take precedence over the DD statement and the data set (#554); before, they were ignored for `DD:` | Code that passed these options with a `DD:` name and relied on the DD's attributes winning drops them. An invalid value now fails with `EINVAL` |
| 2.12.1 | A C module entered by LINK no longer leaves 128 bytes of heap behind at each normal run (#546) | Nothing. Rebuild the LINKed module (not the caller) to get the fix: the leak is in the module's own run-time teardown |
| 2.12.0 | New: `__fx37()` (`<mvs/file.h>`) returns the x37 code (D37-04, B37-04) a write that failed with `ENOSPC` stood for (#539) | Nothing |
| 2.12.0 | A data set that runs out of space at its 16th extent, or on VIO, reports `ENOSPC` instead of `EIO` (#538) | Nothing. Code that treated `EIO` on a write as "maybe full" can test `ENOSPC` alone |
| 2.11.0 | New: `tryinfo()` (`<mvs/recovery.h>`) returns the PSW and register 14 of the abend the last `try()` caught (#530). The CRT words `crtavail[3]` at +17C are now `crttrpsw[2]` and `crttrr14` | Nothing. Code that used `crtavail` or `CRTAVAIL` (unused space) renames it - nothing in libc370 did |
| 2.10.3 | A member whose write failed (out of space, I/O error) is not put into the directory when the stream is closed (#524) | Nothing. A program that relied on the empty member appearing after a failed write - none should - no longer finds it; the old member keeps its content |
| 2.10.2 | `__listds()` returns a data set or catalog name that contains `-` whole, and its filter sees the whole name (#519) | Nothing. A caller that matched the cut names (`IBMUSER.OFTPIN.NOT` for `IBMUSER.OFTPIN.NOT-CATE`) gets the real ones |
| 2.10.1 | `vswrite()` on a KSDS opened with `VSACCESS_DYNAM` puts direct; `vsread()` of a missing key reports reason code 16 (#514) | Nothing. Code that treated reason code 12 as "duplicate" on such a handle checks for 8 now; a local direct-`PUT` workaround can go |
| 2.10.0 | A COBOL main program calling C through `@@CCALL` gets a C environment instead of U3101 (#507) | Call `CCTERM` before `STOP RUN`, or buffered C output and `atexit()` functions are lost. Code that relied on U3101 to detect a missing C program: it is now raised only on a C thread or a subtask of a C program |
| 2.9.0 | COBOL and other OS-linkage code can call a C function through `@@CCALL` and a stub made by the macro `CCALL` (#502) | Nothing. Code that called C directly from COBOL failed; it can use the stub |
| 2.8.1 | The sysroot carries `share/libc370/sysmac-removed.txt`, the 118 IBM macros releases before 2.8.0 put into `<sysroot>/macros` (#498) | After an upgrade that unpacked the tarball: `cd <sysroot>/macros && grep -v '^#' ../share/libc370/sysmac-removed.txt \| xargs rm -f`. `make install`, `apt`/`dnf` and Homebrew need nothing |
| 2.8.0 | Needs **cc370 1.6.0** or later; every public header stops with `#error` otherwise. `sysmac/` is gone (#494). libc370 no longer installs SYS1.MACLIB or SYS1.AMODGEN members into `<sysroot>/macros`, only its own macros and the five JES2 mappings (`hasp/`). The IBM members come from maclib370, `<sysroot>/maclib370/macros`, which `as370` searches by itself. `fclose()` changes by 8 bytes (FREEPOOL at the TK5 level) | Update cc370 to 1.6.0. Then nothing, with `as370`'s default search. A build that names `<sysroot>/macros` with `-I` or `AS370_MACLIB` adds `<sysroot>/maclib370/macros`. A sysroot filled by an unpacked tarball still holds the old copies; `make install` and the packages remove them |
| 2.7.0 | The packages and the Homebrew formula depend on maclib370 >= 1.0.0, < 2 (SYS1.MACLIB and SYS1.AMODGEN, #490) | `apt`/`dnf`: download maclib370's package from its release page and name it beside libc370's. Homebrew: `brew trust mvslovers/tap` before `brew install mvslovers/tap/libc370`. Tarball: unpack maclib370's tarball into `<sysroot>/maclib370` |
| 2.1.0 | Needs cc370 1.1.0 or later. Every public header stops with `#error` otherwise (#315) | Update cc370 |
| 2.1.0 | The compiler helpers (`@@DIVDI3`, ...) moved to cc370's `libcc370rt.a` (#313) | A build that runs the linker itself adds `-lcc370rt` |
| 2.2.0 | `setbuf()` returns `void` (#339) | Stop using its return value |
| 2.6.3 | `rand()` returns values from 0 to `RAND_MAX`, and without `srand()` its sequence is the one `srand(1)` starts (#387). Before, it returned only 0..4095 and 32768..36863, and the first call without `srand()` returned 0 | Nothing, unless a program depends on the exact numbers: every sequence differs from 2.6.2's. A workaround such as `rand() & 0xFFF` keeps working |
| 2.6.2 | `fseek()` to a position inside the current record of an `"r"` stream lands there, and `ftell()` reports it (#473). Before, the stream was placed wrongly within the record or in front of it, and `ftell()` kept the old position | Nothing. A workaround that seeked only to the start of a record keeps working |
| 2.6.1 | stdio skips the stream's ENQ only when no other task of the job step can run C code: this task has no subtask and no task above it is a C task (#470) | Nothing. Subtasks a program ATTACHes itself are serialized again, and a module LINKed into a thread locks the outer program's streams |
| 2.5.0 | stdio skips the stream's ENQ while no thread was created through `cthread_create()` (#453) | Nothing for a program without threads or with `cthread_create()` threads. A program that ATTACHes its own subtasks outside `cthread_create()` and shares a `FILE` with them serializes it itself, e.g. with `lock(fp, 0)` around each use |
| 2.5.0 | `fprintf()` writes results of 8192 characters and more in full, and `printf()`/`fprintf()`/`vfprintf()` return a negative value on an output error (#385). Before, `fprintf()` cut at 8192 bytes with a X'00' as the last one and returned the bytes written | Nothing, unless a program relied on the cut, or treats every return value of `printf()` as a count |
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
