# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Fixed
- **`va_start()` with a `double` or `long long` last parameter (#382).**
  `<stdarg.h>` defined `va_start(ap, last)` as `&last + 4`, which is right
  only for a last parameter of 4 bytes. After a `double` or `long long`
  every `va_arg()` read 4 bytes early. `<stdarg.h>` now uses the compiler's
  builtins, which know the parameter list. `va_list` is still a `char *`,
  so code that passes one on (to `vprintf()` and the like) is unaffected,
  and no library function changes behaviour: none has such a last
  parameter. The same applies to a `char`, `short` or `float` last
  parameter, which C99 leaves undefined. Measured on MVS: 8 of 8 cases,
  previously 2.
- **printf no longer shifts its arguments after a conversion it did not
  handle (#383).** `%c` with a width or a flag, `%n` with a length
  modifier, `%a` and `%A` printed nothing and did not take their argument,
  so every later conversion printed the argument meant for the one before
  it (`"%5c|%d"` of `'x', 42` gave `|120`). `%c` now pads to its width. `%n`
  stores the count at the width that `hh`, `h`, `l`, `ll`, `j`, `z` or `t`
  names. `%a` and `%A` are still not implemented: they now take their
  `double` and print the conversion as written, as does any conversion
  without a meaning. A `%` at the very end of a format no longer makes
  printf read past the end of the format string. This applies to every
  function of the printf family. Measured on MVS: 34 of 34 cases,
  previously 0.

### Removed
- **The PDPCLIB macros `PDPMAIN`, `PDP370`, `PDP380`, `PDP390` and
  `PDPORIG` (#377).** Nothing in libc370 or cc370 used them. cc370 emits
  its `main` stub inline and ships its own `PDPTOP`, which contains all of
  `PDP370`. They are no longer installed into `<sysroot>/macros`. Assembler
  that still contains `COPY PDPMAIN` comes from a pre-cc370 compiler.
  Regenerate it with cc370. `make install` does not delete macro files, so
  a sysroot that had them keeps its copies until they are removed by hand.

### Changed
- **`docs/migration-2.0.md` is now `docs/migration-2.x.md`.** A new
  section, *Since 2.0.0*, lists what each later 2.x release asks of a
  program or a build.

## [2.2.0] - 2026-10-04

Requires cc370 1.1.0 or later, as 2.1.0 did (`sdk/cc370.json`: `>=1.1.0 <2`).

### Added
- **`<stdbool.h>` and `<iso646.h>` (#336).** Neither existed, and cc370
  searches only the sysroot, so `#include <stdbool.h>` failed in every
  program; C99 requires both even of a freestanding implementation.
- **`va_copy` in `<stdarg.h>`, `FLT_EVAL_METHOD` (0) and `DECIMAL_DIG` (18)
  in `<float.h>` (#336).**
- **The 35 missing `SCN*` macros of `<inttypes.h>` (#336):** every 8-bit,
  64-bit and `MAX` width, possible since scanf learned `hh`, `ll` and `j`
  (#318). cc370's `-Wformat` checks all 70 against their types.
- **The `<ctype.h>` functions are declared (#337).** The header defined only
  macros, so `(isalpha)(c)`, `&isalpha` or `#undef isalpha` did not compile,
  though all 14 functions were in `libc.a` (C99 7.1.4). The generated code
  of the library is unchanged.
- **`vprintf()`, `vscanf()`, `vfscanf()`, `vsscanf()` (#338)**, the
  `va_list` forms C99 requires; thin wrappers over the existing engines.

### Changed
- **`snprintf()`/`vsnprintf()` take a `size_t`, `setbuf()` returns `void`,
  as C99 declares them (#339).** A program that declared the C99 prototype
  itself did not compile, and host compilers warned about the declarations.
  On S/370 callers pass the same bits, so this is treated as a defect fix:
  **only code that used `setbuf()`'s return value needs a change** (C99 has
  none; it was `setvbuf()`'s). A size of `SIZE_MAX` now means "large", not
  -1.
- **libc370 is built with `-Os` instead of `-O1` (#344).** `libc.a`'s text
  shrinks from 284,430 to 276,313 bytes (-8,117, -2.9%; 282 members smaller,
  89 larger by at most 71 bytes), and a program linked against it is about
  1.7-4.3 KB smaller. The library's MVS test series (55 tests) ran identically against the `-O1` and the `-Os` library.
  `LIBC370_OPT=-O1` builds the previous variant.

### Fixed
- **`fopen()`, `__listds()`, `__listvl()` and the dynalloc text-unit
  builders no longer move the caller's `strtok()` position.** They
  tokenised with `strtok()`, whose position is one per task, so a caller
  looping with `strtok()` and calling one of them in the loop lost its
  place: the next token was `NULL` after `fopen()` creating a data
  set or a SYSOUT with DCB keywords, and `"FUNCTION C"` - a piece of the
  IDCAMS output - after `__listds()`. The library now tokenises
  on a position of its own. #301 fixed the same in `jesjob()`; `__dsalc()`
  already saved and restored the position.
- **A program without SYSIN starts in about 100 K less REGION (#277).**
  @@start opens stdin as `'NULLFILE'` when there is no SYSIN DD, and with no
  DCB attributes that opened LRECL=BLKSIZE=32760: two 32 K buffers in
  `__aopen()` and a 32 K C buffer, for a stream that never transfers a
  byte. A DUMMY read - `'NULLFILE'`, or a JCL `//SYSIN DD DUMMY` - now
  opens with 80-byte records and no C buffer of its record size. A
  small test program now reaches `main()` from REGION=336K; it needed 448K
  before (measured in 16K steps, so 96-128K less).
- **@@start says why a standard stream could not be opened (#254).** It
  reported "SYSTERM DD not defined" or "SYSIN DD not defined" - in a
  dynamic SYSOUT - and nothing at all for stdout, while the cause was
  storage. Now a WTO in the job log names the stream and errno, e.g.
  `@@START: stderr (SYSTERM) could not be opened: errno 12, out of storage
  - raise REGION`. `fopen()` now sets `errno` to `ENOMEM` when `__aopen()`
  runs out of storage for its buffers (it left `errno` 0).
- **`ssvt_set()` and `ssvt_funcmap()` return 0 on success (#240).** The
  success path fell off the end of the function and left
  `0x100 | key<<4` in R15 - 384 for a key-8 caller, never 0 - so a caller
  testing the result read every success as a failure.
- **`jesjob()` no longer destroys the caller's `strtok()` position
  (#301).** It trimmed blank-padded fields with `strtok()`, whose position
  is library-wide, so a caller looping over names with its own `strtok()`
  and calling `jesjob()` in the loop lost its place after the first name
  The fields are trimmed by hand now, cut at the
  same place.
- **The list builders no longer hand back a short list when storage runs
  out (#61, #157, #158).** `__listvl()` and `__listal()` returned what they
  had built so far, and `__listds()` skipped the record and went on, so its
  list was missing a data set somewhere in the middle - each read as
  complete. Now all four (`__listpd()` since 2.0.0) free what they built and
  answer `NULL` with `errno` `ENOMEM`, and clear `errno` on entry, so an
  empty result is `NULL` with `errno` 0. `__listal()` also no longer leaks
  the record whose array insertion failed, and `__listvl()` no longer
  writes "out of memory" to the console. **Callers that treat `NULL` as
  "empty" should read `errno`.** Failing every allocation in turn: 0 short
  lists; the previous library 42.
- **`__listds()` no longer loses the data set after an entry without a
  volume (#308).** An entry with no `VOLSER` line kept the parser inside it,
  so the next entry line was skipped: that data set was missing and its
  volume was reported for the entry before. It happens in practice: with the option
  `VOLUME`, `LEVEL('SYS1')` listed `SYS1.PAGECSA` on `SYS1.PARMLIB`'s volume
  and lost `SYS1.PARMLIB`, `SYS1.SVCLIB` and `SYS1.VTAMLIB` (page spaces and
  clusters carry no volume there). With `"NONVSAM VOLUME"` it does not
  happen. An entry without a
  volume is still not listed.
- **`floor()`, `ceil()`, `modf()` and `fmod()` are right beyond 2**31
  (#273).** All four took the integral part through a 32-bit integer, which
  cc370 converts modulo 2**32: `floor(2147483648.5)` gave -2147483648 and
  `fmod(10540800000, 1)` 8589934592. The integral
  part is now built from power-of-16 pieces that are exact in HFP, without
  linking cc370's `long long` conversion helpers; from 2**52 up a double is
  integral and is returned as it is. `fmod()` is still computed as
  `x - trunc(x/y)*y` - no more exact than before, but now in `[0, |y|)`
  for any quotient. Of 40 test cases on MVS all pass; the previous library passed 15.
- **`__dsalc()`/`__dsalcf()` with `UNIT=` or `VOLSER=` no longer wait for
  the operator (#181).** A volume that was not mounted sent SVC 99 into
  allocation recovery (`IEF238D REPLY DEVICE NAME OR 'CANCEL'`), and the
  caller's task stopped until someone replied - a server thread, for
  instance, handed a mistyped volume name by a client. The request now carries `S99NOMNT` when
  it names a unit or a volume, as `fopen()` does since #172, and comes back
  with an error at once; without either, the request is unchanged. A caller
  that wants the operator to mount a volume (a tape, a volume not yet
  mounted) says so with the new keyword **`MOUNT`** (`fopen()`: `,mount` in
  the mode string), which leaves `S99NOMNT` off.
- **`tmr_stop()` waits for the timer thread to end before deleting it
  (#345).** It deleted the thread at once, the delete was refused because
  the thread had not ended yet (#11), and the handle was dropped anyway: a
  program that stopped its timer and returned ended **ABEND SA03**, a task
  ending with a subtask still attached. The wait is bounded (5 seconds). If
  the thread still runs, the handle is now kept instead of lost.
- **printf honours the `0` and `-` flags for `%f`, `%e` and `%g`, and counts
  a `+` or space sign in the width (#355).** `"%05.1f"` of 2.5 printed
  `"  2.5"` (now `"002.5"`), `"%-6.1f"` padded on the left, and `"%+6.1f"`
  came out seven characters wide.

## [2.1.0] - 2026-10-03

**Requires cc370 1.1.0 or later.** The compiler's helper routines and the
prologue macros now ship with cc370 itself, and every libc370 header checks
the compiler version.

### Changed
- **The compiler helpers and prologue macros moved to cc370 (#313).** The
  64-bit and conversion helpers cc370 emits calls to (`@@MULDI3`,
  `@@DIVDI3`, `@@FIXDFD`, ...) are in cc370's `libcc370rt.a`, `PDPTOP`,
  `PDPPRLG` and `PDPEPIL` in its macro directory. libc370's copies and their
  tests are gone. No public header declared either, so no source changes; a
  build that runs the linker itself has to add `-lcc370rt`.
- **Every public header checks the compiler (#315).** `<sys/_cc370.h>`,
  included by all of them, stops a target build with `#error "libc370 needs
  cc370 1.1.0 or later"` when `__CC370__` is missing or older. Host
  compiles (no `__MVS__`) are unaffected. The number comes from
  `sdk/cc370.json`, the one place the requirement is written; CI fails when
  the header and that file disagree.

### Added
- **Release artifacts (#326).** Every release carries the sysroot tarball
  `libc370-<v>-sysroot.tar.gz` (unpack it into a cc370 sysroot), the
  packages `libc370-dev_<v>_all.deb` and `libc370-devel-<v>.noarch.rpm`
  (`/usr/lib/cc370/cc370`), `libc370-<v>-metadata.json` with the cc370 range
  and `SHA256SUMS`. The cc370 requirement is written once, in
  `sdk/cc370.json`; the package dependencies, the metadata and the release
  notes are derived from it, and the release is built with the cc370 release
  it names as the minimum.
- **`_Exit()` in `<stdlib.h>` (#314).** Ends the program without running
  `atexit()`/`on_exit()` functions; their registrations are dropped and the
  rest of `exit()`'s teardown still runs -- streams are closed and the
  runtime's storage is freed, which matters where the runtime does not end
  with its task. External name `@EXIT`. Measured on MVS.
- **`<inttypes.h>` (#314):** `imaxabs()`, `imaxdiv()`/`imaxdiv_t`,
  `strtoimax()`, `strtoumax()` and the `PRI*` macros for every width,
  `LEAST`, `FAST`, `MAX` and `PTR`. `SCN*` exists for the 16- and 32-bit
  types and `PTR` only: `scanf` does not know `hh`, `ll` or `j` yet
  (#318), and a missing `SCNd64` fails at compile time where `"lld"`
  would compile and store 4 of 8 bytes. `wcstoimax()`/`wcstoumax()` follow
  with the wide-string conversions. cc370's `-Wformat` checks every macro
  against its type; measured on MVS, 19/19.
- **`strtof()` and `strtold()` in `<stdlib.h>` (#314).** `long double`
  is `double` under cc370, so `strtold()` is `strtod()`. `strtof()` narrows
  `strtod()`'s result and answers a value above `FLT_MAX` itself with
  `FLT_MAX` and `ERANGE`: a plain `(float)` of such a value rounds past the
  largest HFP exponent and ends **S0CC** (measured on MVS).
- **`isblank()` in `<ctype.h>` (#314)**, as macro and function, true for
  `' '` and `'\t'` (EBCDIC X'40' and X'05') only, through a new bit 0x0800
  in the `__isbuf` table. Measured on MVS, 10/10.
- **`strtoll()`, `strtoull()`, `atoll()`, `llabs()`, `lldiv()` and
  `lldiv_t` in `<stdlib.h>`; `LLONG_MIN`, `LLONG_MAX` and `ULLONG_MAX` in
  `<limits.h>` (#314).** The C99 `long long` siblings of `strtol()`,
  `strtoul()`, `atol()`, `labs()` and `ldiv()`; before, a program calling
  one failed to link (`STRTOLL` unresolved). The two parsers follow C99
  where their `long` siblings do not yet: out-of-range input saturates and
  sets `ERANGE`, `endptr` is `nptr` when nothing converts, a digit must be
  below the base, and base-36 letters are read through a table rather than
  `c - 'A'`, which is wrong in EBCDIC past `I`. An invalid base sets
  `EINVAL`. Built with cc370 1.0.0; `test/mvs/tstll.c` passes on MVS (64/64).

### Fixed
- **scanf reads integers and floats like `strtoul()` and `strtod()`
  (#316).** `%o`/`%x`/`%i` take only digits below the base and `0x` only as
  a prefix (`"1x2"` in `%x` was 0x12); `%e`/`%f`/`%g` go through `strtod()`,
  so a value past the HFP range no longer ends S0CC, and a `%f` above
  `FLT_MAX` stores `FLT_MAX`. Measured on MVS: 43/43; the 2.0.0 library
  ends S0CC on the same test.
- **`strtod()` and `atof()` no longer end S0CC out of range (#316).**
  Any exponent past about 75, either way, overflowed an intermediate:
  `strtod("1e76")`, `("8e75")` and `("1e-80")` all abended (measured).
  The range is now checked before any intermediate is formed; overflow
  returns `±HUGE_VAL`, underflow 0, both with `ERANGE`. Also C99 now: `e+5`
  is accepted, `"1e"` leaves `endptr` at the `e`, and nothing converted
  leaves it at `nptr`. Measured on MVS, 32/32.
- **`strtol()`, `strtoul()` -- and with them `atoi()` and `atol()` --
  follow C99 (#316).** Rewritten on the shape of `strtoll()`/`strtoull()`.
  **Behaviour changes a caller can see:** a digit must be below the base
  (`strtoul("9", 8)` was 9, `"g"` in base 16 was 16); `strtoul` takes a sign
  (`"-1"` is `ULONG_MAX`, was 0) and `strtol` rejects a second one; out-of-
  range input saturates and sets `ERANGE` instead of wrapping (`atoi` of an
  out-of-range string now gives `INT_MAX`/`INT_MIN`); `endptr` is `nptr`
  when nothing converts; an invalid base sets `EINVAL`; letters past `I`
  read right in EBCDIC. Measured on MVS: 35/35, against 17 of 35 failing
  with 2.0.0.
- **scanf knows the length modifiers `hh`, `ll`, `j`, `z`, `t` and `L`
  (#318).** It knew `h` and `l` only: `%lld` stored a `long`, which is the
  high word of a `long long` on S/370, and left the low word as it was;
  `%hhd`/`%hhn` wrote past a `char`; `j`, `z` and `t` derailed the format.
  `%n` honours the modifiers too. Behaviour change: a `-` before a `%u`,
  `%x` or `%o` value now negates it as `strtoul()` does, instead of being
  dropped. Measured on MVS: 23/23, against 18 of 23 failing with 2.0.0.
- **`%lld`, `%lli` and `%jd` print a negative value with its sign (#321).**
  The 64-bit path of `printf` treated every value as unsigned, so `-5`
  printed `18446744073709551611` -- and so did `PRId64`. Measured on MVS:
  17/17, against 11 of 17 failing with 2.0.0.

## [2.0.0] - 2026-10-01

**libc370 2.0 reorganises the public headers, and almost every program that
includes one has to change its `#include` lines.** Read
[doc/migration-2.0.md](https://github.com/mvslovers/libc370/blob/v2.0.0/doc/migration-2.0.md)
before upgrading: what moved where, how to migrate a source file, the
interfaces that changed meaning, and how to stay on 1.x. 1.0.8 is the last
1.x release; its tag `v1.0.8` and the branch `1.x` stay.

Headers moved into `mvs/`, `ext/`, `ibm/`, `s370/` and the POSIX headers;
mixed headers were split, internal ones left the sysroot; there are no
compatibility headers. Crypto moved to
[crypto370](https://github.com/mvslovers/crypto370).

**Requires the current cc370** (`main`, cc370@latest). 2.0.0 was built and
tested with cc370 `47b3545`; an older compiler is not supported.

### Added
- **`inet_addr()`, `inet_ntoa()`, `inet_pton()`, `inet_ntop()` in
  `<arpa/inet.h>` (#51).** `AF_INET` only. None of them uses `scanf` or
  `printf`, so converting an address no longer pulls either into a load
  module (a server parsing addresses with `sscanf("%u.%u.%u.%u")` was the
  case in point).
  `inet_ntoa()` keeps BSD's semantics -- one buffer, overwritten by the next
  call -- but takes it per process from `__wsaget()`, since a reentrant load
  module cannot write a static; between threads use `inet_ntop()`. It
  returns NULL when the runtime has no process anchor -- `inet_ntop()` into
  a buffer of the caller's never does.
  `INET_ADDRSTRLEN` (`<netinet/in.h>`) and `socklen_t` (`<sys/socket.h>`)
  come with them.
- **`__walkpd()` in `<mvs/dslist.h>` (#80).** A PDS directory member by
  member, handed to a callback that may stop the walk; nothing is allocated,
  so a directory of any size costs one block buffer. `__listpd()` built a
  record per member first, and on `SYS1.SMPCDS` (some 23000) that exhausted
  the caller's region -- an FTP server's `LIST`, for instance. A caller that
  walks the directory itself for that reason can use it instead.
- **`idcams_sysprint()` in `<mvs/idcams.h>` (#71).** `idcams()` returns
  IDCAMS's condition code and nothing else, so 8 meant "not found" and
  "refused" alike. `idcams_sysprint(fn, arg, fmt, ...)` runs the
  same commands and calls `fn(arg, msgno, text, len)` for every SYSPRINT
  line, with the IDCnnnnI number read from the line -- the number IDCAMS
  hands its output exit drops the leading digit (IDC3012I arrives as 12) and
  gives the two summaries -1 and -2, measured on MVS 3.8j. The
  header documents the record format and which message explains a condition
  code. `idcams()` is unchanged.
- **`fopen()` can say where a new data set goes: `unit=` and `volser=`
  (#172).** `__fpnew()` sent no `DALUNIT` and no `DALVLSER`, so every data
  set `fopen(name, "w...")` created landed on SVC 99's default unit, and a
  caller that needed a volume had to `__dsalcf()` first and `fopen()` the
  existing data set. The mode string now takes the same two
  keywords `__dsalc()` does -- `fopen(dsn, "wb,unit=sysda,volser=pub001")`,
  several volumes as `volser=(pub001,pub002)` -- and the environment
  `DATASET_UNIT` / `DATASET_VOLSER`, next to `DATASET_SPACE`. The mode string
  wins, as for the other four. Without either, or with an empty value, the
  request is the one 1.x sent. They apply where the other DCB keywords do:
  only when `fopen()` creates the data set (DISP=NEW). Environment values are
  passed as given, not folded to upper case, as `DATASET_RECFM` always was.
  A request that names a unit or a volume carries `S99NOMNT`: without it a
  volume that is not mounted does not fail -- SVC 99 goes into allocation
  recovery (`IEF238D REPLY DEVICE NAME OR 'CANCEL'`) and the task waits for
  the operator. With it, `fopen()` returns NULL at once. Measured on MVS
  3.8j: with `unit=sysda,volser=<vol>`, with `volser=<vol>` alone, and
  through the environment, the data set is in the catalog and in the VTOC on
  the named volume, where the same `fopen()` without them put it on the
  system's default volume.
  Also fixed on the way: the mode string folded everything to upper case
  except what stood in parentheses, so a volume list would have reached
  SVC 99 in lower case. No earlier keyword changes its result: `lrecl=`,
  `blksize=` and `space=` carry digits, and `recfm=` goes through
  `__txrecf()`, which folds case itself.

### Changed
- **`__listpd()` answers NULL with `errno` `ENOMEM` when storage runs out
  (#80).** 1.x returned the records it had collected so far -- a short member
  list indistinguishable from a complete one. It now frees them and says so.
  It also collects on top of `__walkpd()`, and no longer uses `strtok()`,
  which ended a caller's own `strtok()` loop (#301).
- **`JESJOB` grows from 80 to 96 bytes: `submit_time64` and `sysid` (#79).**
  The submit time (JCTRDRON/JCTRDTON, time on the input processor) and the
  input processor's system id (JCTRDSID) were in the JCT and surfaced
  nowhere, so a job API had to leave z/OSMF's `exec-submitted` empty.
  Both are appended at 0x50, so offsets 0x00-0x4F keep their 1.x values.
  `jesjob()` allocates every `JESJOB`, so a consumer only rebuilds. Measured
  on MVS 3.8j: a job held 20 seconds shows its submit 21 seconds before its
  start.
- **`DSLIST` grows from 98 to 104 bytes: `catnm`, the catalog an entry was
  found in (#50).** A z/OSMF-style data set API had to answer `catnm` with
  `""` for want of it. IDCAMS LISTCAT on MVS 3.8j names the catalog per
  entry (`IN-CAT --- SYS1.VSAM.MASTER.CATALOG`, or a user catalog), and
  `__listds()` already reads that output, so it costs no extra I/O. The field
  is a `const char *`, not a `char[45]`: at 104 bytes a record still costs
  128 bytes of GETMAIN, where an inline name would have made it 192. The string
  belongs to the list -- consecutive entries from one catalog share it, and
  `__freeds()` frees each name once, also after the caller has reordered the
  array. Copy it to keep it past `__freeds()`. NULL when the listing names no
  catalog, and in every record `__listal()` builds. Appended after `disp`, so
  every 1.x offset is unchanged; `__listds()` and `__listal()` allocate every
  `DSLIST`, so a consumer only rebuilds. Measured through the real
  `__listds()` on MVS 3.8j: 30 entries of a user's high-level qualifier, all
  from one user catalog in one shared string, 11 under `SYS2`, all
  `SYS1.VSAM.MASTER.CATALOG`.
- **`in_addr_t` is an integer and `inet_aton()` returns 1 for an address
  (#51).** 1.x defined `in_addr_t` as `struct in_addr` and had `inet_aton()`
  return 0 for an address and -1 for none -- the opposite of BSD, z/OS and
  Linux, so code written for them read every valid address as an error.
  2.0 follows POSIX: `in_addr_t` is `unsigned long` (32 bits, the type
  `s_addr` always had), `struct in_addr` holds it as `s_addr`, and
  `inet_aton(const char *, struct in_addr *)` returns 1 or 0. It also refuses
  what 1.x took: anything after the last part but a blank (`"1.2.3.4x"`), and
  a first part above 255 in the four-part form. Code that used `in_addr_t`
  as the struct says `struct in_addr` instead; code that tested
  `inet_aton() == 0` for success inverts the test. `struct in_addr` and
  `sin_addr` keep their layout.
- **`make install` replaces the sysroot's `include/` instead of adding to it
  (#256).** It clears the directory, then copies the header tree with its
  subdirectories, which 2.0 introduces (`mvs/`, `ibm/mvs/`, ...). Before, it
  copied `include/*.h` and never deleted, so a header that left libc370 stayed
  in every sysroot it had once been installed into. libc370 owns that
  directory: cc370 searches no other, and every file found there came from
  libc370.
- **`libc.a` no longer carries the path it was built in (#256).** `mklibc.py`
  names each source relative to the repository, so `assert()` in
  `tm64syr.c` records `src/time64/tm64syr.c` instead of the builder's absolute
  path, and two checkouts build the same library.

### Removed
- **SHA-256, Blowfish and base64 (#244, #256).** `sha256.h`, `blowfish.h`,
  `clibb64.h` and their ten TUs moved to
  [crypto370](https://github.com/mvslovers/crypto370) 1.0.0. Declare
  `mvslovers/crypto370` as a dependency and include `base64.h` instead of
  `clibb64.h`; the C names are unchanged, the base64 symbols are now
  `B64ENC`/`B64DEC`.
- **`clib.h` (#256).** It included `mvs/crt.h`, `mvs/wto.h` and `clibos.h`
  and defined eight `__getcrt()`-style aliases nothing used. Include what you
  need directly.
- **miniz headers (#243).** `clibmz.h`, `clibmzi.h`, `miniz.h`,
  `miniz_common.h`, `miniz_tdef.h`, `miniz_tinfl.h` and `miniz_zip.h`, plus a
  Windows download marker committed beside them. libc370 never shipped the
  code: a call compiled and then failed to link. No consumer included them.
- **PDF, `emfile` and `ipc` headers, and the `emfile`/`ipc` sources (#248).**
  `clibpdf.h`/`clibpdfi.h` (a PDF generator with no code in the tree),
  `emfile.h`/`emfilei.h` (a byte-addressed file over FB 4096 blocks) and
  `ipc.h`/`ipci.h` (message passing over loopback TCP), with the sources of
  the latter two under `src/wip/orig/`, which was never built. None of the
  90 symbols they declared was in `libc.a`, and no consumer included them.
  `emfile.h` also `#undef`'d `errno.h`'s `EMFILE` for whoever included it.
- **`clibsrb.h` is no longer installed (#248).** It moved to
  `src/wip/mvs/srb.h` for later. It had no user; its out-of-line
  `srb_getmain()`/`srb_freemain()` had no code behind them and are gone.

### Fixed
- **Abend reports show a user abend code again (#68).** `__abrpt()`
  formatted it with `"U%04D"`; libc370's printf has no `%D` and printed the
  letter, so a U0123 abend was reported as `UD`. Now `"U%04d"`. The same
  kind of slip in `localtime64_r()`'s failure message (`"%016LLX"`) printed
  `LLX` where the time value belonged.
- **`inline_srb_freemain()` freed from subpool 0 (#248).** Its FREEMAIN read
  `SP+(%2)`, which the macro accepts and then ignores, so storage got from
  `SRB_SUBPOOL` went back to subpool 0. Now `SP=(%2)`; the expansion loads
  the subpool into R0 as GETMAIN's does. It is in `src/wip/mvs/srb.h`, no
  longer installed.

## [1.0.8] - 2026-09-30

**The last 1.x release.** The next is 2.0.0, a hard cut to a new header
layout with no compatibility headers (#245, `doc/design-2.0.md`). Programs
that cannot migrate yet can stay on `v1.0.8`.

**Requires cc370 at `f3f7e21` or later**, unchanged from 1.0.7.

**Behaviour a caller can see**, all under "Fixed" below:
- a lost last block is reported: `rclose()` returns -1, and a `+` stream
  turning from write to read answers `EOF`/-1 with `ferror()` set, both with
  `errno` `ENOSPC`/`EIO`. `freopen()` still succeeds, as C99 requires, and
  only leaves `errno` set (#228)
- `rclose()` frees the DD that `ropen()` allocated (#229)
- `rwrite()` and record-mode `fwrite()` refuse a record they cannot write,
  with `EINVAL`, instead of abending or overrunning (#232, #236)

### Added
- **Prototypes for eight routines that had none (#39 step 1, PR #242).**
  `__tzget()` in `<time.h>`; `vwtorf()` in `clibwto.h`; `vvprintf()`,
  `vvscanf()`, `__fptmp()` and `__fpfree()` in `clibio.h`; `rdjfcb()` in
  `osjfcb.h`; `initssob()` in `clibjes2.h`. A program that carries its own
  `extern int __tzget(void);` still compiles: the signature is identical.
  `@@freepd.c` and `malloc.c` now call
  `arraycount()`/`arrayfree()`/`wto_traceback()` instead of undeclared
  aliases. All 23 touched TUs assemble byte-identical, so no code changes.
- **CI (#249).** `.github/workflows/build.yml` builds the library on every PR
  and push to `main`, with cc370 built from its `main` and cached per cc370
  commit. It reports the compiler warning count, and runs the host tests in
  a second job on macOS: on Linux they cannot link, because ELF reads the
  `@@` in libc370's `asm("@@…")` symbol labels as a version separator.
  `release.yml` publishes a `v*` tag: it checks that `VERSION` matches the tag
  and takes the notes from this file's section for the version (a tag without
  one fails). A release that already exists keeps its notes.
- **`make test-host` / `test/host/run.sh`**: every host test's recipe from its
  header comment, in one runner. 21 of the 26 run by default. The other five
  do not build on `main` and are listed in the script with their issues:
  `tstfabnd`, `tstfcls`, `tstfpapp` and `tstplus` since #182 (#241), and
  `tstjesop` since #126 (#251).
- `sdk/changelog-section.py`: prints one version's section of this file.

### Fixed
- **`freopen()`, the `+`-stream turn and `rclose()` report a lost last block
  (#228).** #182 made `fclose()` report an out-of-space on the final, short
  block, which only its CLOSE writes; these three paths close the DCB
  themselves and still ignored that rc, so the tail went missing and the
  caller was told it had succeeded.
  - **`rclose()`** returns **-1** with `errno` `ENOSPC`/`EIO` (it returned 0
    on every path). The handle is freed either way.
  - **A `+` stream turned from writing to reading** (the `fgetc()`,
    `fseek()` or `rewind()` after a write) still completes the turn, since
    the data set is valid, only shorter, but the call answers `EOF`/-1 with
    **`ferror()`** set and `errno` `ENOSPC`/`EIO`. Before, `fgetc()` answered
    a plain `EOF`, which read as the end of the data. After `clearerr()` the
    stream reads what is on disk.
  - **`freopen()`** still succeeds: C99 7.19.5.4 ignores a failure to close
    the old file. It now leaves `errno` at `ENOSPC`/`EIO` when the old
    stream lost its tail, which is the only place the loss can go. An
    `fflush()` before the `freopen()` does not help, because the short block
    is written at CLOSE, not at the flush.

  Measured on MVS with `test/mvs/tstclspc.c`, extended by these three
  paths over the same `TRK(1,0)` FB 80/800 cases (190 records in full
  blocks, 195–199 lose the short block): red against 1.0.7 (all three
  silent), green with the fix, 18/18. **Contract change**: a program that
  seeks on a `w+`/`r+` stream before every operation, as interpreters'
  stream I/O does, now gets an error where it got silent loss.
- **`rclose()` frees the DD `ropen()` allocated (#229).** `ropen()` on a data
  set name, rather than `dd:name`, allocates a DD by SVC 99 and records it in
  the handle; `rclose()` never released it, so every open by name held one
  allocation, with the data set, until step end. `rclose()` now unallocates
  it and returns **-1** with `errno` `EIO` if that fails (an `__aclose()`
  failure keeps its `ENOSPC`/`EIO`). `ropen()` also releases the DD when the
  open itself fails after the allocation, e.g. on a missing member.

  Measured on MVS with `test/mvs/tstrfree.c`, which counts the step's
  DSAB chain: red against the previous library (the chain grew from 6 to 28
  over 22 opens by name, the failed one included), green with the fix,
  10/10. The data set existed, so the probe covers the SHR allocation only;
  `ropen()`'s fallback that allocates a data set `NEW` (without a normal
  disposition) is not measured.
- **`ropen()` opens a quoted data set name without a member (#231).** The
  form `include/rfile.h` documents, `"'my.dataset.name'"`, failed: the
  opening quote was skipped but the closing one was copied into the DSN,
  and SVC 99 rejected the name (`errno` 860, `X'035C'`). With a member the
  copy stopped at `(` first, so `'x.pds(member)'` was not affected. Under
  TSO a fully qualified name had no working spelling at all, since the
  unquoted form gets the prefix. Measured on MVS with
  `test/mvs/tstrfree.c` checks (11)/(12): red against the previous library
  (`rc=12 errno=860`, the only FAIL), green with the fix, 12/12.
- **`rwrite()` refuses a record it cannot write instead of abending or
  corrupting the data set (#232).** On RECFM=V the record carries its RDW,
  as `rread()` has always returned it; `include/rfile.h` now says so for
  both directions. `rwrite()` answers non-zero with `errno` **`EINVAL`** and
  writes nothing when the record is longer than LRECL (LRECL-4 on a spanned
  data set), or on V shorter than 4 bytes, with an RDW that does not equal
  `size`, or with RDW bytes 2-3 not zero. Before, a V record without its
  RDW abended U0002 in `@@AWRITE`, and a record longer than LRECL overran
  the LRECL-sized work buffer and was written as it was: on VB it made
  every later read of the data set abend U1234, on FB it was cut to LRECL.
  An `__awrite()` failure now sets `errno` `ENOSPC` or `EIO` (it set none).

  Measured on MVS with `test/mvs/tstrwrv.c`: against the previous library
  each of the five groups fails or abends (U0002 twice, U1234 once), green
  with the fix, 16/16.
- **libc370 builds with an as370 that diagnoses non-EBCDIC source
  (#235).** `src/jes/jesiropn.c` named `__alloc_intrdr`'s `&FUNC` with two
  EN DASHes (U+2013) instead of `__`. The current as370 assembled their
  UTF-8 bytes silently into the prologue's identifier (six bytes of garbage
  where `__` belongs); the as370 of cc370#483 rejects a character without
  an EBCDIC image in a constant, rc 8, and `make build` stopped there -
  the only one of the 756 sources (728 `.s`, 28 `.asm`) that did. No change in behaviour.
- **`fwrite()` in record mode refuses a record it cannot write (#236).**
  The `",record"` path had #232's defect: `size*nmemb` bytes went into the
  LRECL-sized work buffer and to `@@AWRITE` unchecked. It now answers 0
  with `errno` **`EINVAL`**, writes nothing and leaves `ferror()` clear
  when the record exceeds LRECL (LRECL-4 spanned) or, on RECFM=V, when
  the RDW the caller supplies is short, does not match, or has bytes 2-3
  set. `size` or `nmemb` 0 now writes nothing and answers 0 (C99
  7.19.8.2); before, each wrote a blank record on FB, and so did a
  `size*nmemb` that wrapped to 0. The contract is documented at
  `_FILE_FLAG_RECORD` in `include/clibio.h`.

  Measured on MVS with `test/mvs/tstfwrec.c`: against the previous library
  VB accepts a record of LRECL+1 and its read-back abends U1234, a plain V
  record and a spanned one of LRECL-3 abend U0002, and FB ends with 5
  records where 1 was written; green with the fix, 19/19.

## [1.0.7] - 2026-09-29

### Added
- **`strcasecmp()` and `strncasecmp()` (#183).** The POSIX names were missing
  entirely, and programs had to work around that with their own
  case-insensitive compare.

  The capability was already in the archive under the MS-style names
  `stricmp`/`strncmpi` — but **nothing declared them**, in any header, so even
  those were unreachable without writing the prototype by hand. All four are
  now declared in `clibstr.h`, and that also closes `stricmp` as one of #39's
  14 routines with no declaration anywhere: `@@finden.c` and `@@listds.c`
  call it six times between them with no prototype in scope, which is #39's
  count of **2** for `stricmp` — it counts warnings, one per TU. Measured
  against the pre-change header, `-Wall` gives 1 + 1 before and 0 + 0 after.

  The two new functions are **standalone** translation units rather than
  wrappers around `stricmp`/`strncmpi`. That is right for `strncasecmp` and a
  wash for `strcasecmp`, which is not what the first draft of this entry
  claimed. The measurement: a program calling only `strlen()` already links
  `STRICMP`, because `@@FINDEN` — the environment lookup the CRT pulls in —
  calls it. So `stricmp` is resident in **every** link whatever anyone does,
  and a `strcasecmp` wrapper around it would have dragged nothing extra.
  `strncmpi` has no such caller, so there the wrapper really would have cost
  its whole CSECT on top of its own.

  The CSECTs, from the ESD, with a house-style wrapper built and measured
  rather than guessed at:

  | | standalone | wrapper |
  |---|---|---|
  | `strncasecmp` | X'E8' = **232** | X'74' = 116 + `STRNCMPI` X'E8' = 232 → **348** |
  | `strcasecmp` | X'F0' = **240** | X'6C' = 108 + `STRICMP` **0** → **108** |

  So standalone saves **116 bytes** on the `n` side and **costs 132** on the
  other, because `STRICMP` is already resident there. Keeping `strcasecmp`
  standalone is therefore the *more expensive* option, chosen for symmetry
  with its sibling and one less call frame on a limited stack. That is the
  whole of the argument; there is no member-count saving on that side.

  They fold through
  `tolower()`, i.e. the `__tolow` table, which is what makes them
  EBCDIC-correct — an ASCII-style fold (`c | 0x20`) would not be, since the
  letters sit in three runs with gaps: a-i X'81'-X'89', j-r X'91'-X'99', s-z
  X'A2'-X'A9'.

  Like the aliases they join, they return `-1`/`0`/`1` where glibc returns the
  difference. Only the sign is specified, so both conform; this keeps them
  consistent with what was already in the archive. External names are
  truncated to eight characters as always — `STRCASEC` and `STRNCASE`.

  `test/mvs/tststrci.c` covers all four, spans all three EBCDIC letter runs,
  and opens with a control that asserts the run is EBCDIC at all — on a host
  the fold goes through the host's ASCII table and cannot observe the property
  under test.

- **`long long` arithmetic links: the libgcc helpers cc370 calls (#187,
  #190).** cc370 emits calls for 64-bit `*`, `/`, `%`, unary `-`, float/double
  <-> `long long`, `__cmpdi2` and the bit builtins, and libc370 defined none
  of them, so any such program failed in ld370. Now present: `@@MULDI3`,
  `@@DIVDI3`, `@@MODDI3`, `@@UDIVDI`, `@@UMODDI`, `@@NEGDI2` (PR #191) and 16
  more, `@@FIXDFD`, `@@FIXSFD`, `@@FXUNDF`, `@@FXUNSF`, `@@FLTDDF`,
  `@@FLTDSF`, `@@CMPDI2`, `@@POPCSI/DI`, `@@PARTSI/DI`, `@@FFSDI2`,
  `@@CLZSI2/DI2`, `@@CTZSI2/DI2` (PR #194). The conversions follow cc370's
  inline 32-bit ones (toward zero, modulo on overflow). A zero
  divisor abends **S0C9**. Measured on MVS: 1138/1138 and 565/565.
  **Requires cc370 at `f3f7e21` or later** (cc370#470) for the eight
  new names; an older cc370 still calls
  `@@FIXUNS`/`@@FLOATD`/`@@POPCOU`/`@@PARITY` and still fails to link. Not
  fixed here: `ll / <constant>` (cc370#467) and `<<` dropping bit 63
  (cc370#468).
- **`intptr_t` / `uintptr_t` in `<stdint.h>` (#187).** With `INTPTR_MIN/MAX`,
  `UINTPTR_MAX` and `PTRDIFF_MIN/MAX` for `__MVS__`. `intptr_t` is `int`, the
  type it had through the generic fallback, so no consumer's type changes (PR
  #191).
- **`r+`, `w+` and `a+` (#189, PRs #207, #208).** `fopen()` refused every `+`
  mode with `NULL`. A `+` stream now reads and writes one DD, turned round by
  the new `__fpswt()` (CLOSE, OPEN the other way, skip to the position) and
  never truncating on a switch. `r+`/`w+` overwrite in place through UPDAT
  with the record length fixed: on F a `'\n'` blank-fills the rest of the
  record; anything that would lengthen a record, or shorten a V/U one, is
  `EOPNOTSUPP`. `a+` counts the size at open, so `ftell()` is the size. `+`
  needs DASD: SYSOUT, terminal and tape answer `EINVAL`; a member is read-only
  on `r+` and after `w+` has switched to reading. `sizeof(FILE)` stays 192.
  Measured on MVS: 15/15 and 9/9. Direction switches and backward seeks
  past the buffer are O(n) (#206).
- **`<wchar.h>`, types and macros only (#195).** `wint_t`, `WEOF`,
  `WCHAR_MIN/MAX`, `WINT_MIN/MAX` (also in `<stdint.h>` for `__MVS__`). The
  wide-character functions are not implemented and not declared, so a call
  fails at compile time rather than at link time (PR #214).

### Changed
- **`wchar_t` is `int` and `ptrdiff_t` is `long`, as cc370 has them (#195,
  #213).** libc370 declared `wchar_t` as `char` and `ptrdiff_t` as `int`, so
  `wchar_t *p = L"abc"` failed under `-Werror` and `%td` of a `ptrdiff_t`
  warned. Both now come from `__WCHAR_TYPE__` / `__PTRDIFF_TYPE__`;
  `PTRDIFF_MIN/MAX` become `LONG_MIN/MAX` with the same values. `mbstowcs()` /
  `wcstombs()` (a `strncpy` through a cast) now convert element by element,
  `mblen()` / `mbtowc()` follow C99 7.20.7, and `wcstombs()` / `wctomb()`
  refuse a value that does not fit a byte. Measured on MVS: 18/18, old libc 11
  of 18 failed (PR #214). **Contract change:** header types change
  (recompile); code that passes a `wchar_t` buffer to `mbtowc`, `mbstowcs` or
  `wcstombs` must be recompiled, the element goes from 1 byte to 4.

### Fixed
- **`fclose()` reports an out-of-space on the last block (#182).** The final,
  short block of a data set is written by `@@ACLOSE`, not by `@@AWRITE`, and
  `@@ACLOSE` ended `RC=0` whatever happened while `fclose()` returned 0 on
  every path. Since #176 turned the SD37 into a return, a data set with room
  for its full blocks but not for the short one lost its tail and the caller
  was told it had succeeded. `__aclose()` now returns the final write's rc
  (12 out of space, 8 I/O error) and `fclose()` returns **`EOF`** with
  `errno` `ENOSPC`/`EIO` — also when the flush it runs first fails, as C99
  7.19.5.1 requires. Measured on MVS with `test/mvs/tstclspc.c`: a
  `TRK(1,0)` FB 80/800 data set holds 190 records in full blocks; 191–194
  still fit and close with 0, 195–199 lose the short block — red before the
  fix (`fclose()` 0), green after it (`EOF`, `errno` 28). **Contract change:** a
  caller that ignored `fclose()`'s result is unaffected; one that treats any
  non-zero as fatal now sees the failure it was previously not told about.
- **`racf_auth()` no longer needs APF (#197).** It issued `MODESET
  KEY=ZERO,MODE=SUP` before every `RACHECK`, so any caller that was not
  APF-authorized ended **S047**, although SVC 130 answers from problem state
  — RAKF's SVC entry has no `TESTAUTH`. It now enters supervisor state only
  when the caller is APF-authorized and not already there. Otherwise it issues
  the `RACHECK` as it is. That opens the call to any unauthorized caller,
  such as a REXX interpreter's `RACCHECK()` function.

  The same fix closes a second defect: the unconditional `MODESET
  KEY=NZERO,MODE=PROB` afterwards put a caller that was **already in
  supervisor state** back into problem state.

  `test/mvs/tstracun.c` runs unauthorized from a library outside IEAAPF00 and
  compares `racf_auth()` against a raw SVC 130 on seven resources. Measured
  on MVS: red on the old library (S047 without APF, supervisor state lost),
  green on the new one (CC 0000). Authorized callers (servers running APF)
  go through the same `MODESET` as before, and cell (3) checks that they
  return in problem state.

  The probe also answers the question #197 left open. A **foreign ACEE**
  passed in the parameter list from problem state is honoured: for another
  user, `LIBC370.TSTRACMX.ALLOW` answers 0, the runner's own identity gets 8. So
  RAKF trusts the plist ACEE of any caller. The `MODESET` protects nothing
  there, and on this platform it never did.
- **`strncmpi()` called `tolower()` out of line, twice per character (#183).**
  The TU was missing `#include <ctype.h>` (`stdio.h` → `clibio.h` does not
  pull ctype in), so `tolower` was an implicit declaration resolving to the
  real function in `src/clib/tolower.c` instead of the `__tolow[c]` macro.
  Measured on the generated assembler: two `L 15,=V(TOLOWER)` call sequences
  per loop iteration before, one `L 2,=V(@@TOLOW)` table load after. It
  computed the right answer throughout — and it had no in-tree caller and no
  test, so nothing had ever run it.

  #39 **did** count this one — its sweep compiles every TU with `-Wall`, and
  the pre-change `strncmpi.c:20` warns on the implicit `tolower`. What omits
  it is the issue's *abridged* table, which shows the top 8 of 27 distinct
  functions (109 of 135 instances) in the missing-`#include` bucket. The
  three TUs touched here now compile clean under `-Wall -Werror` (the
  pre-change `strncmpi.c` does not — RC 1), which is #39's step 3 on a
  three-file scale; the step itself is turning `-Wall` on in
  `sdk/mklibc.py`, and that is still open.

- **A second concurrent open of a spool SYSIN is refused, not fatal (#184).**
  `@@start` opens `dd:SYSIN` as stdin, so every user
  `fopen("dd:SYSIN", "r")` is inherently a *second* open of that DD — and on
  an instream `DD *` that is the one thing JES2 will not do. `HOSOPEN`
  dispatches by data set **type** — `HO000` internal reader, `HO100` `'SI'`,
  `HO200` `'SO'`, `HO300` `'PS'` — and an instream SYSIN reaches `HO100`,
  whose non-XBM path falls through `HO107` into the process-SYSOUT open
  code. Plain SYSOUT never comes here; `HO200` is its own block:

  ```
  HO300    DS    0H
           L     R0,SDBDEB        GET SDB'S DEB POINTER.
           LTR   R0,R0            IF NO DEB, DATA SET IS
           BZ    HO110            CLOSED.  GO OPEN IT.
           TM    SJBFLG1,SJB1XBM  IF OPEN ALREADY AND XBM,
           BO    HORET            IGNORE OPEN.
           B     HOERR            NOT XBM BUT OPEN - ERROR.
  ```

  Already open and not an execution batch monitor is `HOERR`,
  unconditionally, and `IEC141I 013-C0` follows. It cannot be negotiated,
  so `fopen()` now answers **`NULL` with `errno == EBUSY`** instead of
  letting the address space die in OPEN — the shape #149 and #176 already
  established here (`clibio.h` cites #149 for exactly this).

  Three bounds on the new check, each measured rather than reasoned about,
  because getting any of them wrong breaks code that works today:

  | bound | why | measured |
  |---|---|---|
  | an input **DD**, not an input open | `HOCSETUP` dispatches on `DSNDSTYP`, the data set's *type*, not the DCB's mode — so the check tests the direction of the stream **already holding** the DD | `fopen("dd:SYSPRINT","r")` with stdout open **succeeds** pre-fix; keying on the caller's mode alone would have broken it |
  | spool only | a real data set tolerates two concurrent DCBs | two concurrent opens of a real data set |
  | already open here | the refusal is about a live DEB | `fclose(stdin)` then `fopen` succeeds **and re-reads from the top** |

  **The direction test is a proxy, not JES2's discriminator**, and the
  entry should say so: the exact one is the JFCB's SYSOUT class, which is
  out of reach before the OPEN. The residual cell is an `'SO'` data set
  opened for read while another *read* stream already holds it — rarer
  than the one measured, unmeasured, and the same class. The check also
  only sees DCBs `fopen()` registered, so a DD opened by `ropen()` (which
  calls `__aopen()` directly), by assembler, or by a subtask with another
  GRT still abends exactly as before.

  **`fseek()` can reach it, but not the way it first looked.** A backward
  seek reopens the DD through `__reopen()`, which calls `fopen()` while
  the old `FILE` is still open and registered — so the refusal would
  surface there. It does not, for a small instream SYSIN: `@@fseek.c`
  satisfies a seek whose target is still inside the current buffer
  without reopening anything, and one block of instream data is entirely
  inside it. Measured, after the check asserting the opposite went red.
  What a seek *past* the buffer does on a spool SYSIN is **not** measured
  — it needs more than one block of instream data, and nothing here
  produces that.

  That third bound is worth keeping: instream SYSIN is **not** one-shot, only
  non-concurrent — so `fclose(stdin)` is a one-line workaround that needs
  no staging data set.

  **The discriminator is not the one the header points at.** `ieftiot.h`
  documents `TIOESYIN` (X'04') as "ENTRY FOR SPOOLED SYSIN DATA SET", and
  that bit is **never set** on MVS 3.8j (measured). A spool DD is marked
  by `TIOESSDS` (X'02'), the VS2 meaning of the same byte. A check written
  from the comment would compile, run and never fire; `test/mvs/tstsysin.c`
  asserts the right bit so it cannot rot silently.

  `fopen()` also preserves `errno` across its failure cleanup now. The
  quit path calls `fclose()`, which runs a teardown of its own, and there
  was no reason its last step should be what the caller reads.

  Gate: CC 0000, 13/13 in the spool step and 8/8 against a real data set.
  **Proven red by the same source linked against the pre-fix libc**, which
  abends `IEC141I 013-C0,IGG0199G,TSTSYSOL,SPOOL,SYSIN` — the issue verbatim.

  **One system.** All of this was measured on one MVS 3.8j system; nothing
  has run on TK5, and the SO-read cell the fix now deliberately permits has
  been measured on that one system only.
- **`<stdint.h>` limits and constant macros conform to C99 7.18 (#188,
  #192).** The signed minimums were wrong: `INT32_MIN` was **+2147483648**
  (unsigned), `INT64_MIN`/`INTMAX_MIN` warned on every use (an error under
  `-Werror`), `INT8_MIN`/`INT16_MIN` were casts unusable in `#if`, and
  `INT_FAST8_MIN` expanded to the typo `IN_LEASTT8_MIN` (PR #193). Then (PR
  #196): `INT32_MAX` is `long` (was `int`),
  `INT8_C`/`UINT8_C`/`INT16_C`/`UINT16_C` expand to their argument instead of
  casting, `SIZE_MAX` and `SIG_ATOMIC_MAX` work in `#if`, and `SIG_ATOMIC_MIN`
  exists. Compile-time gate `test/mvs/tststdint.c`; all TUs under `src/`
  compile to byte-identical `.s`. **Contract change:** `INT32_MIN` changes
  sign in C and `#if`; `INT32_MAX` changes type; `INT8_C(200)` is 200, was
  −56.
- **An empty line on a text stream is a record (#199).** `fputs("a\n\nb\n")`
  wrote two records: every `'\n'` went to `__fflush()`, which returns at once
  on an empty buffer. A newline now goes to the new `__fflnl()`, which always
  writes a record (FB: LRECL blanks, VB: RDW only, U: one blank); `fflush()` /
  `fclose()` still write nothing when nothing is pending. Measured on MVS,
  red before and green after (PR #201). **Contract change:** blank lines now appear in
  SYSOUT, logs and data sets. Not covered: a TSO terminal (TPUT skips length
  0) and `printf("\n")` (cc370#477).
- **A write stream knows its position (#200).** `ftell()` on a writer counted
  from the last record (`0 0 2` for `3 6 8`), and `fseek()` on a writer
  re-served the stale buffer and wrote it again (`ABCxy`). A flush no longer
  resets the position, and a stream not open for reading can only seek to
  where it is; anything else is `-1`, `errno` `ESPIPE`, stream untouched,
  error indicator not set. Measured on MVS, red before and green after (PR
  #202). **Contract change:** `rewind()` / a backward `fseek()` on a `"w"`
  stream used to reopen and truncate (when the position was non-zero); it now
  fails with `ESPIPE`.
- **A stream refuses the wrong direction with `EBADF` (#189, PR #203).**
  `fgetc()` on a `"w"` stream returned bytes from the write buffer — the next
  write produced the record `LL2` — and past the buffer abended
  S400. `fputs()` on `"r"` returned success with `errno` 0. `fgetc`, `fread`,
  `fputc` and `fwrite` now answer `EOF`/`0` with `errno` `EBADF` (`fgets`
  `NULL`); the error indicator is not set, so the stream keeps working in its
  own direction. Measured green on MVS. **Contract change:** new `EBADF` returns.
- **`"a"` appends: OPEN EXTEND (#198).** `fopen(…, "a")` opened for OUTPUT and
  overwrote the data set, by name, as a member or through `DISP=OLD`; only
  `DISP=MOD` appended. `"a"` now opens EXTEND (OUTPUT on SYSOUT and
  unit record, as before). An existing PDS member cannot be extended by BPAM
  and is refused with `NULL`, `errno` `EOPNOTSUPP` (45), left intact; a new
  member is created. `@@aopen` returns -45 for EXTEND on a member named in the
  JCL, which otherwise abended SB14-04 at CLOSE. Measured on MVS, red before
  and green after (PR #205). **Contract change:** existing data sets are
  appended to instead of replaced; `"a"` on an existing member fails;
  `ropen()` reports `errno` 45. Appending to a member is #204.
- **Two defects in the inherited UPDAT assembler (#189, PR #208).** `@@aread`
  rewrote the block before every record, so the read after a rewrite skipped
  the rest of the block and a second rewrite in the same block landed on the
  wrong record; `@@atrout` skipped the rewrite of a block read to its end.
  Both changes are guarded by UPDAT (`OPENCLOS == X'84'`); every other open
  mode takes the old path. Measured green on MVS.
- **`ftell()` on FB text writers counts in the byte view, and `fseek()` on a
  writer no longer flushes (#189, PR #207).** A writer now counts LRECL + 1
  per F record, as a reader of the same data set does: after `"L1\n"` on FB 80
  `ftell()` is **81** (was 3). `fputs("AB"); fseek(fp, ftell(fp), SEEK_SET);
  fputs("CD\n")` gives one record `ABCD`, not `AB` and `CD`. **Contract
  change:** both. The `3 6 8` of #200 still holds for V.
- **`printf` accepts `z`, `t`, `j` and `hh` (#211).** `__examin()`, the parser
  behind the whole printf family, did not know them, so the argument was never
  consumed and every later conversion read one slot early — `"%zu|%s|%d"`
  crashed on the host. `z`/`t` map to `l`, `j` to `ll`, `hh` is skipped like
  `h`. Measured on MVS: 24/24, pre-fix libc 20 of 24 failed (PR #212).
  **Contract change:** new modifiers accepted. A negative `%jd` prints its
  two's complement, as `%lld` does; `@@prtfx.c` is not changed.
- **`tsocmd()` works from a TSO command processor (#210).** `ppacppl` was read
  by `tsocmd()` and written by nobody, so `tsocmd()` — and `ispexec()` through
  it — always returned 8 (`No CPPL`). `__start()` now records R1 as the CPPL
  when `GRTFLAG1_TSO` is on and the CPPL's PSCB word equals `ppapscb`.
  Measured on MVS: red before, green after, the caller unchanged after the
  call, also in TSO foreground and from the link list (PR #217). **Contract
  change:** `tsocmd()`/`ispexec()` now LINK instead of returning 8.
- **A CPPL fills four words of `grtptrs`, not ten (#218).** A CPPL has no VL
  bit, so `__start()` copied ten words, six from past the list — for a command
  LINKed by `tsocmd()` the caller's stack frame. A recognised CPPL now gives
  exactly four; a PARM list is not read past its VL bit. A list with neither
  still gets ten. Measured on MVS, red before and green after (PR #219).
  **Contract change:** `n` is 4 for a CP.
- **`__dblcvt()` rounds correctly past 14 digits and never rounds zero
  (#209).** The rounding cap used `DBL_MANT_DIG` (14, in hex digits), so every
  `%e`/`%f`/`%g` of 14+ digits got a fixed 5e-15 (`…4884981308350688`), and
  `%.20g` of 0.0 printed `0.000000000000005`. The cap is now 17 decimal digits
  and 0.0 is not rounded; 6.96 million conversions below the old cap are
  byte-identical. Measured on MVS: red before (22/29 failed), green after
  (PR #223). **Contract change:** output digits change at precision 14+.
- **`__dblcvt()` overflowed its caller's buffer (#222).** It `strcat`ed into
  buffers it had no length for (`numbuf[50]`, `work[80]`, its own
  `work[125]`): `%f` of ≥ 1e41, `%.100f`, `%90f` and more overwrote the stack
  — S0C4, PSW `078D1000 00F0F0F6`. It now takes the buffer size and
  truncates (NUL always, exponent before fraction digits, digits before
  padding); `numbuf` is 96, `__examin()`'s `work` 128. 30,596,740 conversions
  unchanged; measured green on MVS, 59/59 (PR #224). **Contract change:**
  `__examin()` output longer than 126 characters (127 with sign) is cut off:
  `%.150f` of 1.0 gives 126 characters, not 152. `__dblcvt()`'s signature
  changed, but it is internal and in no header.

## [1.0.6] - 2026-09-13

### Fixed
- **A stream that has failed now fails fast, and `ferror()` answers 1 (#149).**
  Nothing in stdio ever looked at `_FILE_FLAG_ERROR`. `__fgetc()` and
  `__fread()` tested only `_FILE_FLAG_EOF`; `__fwrite()` and `__fputc()`
  tested neither. So after a failed write the bytes were still accepted into
  the FILE buffer, flushed into a WRITE that failed again, and dropped at
  `@@fflush.c`'s `reset:` label — **accepted and then silently discarded.**

  Measured on MVS (`test/mvs/tstnospc.c`): after one `ENOSPC`,
  **46 of the next 50 `fwrite()` calls returned the full 80 bytes and not one
  of those records reached the disk.** The caller heard about 4 of 50. With
  the guards in place all 50 are refused at the call, and a refusal
  costs nothing measurable — 253 µs against a 254 µs measurement floor, where
  a write that actually reached the access method and failed cost 2355 µs.

  The guard is on every entry that can reach the access method, `__fputc()`
  included: `fprintf()`, `fputs()` and `puts()` go through it and not through
  `__fwrite()`. `clearerr()` lifts it — which is what a caller who has freed
  space must now do.

  A refused call reports the **right** `errno`, not a stale one. The FILE
  keeps no errno of its own and `@@AWRITE` clears `IOSFLAGS` before it
  returns, so a new flag bit **`_FILE_FLAG_ENOSPC` (0x0004)** records which
  error set `_FILE_FLAG_ERROR`. It rides along with it and is cleared
  wherever it is cleared. No change to the 192-byte FILE.

- **`ferror()` and `feof()` as macros returned the raw flag value (#149).**
  The function forms return 1/0; the macros in `<stdio.h>` returned
  `flags & _FILE_FLAG_ERROR` — **2** — and `flags & _FILE_FLAG_EOF` — 1. So
  `if (ferror(f) == 1)` was **always false** for anyone who included
  `<stdio.h>`, and true for anyone who did not. Both macros now yield 1/0.

  `clearerr()` itself needed nothing: it has been in the tree since the
  initial commit (`48111ed`, 2024-09-12), contrary to what #149 states.

- **An out-of-space write is a return code now, not an ABEND (#176).**
  `@@AOPEN` plants an `EXLST` type **X'08'** exit. `IFG0554T` — the module
  named in the `IEC031I D37-04` line — scans the DCB exit list for it *before*
  it abends, and takes `R15=1` as "rewrite the format-1 DSCB from
  `DCBFDAD`/`DCBTRBAL`, clear the unit-exception bits in every IOB, drop FEOV,
  and return to the access method to drive the caller's SYNAD with **output
  error, no space available**". libc370 has had that SYNAD stub since #147, so
  the condition now arrives the same way an uncorrectable I/O error already
  did: `@@AWRITE` answers a nonzero rc, `__fflush()` sets `_FILE_FLAG_ERROR`
  — **and reaches its `reset:` label**, so `fp->upto` is cleared and there is
  nothing left for `fclose()` to re-drive.

  **What it fixes is worse than the abend it removes.** Re-driving a WRITE
  against a DCB that had taken an x37 did not fail cleanly: `0x0C4` on one run
  and `0x0C6` on another for identical code, i.e. a wild store. `__fabandon()`
  (#168) gave a caller a way out, but nothing stopped the re-drive itself and
  one path reached it with no API call at all — `@@exit.c` walks
  `grt->grtfile` at program termination and `fclose()`s every survivor **with
  no ESTAE around it**, so a caller that recovered an x37 and simply
  returned from `main()` took the program check in teardown.

  `errno` is **`ENOSPC`, not `EIO`**: `@@AWRITE` answers **12** for the x37
  case and 8 for a SYNAD error, because out of space is the caller's problem
  to solve and a bad track is not — a distinction #149 needs.

  **Planted only where #147 plants SYNAD.** `IFG0554T`'s own header: *"if a
  SYNAD address is not present or if CLOSE called EOV, an 001 abend is
  issued"*. EXCP tape has no SYNAD and gets an inactive last entry instead,
  keeping today's behaviour. Two consequences worth knowing: at **16 extents
  or on a VIO unit MVS skips the exit and assumes `RC=1` anyway**, so libc370
  already behaved this way for a 16-extent x37 — the exit makes the
  one-extent case behave like the sixteen-extent case. And an x37 raised *by
  CLOSE* becomes S001 rather than D37, which is strictly less legible; the
  exit cannot tell from the DCB that it was entered from CLOSE. It was
  measured that CLOSE with nothing pending completes on `TRK(1,0)`, so that
  path is not observed.

  Not the X'11' ABEND exit, which is what #176 said first. `EXLDCBAB EQU
  X'11'` appears exactly once in the whole MVS 3.8j source tree — its own
  definition in `IHAEXLST` — and no module consults it.

  **Measured** by `test/mvs/tstx37.c` + `jcl/tstx37.jcl` on MVS:
  **CC 0000, 5/5 PASS, and no `IEC031I` line in the job log at
  all.** The probe uses no `try()` on purpose — if the exit is not taken the
  step abends and the log says so louder than any return code. The same
  `TRK(1,0)` that produced a D37 at 200 records now stops at 200 with
  `ferror()` set, `errno` 28, `fclose()` returning and `remove()` answering 0.
  Cross-checked by `test/mvs/tstfabnd.c`, whose three steps each
  produced a D37 before the change, now report `try()` = 0 and "nothing to
  measure" in all three.

## [1.0.5] - 2026-09-13

### Added
- **`__fabandon()` — close a `FILE` whose last write failed (#168).** There was
  no way to do it. `fclose()` flushes before it closes, so when the pending
  block is exactly what could not be written, the close re-drives the failing
  WRITE, abends in turn, and never reaches `__fpfree()` — the DD stays
  allocated for the life of the job. Measured on MVS 3.8j with an FTP
  server receiving an upload into `SPACE=TRK(1,0)`: the D37 was recovered,
  the CLOSE after it abended, freeing the DD answered 4 and IDCAMS could
  not scratch the data set (RC 8).

  `__dsfree()` on that DD by name answers 4 — the DCB the failed CLOSE left
  open still holds the allocation — and IDCAMS `DELETE` answers 8 from inside
  the address space. So a server that runs out of space on a data set it
  created can neither clean it up nor let its user clean it up; only a restart
  releases it. The server's `DISP=(NEW,CATLG,DELETE)` says the partial
  should be scratched, and it could not be.

  `__fabandon(FILE *)` discards the buffer instead of flushing, tells the DCB
  there is nothing pending (`__adisc()`, new `asm/@@adisc.asm`, clearing
  `IOFLDATA`, `IOFLSDW`, `BUFFCURR` and `KEPTREC`), and issues CLOSE under an
  ESTAE so a close that fails anyway is *reported* rather than propagated:
  `0` clean, `>0` the `0x00sssuuu` abend code, `-1` not a FILE, `-2` the DD
  would not unallocate, `-3` ESTAE CREATE failed and nothing was torn down.

  **It is the C buffer, not the DCB buffer.** The issue put the crux on the
  DCB's own state, and for that server's shape — `fopen(dsn,"wb")`, the buffered
  `__fputc` path — it is not: the D37 fires inside `__awrite()`, the caller's
  ESTAE unwinds *through* libc370, `@@fflush.c`'s `reset:` label never runs,
  and `fp->upto` still points past the block that failed. `@@ATROUT` clears
  `IOFLDATA` *before* the WRITE, so the DCB side is already quiet and
  `@@ACLOSE`'s opening `FIXWRITE` is a no-op. `__adisc()` is there for the
  general case — a caller that abandons while a *good* partial block is
  pending means "write nothing more" — and `try()` for the one `@@ATROUT`
  cannot help with: BSAM CLOSE writes the EOF mark and can reach EOV, and a
  `_FILE_FLAG_RECORD` caller has no C buffer at all.

  **It cannot be a smarter `fclose()`.** An x37 is an ABEND, not a SYNAD
  condition, so `@@fflush.c` never sets `_FILE_FLAG_ERROR` (#147) and after the
  caller's ESTAE recovers the FILE looks healthy to the library. Only the
  caller knows — see #149, which this does not resolve.

  **The stale ENQ goes too.** `lock()` is ENQ `RET=HAVE` keyed on the pointer
  value. The abended `fwrite()` held the FILE lock and the ESTAE retry never
  DEQ'd it, so `lock(fp,0)` answers 8 and `fclose()` — reading that as "an
  outer caller owns it" (#145) — leaves a CLIBLOCK ENQ standing on storage it
  then `free()`s. `__fabandon()` DEQs unconditionally: there is no legitimate
  outer holder of a FILE being destroyed.

  `fclose()`'s teardown tail moved to `__fpterm()` so the #145/#147 lock
  sequence exists once. `fclose()` is otherwise unchanged, and pulls neither
  `___try` nor `@@ADISC` — those come only with `@@FABAND`.

  **Verification:** `test/host/tstfabnd.c`, 32 checks, 4 of them red without
  the fix, compiling the real `fclose.c`/`@@faband.c`/`@@fpterm.c`; it also
  asserts the old `fclose()` behaviour as a control so the rest cannot go
  vacuous. `test/host/tstfcls.c` (#147) still passes and gained a check that
  `__aclose()` precedes `__fpfree()` (#167's ordering).

  **Measured on the target**, `test/mvs/tstfabnd.c` + `jcl/tstfabnd.jcl`, run
  on MVS 3.8j: **job CC 0000**, all three steps green, 7/7
  checks pass, no teardown abend. `TRK(1,0)` takes 200 records of 80
  before the D37; then

  | step | | |
  |---|---|---|
  | GREEN — `__fabandon()` | rc **0** | `remove()` rc **0** |
  | SPLIT — `fflush()` alone | rc `0x000C4000` | `__aclose()` alone rc **0**, `remove()` rc **0** |
  | FCLOSE — `fclose()` alone | rc `0x000C4000` | `__dsfree()` rc **4** |

  Three things the run settled rather than assumed. **CLOSE with nothing
  pending completes** on a data set that is out of space — the open question
  in #168, since BSAM CLOSE writes the EOF mark and can reach EOV; `try()`
  stays, but point 3 did not fire on this system. **Point 1 is the crux on the
  target too**: the SPLIT step drives the two halves of `fclose()` under
  separate `try()`s and it is the *flush* that abends, with CLOSE straight
  afterwards clean and the DD gone. And **the second abend is a program check,
  not a second D37** — `0x0C4` and `0x0C6` both appeared across runs for
  identical code, so re-driving a WRITE against a DCB that has taken an x37
  walks into wild storage rather than failing cleanly. That is worse than #168
  assumed, is not fixed here, and is filed as **#176**: nothing stops the
  re-drive itself, and `@@exit.c` reaches it with no ESTAE for a caller that
  recovers an x37 and simply returns from `main()`.

  One case per step, and not for cosmetics: run together, the FCLOSE step's
  after-the-fact rescue answered `-2` and left the DD; run in its own address
  space it answers `0`. Every `-2` had both an earlier IDCAMS DELETE and an
  earlier D37 in the same step and nothing measured separates the two, so the
  probe reports that path and never asserts it. The supported use is
  `__fabandon()` **instead of** `fclose()`.

- **`fopen()` can ask for RLSE (#167).** `__txrlse()` built a `DALRLSE`
  (`0x000D`) text unit and had a prototype in `svc99.h`, and nothing in the
  library ever called it — so there was no way to get unused space released for
  a data set written through `fopen()`. A new mode-string keyword now does:
  `fopen(dsn, "wb,rlse")`, in the same comma-separated family as `record` and
  `bsam`. `__fpmode()` turns it into `_FILE_FLAG_RLSE` (`0x0040`) and both
  `__fpold()` (DISP=OLD) and `__fpnew()` (DISP=NEW) add the text unit.

  **Opt-in, not a default.** Setting `DALRLSE` unconditionally would change
  what `fclose()` does for every caller's output `fopen()`, and would take
  an append-mode writer's primary extent at every close. Nothing changes for a caller that does not spell the keyword.

  **It has to be here and not in `__dsalc()`'s opts parser.** RLSE is honoured
  at CLOSE of the DCB opened against the DD that carried it, and `fclose()`
  runs `__aclose(fp->dcb)` *before* `__fpfree()` drops the DD — so the DD
  `fopen()` allocated is still there when CLOSE looks. A caller that
  allocates with `__dsalcf()`, `__dsfree()`s that DD and *then* `fopen()`s the
  data set by name — an FTP server receiving an upload does exactly that —
  would find an `RLSE` keyword in the opts parser a no-op.

  **Skipped for a PDS member.** `fopen()` tries `__fpshr()` for a member and
  falls through to `__fpold()` when that fails, so the flag alone would put
  partial release on a PO data set and take the space the next member needs.
  A caller may pass `"wb,rlse"` unconditionally; the library decides.

  **Two sharp edges, neither a defect.** `"ab,rlse"` is accepted, and accepting
  it is right — but RLSE is what makes append expensive: every `fclose()` gives
  the unused primary back, so every following append takes a *secondary* extent,
  and a data set gets 16 of those on one volume. Repeated append-with-RLSE walks
  it into an x37 at a rate the caller chose. And the keyword only reaches the two
  functions that allocate: `__fpmode()` sets the flag for any mode string
  containing `rlse`, but it is silently ignored for read opens (`__fpshr`),
  `&TEMP` data sets (`__fptmp`), `DD:ddname` (nothing is allocated), `*` SYSOUT
  (`__fpstar`), and — deliberately — PDS members.

  **Measured, not just built.** Host red/green: `test/host/tstfprls.c`, 51
  checks, 8 red before the fix, links the real `@@fpmode.c`/`@@fpold.c`/
  `@@fpnew.c` and captures the text unit array at the SVC 99 call. Target:
  `test/mvs/tstfprls.c` + `jcl/tstfprls.jcl`, run on MVS 3.8j
  (CC 0000, a 3350 volume, 30 tracks/cylinder). Each case allocates
  `TRK(30,5)`, writes one record, closes, and adds up the extents from the
  format-1 DSCB:

  | case | mode | tracks |
  |---|---|---|
  | `__dsalcf()` TRK(30,5) | — | 30 |
  | DISP=OLD | `"wb"` | 30 |
  | DISP=OLD | `"wb,rlse"` | **1** |
  | DISP=NEW | no `rlse` | 30 |
  | DISP=NEW | `",rlse"` | **1** |

  **SVC 99 accepts `DALRLSE` with DISP=OLD and no space keys** — the FTP
  server's exact shape — and CLOSE released 29 of the 30 tracks. No SVC 99 returned nonzero.
  That matters beyond the feature: had SVC 99 rejected it, `__fpold()` would
  fail, `fopen()` would fall through to `__fpnew()`, DISP=NEW on an existing
  cataloged data set would fail too, and the caller would lose the open
  entirely. There is no graceful degradation in this design, and it does not
  need one.

  Filed off the back of this work, none of it fixed here: #171 (overlapping
  `strcpy(p, p+1)` in `@@fpnew.c`/`@@dsalc.c`), #172 (`__fpnew()` sends no UNIT
  text unit) and #173 (`clibdscb.h` models DSCB key presence inconsistently —
  `struct dscb4` is unusable with `__dscbv()`, which the probe and
  `@@listds.c:192` work around identically and independently).

- **A probe for finding the JES2 checkpoint and spool without a DD
  (`test/mvs/tstjesda.c` + `jcl/tstjesda.jcl`, research for #142).** No library
  change: it exists to settle whether `jesopen()` can drop its two `DD:`
  literals (`jesopen.c:37,47`), and the answer it measured is *not from a
  constant*. `__cpopen()`/`__jsopen()` already dynalloc when the argument is not
  `DD:`, so the plumbing is there — but JES2 builds the data set names from
  `$DSNPRFX` (an init parameter, default `SYS1`) plus the assembled literal
  `.HASPACE`/`.HASPCKPT`, and that prefix lives in the HCT, which is unreachable
  from another address space. Recorded here so the next attempt starts from the
  measurement rather than from the issue text, which understates it both ways.

### Fixed
- **`recv()` capped its X'75' chunk at 4096, and only 256 or less is safe
  against a restarted copy (#154).** X'75' moves data in 256-byte segments and
  the instruction is restartable: a page translation exception on the guest
  buffer is nullifying, so MVS resolves the page and the instruction runs again
  from the top. The guest side resumes correctly — R1 holds the bytes remaining
  and the base register was advanced before the exception — but the host side
  has nothing to resume from. Upstream `x75.c` recomputes its pointer from
  `map32[R2]` on every entry and R2 is a slot index that never advances, so the
  remaining bytes are copied from the **start** of the host buffer to the
  already advanced guest address. A single segment is atomic against that
  exception (`vstorec()` resolves both page addresses through `MADDRL` before
  either `memcpy`), which makes 256 or less immune by construction: it either
  faults having moved nothing, where resuming from the start is correct, or it
  completes. The defect needs one **completed** segment before the fault, which
  is why every larger cap looked like a fix and then failed — 4096 here since
  `cd43a70`, then 2048 in a caller, which failed five days later, after which
  that caller went to one byte per `recv()` and has stayed there.
  The comment being replaced blamed a dyn75/Hercules buffer-size limit that
  does not exist; the symptom it recorded was real and is that replay.
  Measured red/green under a forced fault by `test/mvs/tst75rst.c`
  (`jcl/tst75rst.jcl`), and independently sighted in production: a
  4577-byte ASCII upload to an FTP server came out as stream bytes
  `[0:2560] + [0:1536] + [4096:4577]` — first bad byte a multiple of 256, tail
  a clean replay of the buffer head, total length exactly right.
  Cost is 16x more X'75' pairs than at 4096 and still a large net win against
  the single-byte reads some callers use today; they can now go back to
  bulk reads. The cap is **permanent**: no return value, status bit or function code
  lets a guest tell a patched emulator from an unpatched one, so it can never
  be raised again on the strength of the host-side fix
  (SDL-Hercules-390/hyperion `4675e7e1`, merged 2026-09-06), which is
  independent of this change in both directions. `@@75send.c` has the same
  exposure and no cap at all; it is deliberately left alone, because capping it
  changes what every caller sees per call.

## [1.0.4] - 2026-09-04

### Changed
- **`sysmac/` has a written scope rule, and #155 is declined against it.** The
  criterion is *libc370 assembles it itself* — a member belongs in the mirror
  when one of the 27 hand-written `asm/*.asm`, one of the `.s` generated from
  `src/`, or one of the `maclib/` macros reaches it, directly or as an inner
  macro. Measured, that is **120 of the 123** members. #155 asked for four more
  (`SPIE`, `TIME`, `WTOR`, `PUTX`) because a COBOL-74 code generator's output
  expands them; all four declined. The need is real, but the consumer is a
  *generator* — it decides its own macro set from the code it is given, so
  nothing here can anticipate it, and adding on request turns libc370 into the
  ecosystem's system macro library by accident. A project that needs a macro
  libc370 does not use brings it along and points `as370` at it with `-I`,
  which is searched before the sysroot, so it keeps the other 120 for free.
  The rule and the three pre-existing exceptions are in
  `doc/consumer-notes.md`; one of them, `xctl.macro`, has a live user outside
  libc370, which is why `<sysroot>/macros` is treated as a published surface.

### Fixed
- **Four external names were each exported by two archived objects, and
  `@@ERRNO` was a data word in one of them (#151).** The archive namespace is
  flat and eight characters wide, and ld370 satisfies an autocall from the first
  object offering the name — so a duplicate lets link order decide which code
  runs, with nothing pinning it. Three were byte-identical twins from a mistyped
  filename (`@@lkrntf.c`/`@@lkuntf.c` and `@@lkrntr.c`/`@@lkuntr.c` both defined
  `__lkuntf`/`__lkuntr`; `josjobfr.c`/`jesjobfr.c` both defined `jesjobfr`) —
  harmless, since `diff` on the generated assembler shows no difference at all,
  but equally unpinned. `@@ERRNO` was not: `@@errno.s` puts the entry on a
  function prologue — the per-task accessor that `errno.h` reaches through
  `#define errno *(__errno())` — while `@@get@er.s`, a PDPCLIB leftover, puts
  the same `ENTRY` on a `DC F'0'`. Every `errno` in the ecosystem compiles to
  `L 15,=V(@@ERRNO)` + `BALR 14,15` (measured in `src/dyn75/@@75sock.s:66`), so
  resolving that reference to `@@get@er.o` would branch onto four zero bytes —
  `X'00'`, an invalid opcode — on the first `errno` touch, in every consumer at
  once. Latent, never observed: current link order picks `@@errno.o`, and
  nothing enforced that. `__get_errno()`, the only reason `@@get@er.c` existed,
  had no callers anywhere in the ecosystem. All four redundant modules are
  deleted and dropped from their `makefile`s; the surviving object of each pair
  is the one already being linked, so no consumer behaviour changes and the
  archive goes from 737 to 733 members with nothing else added or removed.
  Guarded by `sdk/dupscan.py`, now a build step: it reads the external names out
  of the generated `.s`/`.asm` for exactly the set being archived — skipping the
  `@@MAIN`-definers and crt startfiles that never become members, so it cannot
  cry wolf — and fails the build before the archive is written. Measured red
  (four duplicates before, one when `@@get@er.c` is restored) and green (733
  archived modules, matching the archive exactly), with a link probe pulling all four
  names into one load module: RC 0, and a negative control confirms ld370 does
  report an unresolved external when there is one.
- **An uncorrectable I/O error is `ferror()` + `errno EIO`, not ABEND S001:
  the DCBs `__aopen()` builds now carry a SYNAD exit (#147 item 3, PR #150).**
  Without one, the first genuine I/O error on any libc370 FILE — bad track,
  wrong-length record, device error — reached CHECK error processing as
  `NO ERROR HANDLING, (SYNAD), EXIT SPECIFIED` and killed the whole address
  space; that is what turned a server's corrupted PUT into a dead started
  task, and what #145 could narrow but not remove. The exit follows the `EOFR24`
  pattern: a six-byte stub copied into the per-FILE work area, planted in
  `DCBSYNA`, finding the new `IOSFLAGS` byte relative to its own entry
  address (R15) — no assumption about the other SYNAD entry registers, which
  is not a theoretical nicety: the first cut derived the work area from R1
  as the DECB address and, measured on 3.8j, the flag landed
  nowhere while the truncated block was delivered as *data*. `@@AREAD`
  answers RC=1 (EOF stays −1), `@@AWRITE` RC=8, and `__fgetc`/`__fread`/
  `__fflush`/`__fwrite` map both to `_FILE_FLAG_ERROR` + `errno EIO`.
  Measured red (S001-1 on a wrong-length READ, `try()`-caught) and
  green (CC 0000: zero garbage lines, `ferror()` set, `feof()`
  clear, `errno` EIO, `fclose()` survives, the program runs on) by
  `test/mvs/tstsynad.c`. The EXCP tape path keeps its own error handling; a
  write-side error is not forceable from an unprivileged batch probe and is
  covered by the shared mechanism plus regression (TSTIOLK/TSTSLK green on
  this libc). Fail-fast after `ferror()` and `clearerr()` are #149.
- **`puts()` is one critical section, `fclose()` tears down under the FILE
  lock, and DEQ keeps its scope bits — `sysunlock()` can actually release
  (#147 items 2, 1, 4; follow-up to #145).** `puts()` locked twice (public
  `fputs()` then `putc()`), so a concurrent printf could split the line at its
  newline; now the string and the `'\n'` go out under one hold. `fclose()`
  flushed, `__aclose()`'d the DCB and freed the buffer and the FILE with no
  hold on the FILE lock — a concurrent `vfprintf()` could slip in after the
  flush and PUT on a closed DCB or write through a freed buffer; the whole
  teardown now runs under the lock (released after `free(fp)`, safe because
  the lock rname is built from the pointer value). And `__enqdeq()`'s DEQ
  branch overwrote `pl.opt` with `ENQ_OPT_HAVE`, throwing the scope bits away:
  every DEQ went out SCOPE=STEP, so `sysunlock()`'s DEQ addressed a different
  resource than `syslock()`'s SCOPE=SYSTEM ENQ, answered rc=8, and the
  system-scope lock stayed held until task end — measured red on 3.8j
  (re-`syslock()` after `sysunlock()` still answered 8) and green
  after the one-character fix (`|=`: release and fresh re-acquire
  both rc=0). Guards: `test/host/tstiolk.c` case 9, `test/host/tstenqdq.c`
  (SVC parameter-list capture), `test/host/tstfcls.c` (teardown-under-hold
  ledger), `test/mvs/tstslk.c`. The fourth #147 neighbour — a SYNAD on the
  QSAM DCBs so a genuine I/O error stops killing the address space — remains
  open there and gets its own design and PR.
- **Concurrent `printf()` no longer corrupts the stream: `vvprintf()` holds the
  FILE lock across the whole line, and the public stdio wrappers never release
  a hold they did not take (#145, PR #146).** The `%s` and `%eEgGfF` branches of
  `vvprintf()` called the public `fputs()`, and every `__examin()` conversion
  emitted through `putc()` — the public `fputc()`; those wrappers lock and
  unconditionally unlock the same FILE, and with ENQ/DEQ RET=HAVE carrying no
  nesting count the inner unlock released the outer hold at the first
  conversion. Everything after it — including the `\n`-triggered
  `__fflush()`/QSAM PUT — ran unserialized, and two tasks printf'ing the same
  FILE merged and truncated records until the broken PUT surfaced as ABEND
  S001-1 with no SYNAD, or as a task silently wedged in
  the corrupted QSAM state. Both faces were reproduced on demand by
  `test/mvs/tstiolk.c` against the unfixed library (the literal
  `IEC020I 001-1` seconds after start; a writer wedged after 8 of
  400 lines, no abend at all), and the fix measured green (CC 0000:
  800 of 800 records intact, zero corrupt). Two layers: `vvprintf()` /
  `__examin()` write through `__fputs()`/`__fputc()` — which also removes an
  ENQ/DEQ SVC pair *per byte* on width/precision conversions — and `fputs` /
  `fputc` / `fwrite` / `fflush` / `fgets` / `fgetc` / `fread` / `fseek` /
  `ungetc` / `freopen` / `vvprintf` itself release only a hold they acquired
  (`owned = (lock(fp,0) == 0)`); the nested-acquire rc=8 contract that rests
  on was measured on 3.8j (TSTIOLK round 1) before being relied on.
  `fprintf()` was never affected — it formats into a private buffer and issues
  one `fwrite()`. Programs that printf from more than one task pick this up
  on their next relink against a released libc370.
  `test/host/tstiolk.c` pins the lock ledger per conversion path on the host
  (one acquire per line, every byte under the hold) as the fast regression
  guard.

## [1.0.3] - 2026-08-23

### Added
- **Prototypes for `sleep()` and `__tzset()`, in `time.h` (#70).** Both are
  built into the library and were declared in no header at all, so every caller
  compiled them as an implicit declaration — a warning under `-Wall`, an error
  under `-Werror` or a strict C99 front end, and in every case a call site whose
  arguments the compiler could not check. `tzset()` was already declared
  (`time.h:59`); `__tzset()`, the internal half it calls, was not. Both now sit
  next to it, and `src/clib/sleep.c` and `src/clib/@@tzset.c` include `<time.h>`
  themselves so the definitions are checked against the prototypes — the pattern
  `tzset.c` has carried since #5.
  **No known caller conflicts:** a program that declared either locally with
  the signatures taken from the definitions agrees with the header rather than
  colliding, and its local declarations can now go.
  This is a step of #39 — the goal there is `-Wall` in `sdk/mklibc.py`, which is
  the only thing that stops the class from coming back.
- **`DCBDSN=` allocates a data set modelled on an existing one (#123, PR
  #124).** SVC 99's `DALDCBDS` text unit — the JCL `DCB=(dsname)` model
  reference. MVS reads the model's DSCB and copies its DCB attributes into
  the new allocation; measured on 3.8j, `DSORG`, `RECFM`, `LRECL` and
  `BLKSIZE` all copied, CC 0000. The new `__txdcbd()` (declared in
  `include/svc99.h`) emits the text unit, and `__dsalc()` gains a `DCBDSN=`
  keyword — tested **before** the `DSN=` branch, because that dispatch runs
  on `strstr()` and `"DCBDSN="` contains `"DSN="`. Explicit DCB text units
  still override what the model supplies. Deliberately **not** `LIKE=`:
  `DCB=` does not copy SPACE, and key X'004B' is `DALRSRVS` in this era's
  table rather than `DALLIKE`, so callers keep supplying their own SPACE.
- **`jesircl2()` hands out the jobid before the close frees it (#118, PR
  #119).** The jobid arrives in `rpl.rplrbar` — an inline field of the RPL
  embedded in the `VSFILE` — as the answer to the `ENDREQ` inside the close,
  and the close frees that `VSFILE`. Every consumer that wanted the jobid was
  therefore reading freed storage.
  `jesircl2(vsfile, jobid)` copies the eight bytes out between the `ENDREQ`
  and the close — the only moment they can legally be read — and zeroes the
  buffer on every early exit, so an ignored return code cannot pass stack
  garbage off as a jobid. `jesircls()` becomes the NULL delegate, which keeps
  the `ENDREQ`, the #115 work-area release and the close in one place; it
  closes exactly as it always did, so nothing has to migrate to keep working.
  A caller that moves to `jesircl2()` **needs this in the sysroot first**.

### Changed
- **`strcpyp()` takes a `const void *source` (#104, PR #136).** It never writes
  through `source` — it uses it for `strlen()` and `memcpy()` and nothing else
  — so the parameter was simply under-qualified, and every caller holding a
  `const char *` had to cast the qualifier away. The one that did not,
  `__vsshwc()`, drew `src/clib/@@vsshwc.c:19: warning: passing arg 3 of
  'strcpyp' discards qualifiers from pointer target type`. cc370 issues that by
  default, with no `-W` flag, so it had been printed and discarded for as long
  as it existed until #103 made the build show it — and it was the **last**
  warning over the ten directories that make up `libc.a`, which made it the
  noise floor the next real warning would have had to be spotted against, the
  same dynamic that hid #99.
  **Source-compatible in the direction that matters:** a caller passing a
  non-const pointer still compiles unchanged, and a sweep of the known callers
  finds call sites only and no local redeclaration — so nothing has to move
  with this.
  `test/host/tstjestx.c` did have to move in the same commit: it defines a
  `strcpyp()` shim to satisfy the linker and reaches the real prototype through
  `jesjob.c` → `clibary.h` → `string.h` → `clibstr.h`, so leaving the shim
  behind is not a warning but `error: conflicting types for 'strcpyp'`, and
  there is no `make test` that would have caught it.
  `clibspl.h`'s `spl_strcpyp()` stays as it is — no callers anywhere in the
  tree, so no warning to clear and no reason to touch a second published
  header. `memcpyp()` is under-qualified the same way but draws no warning
  today, as no caller passes it a const pointer; it is left for its own change,
  together with the now-redundant `(void*)` casts at the ~20 `strcpyp()` call
  sites. Verified: cc370 warnings over the ten `libc.a` directories, 1 before
  and 0 after; `test/host/tstjestx.c` builds clean under ASan, 20/20.
- **The bare inline-asm SVC macros carry a clobber list (#125, PR #134).** Ten
  inline `__asm__` statements expanded to an SVC while declaring **no** clobbers
  at all, leaving cc370 free to keep a live value in a register the SVC
  destroys. All ten now declare `"0","1","14","15"`: `@@cminit.c` (STIMER),
  `@@cmterm.c` (STIMER ×2), `@@cmwshu.c` (STIMER), `@@ecbtwl.c` (WAIT ECBLIST,
  TTIMER CANCEL), `@@tmthrd.c` (WAIT ECBLIST), `@@abrpt.c` (OPEN, CLOSE) and
  `@@ctcrtx.c` (ATTACH) — the last worth a second look, because it loads R1 and
  reads R15 back into memory operands, so it already knew those registers were
  in play and just never said so to the compiler.
  **The failure mode is not theoretical.** In a server the identical bare form
  put a struct pointer in R15 and kept it there across a `STIMER`; the next
  iteration of the loop tested whatever the SVC had left, read a field from
  that address and exited — on MVS a wait loop returning after one pass with
  none of its exit conditions true.
  Nothing here is miscompiled today, and the reason **does not generalise**:
  every one of these loop bodies happens to contain a function call, and the
  call already forces R0/R1/R14/R15 to be treated as dead across that region.
  That server's loop had none. `@@cminit.c` is the one waiting to bite — it sits
  inside `#if 0`, so whoever re-enables it gets no call before the `STIMER`.
  Generated code compared before and after with `cc370 -O1 -S` for all seven
  files: six are byte-identical and `@@cmterm.c` gets *better* — with the
  clobbers declared the compiler hoists `&mgr->wait` out of the retry loop into
  R5 instead of recomputing it each pass — so this buys a guarantee at no cost.
  File-scope `__asm__` blocks that define standalone assembler routines
  (`EXITDRVR` in `@@ecbtwl.c`/`@@tmthrd.c`, `RETRY`/`RECOVERY` in `@@estae.c`)
  are deliberately untouched: they do their own `SAVE (14,12)` and do not share
  the compiler's register allocation.
  **#125 stays open** for the statements carrying a *partial* list — `"0","1"`
  in the four `@@75*.c` socket files, `"0","1","15"` in `@@75conn.c` and
  `@@enqdeq.c`, `"1","14","15"` in `@@ctwait.c`, `@@ecbwt.c`, `@@ecbpst.c`,
  `@@vsclos.c`, `@@vsopen.c`, `jesiropn.c` and the six `os/os*` files. Each is
  missing a register the SVC can destroy, so each is the same latent defect,
  but they are a much wider change across the base library of the whole
  ecosystem and deserve their own pass with its own before/after comparison.
- **BREAKING — the C startup stack GETMAIN is conditional, and a shortage
  abends U0801 by name instead of S80A from inside the SVC (#108).**
  `@@CRT0`/`@@CRT1`/`@@CRTM` obtained their stack with `GETMAIN R,LV=(0)` —
  register form, *unconditional* — so an address space that could not spare
  the ~262K abended inside SVC 10 with nothing to say. What reached the
  operator was a bare `S80A`, which on a busy system is indistinguishable
  from any other storage abend anywhere else in it: a server that loaded the
  program logged only `failed with S80A ABEND`, and neither the requester nor
  the size was recoverable from it. All three sites now issue
  `GETMAIN RC,LV=(R8),SP=(R2)` and, on a nonzero R15, `WTO` the module name
  and `ABEND 801,DUMP` — the same shape the CLIBGRT and CLIBCRT guards in
  those very files have carried since #81/#85. Such a server now reports
  `failed with U0801 ABEND`, and the console carries
  `@@CRT0 - No storage for C stack`.
  **This does not reverse #83's decision, it completes it.** #83 left these
  three GETMAINs unconditional on the grounds that "at program start there is
  no caller to fail back to", and that reasoning still holds — the program
  still dies. What changes is only that it dies *by name*. In particular the
  startup does **not** retry with a smaller stack: `PDPPRLG`
  (`maclib/pdpprlg.macro`) has no bounds check at all — it loads the NAB from
  `76(13)`, chains, and stores it back — so a short stack would run off its
  end silently rather than fail, which is strictly worse than abending.
  The refused length stays in **R8** for the dump. It is deliberately not in
  the WTO text: formatting a number needs writable storage, and at that point
  there is none — the GETMAIN just failed, and the CSECT itself is not
  writable because consumer load modules link RENT by default (`--norent`
  only on request), which is the one place a static work area would
  turn this diagnostic into the S0C4 it is meant to explain.
  **What changes for callers:** nothing at compile time; the new abend code
  arrives at each consumer's next relink. Monitoring keyed on `S80A` from a
  libc370 program start should key on `U0801` instead.
  Verification is by inspection of the assembled expansion, not by test: a
  startup storage shortage is not reproducible from batch on MVS 3.8j. The
  listing confirms `SVC 120` with a `B'00000000'` mode byte (conditional),
  only `LR`/`SLL`/`ICM`/`SR` ahead of it (no store into the inline parameter
  list, so the RENT attribute holds), `LR 0,R8` leaving R8 intact for the
  `ST R8,PPASTKLN` that follows, and the `LTR`/`BZ`/`WTO`/`ABEND` path behind
  it. `@@estae.c:147`'s `GETMAIN RU` stays unconditional as #83 left it.

### Fixed
- **Worker teardown no longer deadlocks on the manager lock, and never
  force-DETACHes or frees a live task (#11).** The S33E on STC shutdown had two
  causes stacked on each other, and the issue named neither — it recorded the
  drain failure as unexplained and treated the S33E and the nested ESTAE fault
  as separate problems.
  **The deadlock.** `dispatch_thread_term()` held `lock(mgr,0)` across its whole
  teardown loop, wait included. But `cthread_worker_wait()` — the only place a
  worker observes `CTHDWORK_POST_SHUTDOWN` — *opens* with
  `cthread_queue_del(&work->queue)`, which takes that same lock whenever the
  worker still holds a dispatched item. `lock()` is an exclusive ENQ with
  `RET=HAVE` (`@@lk.c`; `ENQ_HAVE`, `ENQ_EXC` and `ENQ_STEP` are all `0` in
  `clibenq.h`), so rc 8 goes only to the task that already owns it and any other
  TCB waits. Every worker that was executing a request when shutdown began
  therefore blocked at the top of `cthread_worker_wait()`, never reached the
  wait, and missed the window — **waiting on the very thread that was waiting
  for it.** `work->queue` is set in exactly one place, `dispatch_work()`'s
  `post_request` branch, which is why it hit busy workers and left idle ones
  draining normally. No wedged handler was ever required.
  **The free.** After the window expired the code force-DETACHed with
  `STAE=YES` — abnormally terminating a live subtask, the S33E — and went
  straight on to `cthread_worker_del()` → `cthread_delete()` → `free(*task)`.
  `newthread()` allocates the stack **inside** that handle
  (`calloc(1, sizeof(CTHDTASK) + newstack)`, `@@ctcrtx.c`), so the free released
  the 64K the dying subtask's recovery exit was standing on, and `free(*work)`
  released the `CTHDWORK` it still reached through `work->mgr` and `work->wait`.
  That is what #9/#10 hardened against: the hardening was right, but the thing
  it was recovering from is a use-after-free.
  `dispatch_thread_term()` and `dispatch_thread_quiesce()` now take the lock per
  array access and release it across the wait, and both adopt the
  `locked = lock(...)` / `if (locked==0) unlock(...)` convention every sibling
  already used. The DETACH is gated on `termecb` inside `cthread_detach()` and
  at the call sites, because `cthread_worker_del()` → `cthread_delete()` is a
  second unguarded route to it; `cthread_delete()` refuses the whole delete
  rather than only the detach, since gating the detach alone leaves the `free()`
  reachable and the free is the half that corrupts. `@@tmstop.c:46` was a third
  instance of the same ungated force detach and is fixed with it. A worker that
  will not stop is left in `mgr->worker` as the new `CTHDWORK_STATE_STUCK`, and
  `cthread_manager_term()` counts those and takes the retain-and-log exit PR #7
  built for the unjoined case — otherwise the dispatch thread ending normally
  would license `free(mgr)` under a live TCB, turning an S33E in a dying address
  space into a use-after-free on a running task.
  **Contract change worth reading:** `cthread_detach()` now returns
  `CTHREAD_DETACH_LIVE` and does nothing when the subtask has not posted
  `termecb`, where it used to terminate it. A caller that already waits for
  `termecb` first sees no change. No owner check was added on
  top: MVS already refuses a DETACH from anything but the attaching task, and
  turning that loud failure into a silent skip would hide a defect rather than
  prevent one.
  The wait stays a `STIMER` poll deliberately —
  `ecb_timed_wait(&task->termecb, …)` would pass the **same** ECB as both the
  waitlist entry and the timeout ECB (`@@ecbtw.c`), posting on timeout the very
  flag the loop tests, so the next pass would read "terminated" and detach a
  live subtask. New probe `test/mvs/tstwterm.c` + `jcl/tstwterm.jcl`: one
  worker, one queued request, teardown while it runs, with a handler that is
  healthy and merely slow.
- **`__listpd()` bounds its directory walk by the bytes `fread()` delivered
  (#80, defect 2).** The halfword taken out of the block became the loop bound
  with no relation to how much was actually read, and three reads in the body
  were unbounded by it: the 8-byte end-of-directory `memcmp`, `buf[pos+11]`
  (the user data length), and a `memcpy` of up to `12 + 2*31 = 74` bytes.
  **A full 256-byte block is the ordinary case that trips it** — entries are at
  least 12 bytes, so 21 of them fill bytes 2..253 and leave two over, `pos = 254`
  still satisfies `pos < 256`, and the first thing the body does is read
  `buf[254..261]`: six bytes past the end of a 256-byte **stack** array, after
  which a member record is built out of whatever was there, `calloc`ed, and
  added to the array the caller believes. Measured under ASan against the
  pre-fix source: `stack-buffer-overflow, READ of size 8 ... at offset 320`,
  where the frame reports `'buf'` as `[64, 320)` — the read starts exactly at
  the end of the array.
  **This reaches the bounded callers too**, which is why it is worth landing
  ahead of the rest of #80: the filter is applied *inside* the loop, after the
  entry has been read, so a caller that passes a filter runs through the same
  overrun as one that does not.
  The fixed-part bound now lives in the loop condition, where `pos + 12 <= len`
  covers the sentinel and the length byte together, and the variable part is
  tested once the entry size is known (`if (pos + size > len) break`). `len` is
  clamped to the read (`if (len > nread) len = nread`) and a read too short to
  hold a block length at all (`nread < 2`) stops before the halfword is
  dereferenced, where the old `len <= 0` did not. Nothing is lost on a
  well-formed block: its last entry ends exactly at `len`, so the loop leaves at
  `pos == len` either way — what the new condition drops is the 1-11 bytes of
  padding the old one walked into.
  New host test `test/host/tstlspd.c`, 15/15 under ASan, red against the pre-fix
  source. **#80 stays open** for defect 1 (unbounded allocation, which needs a
  `max` parameter and therefore the relink round) and defect 3 (a `calloc()`
  failure returns a silently truncated list, whose convention is settled
  together with #61). `break` keeps a malformed entry distinct from the `x'FF'`
  logical end of directory and preserves what the block already yielded;
  *signalling* that shortfall to the caller is defect 3's job, not this change's.
- **`cthread_worker_add()` releases the manager lock when the create fails
  (#107).** The function takes the manager lock and drops it under its `unlock:`
  label, but the `cthread_create_ex()` failure path left through `goto quit` —
  *below* that label — so a lock this call acquired was never released. `lock()`
  is an exclusive ENQ (`CLIBLOCK`/`LOCK.%08X`) held by the issuing TCB, so once
  it is left held every later `lock(mgr,0)` from any other task in the address
  space waits forever: `dispatch_thread_check()`/`dispatch_thread_work()` wedge,
  `cthread_queue_add()` blocks with them, and the whole worker pool stops **with
  no abend, no message, and nothing in a dump pointing here**. The trigger is
  the realistic one — `cthread_create_ex()` returns NULL when the
  `CTHDTASK + stack` `calloc()` fails — so the hang arrives exactly under the
  storage pressure that leaves the caller least able to absorb it.
  **Latent in-tree, live for consumers.** All three in-tree callers
  (`dispatch_thread_init()`, `dispatch_thread_work()`, `dispatch_thread_create()`
  in `@@cminit.c`) already hold the lock when they call in, so `lock()` returns 8
  (ENQ `RET=HAVE`), `locked != 0`, and skipping the unlock happens to be correct.
  But `cthread_worker_add()` is exported in `include/clibthdi.h`, and an unlocked
  caller owns an ENQ it never releases. One line: `goto unlock` instead of
  `goto quit`. `rc` is already -1 at that point, and the `cthread_worker_del()`
  above re-enters `lock()`/`unlock()` correctly through the same `locked`
  convention. The sibling functions (`@@cmqadd.c`, `@@cmqdel.c`, `@@cmwdel.c`,
  `@@cmterm.c`) already route every exit through their unlock; this was the only
  one with an exit that bypassed it.
- **`rename()` renames PDS members via STOW change under `DISP=SHR`, not
  IDCAMS ALTER (#131).** The ALTER sibling of #127: IDCAMS allocates the
  ALTER target exclusively, and with a standing allocation of the same DSN
  in the address space the shared SYSDSN ENQ is escalated to exclusive
  with no way back down - measured with the same KEEP-DD A/B that settled
  the delete (the probe sat in `IEF099I WAITING FOR DATA SETS` for the
  life of the renaming step).  Names of the form `dsn(member)` on BOTH
  sides with the same dsn now go to `__renmem()` (STOW change, TTR and
  ISPF statistics preserved); whole data sets and cross-data-set forms
  keep IDCAMS ALTER.  No known caller was exposed - those that rename members
  already call `__renmem()` directly - so this closes the API-level trap.
- **`vsnprintf()` honours its bound on every conversion and always
  terminates (#128).** The simple conversions were bounded, but every
  width/precision conversion went to `__examin()`, whose first act was
  `unused(chcount)` - the remaining-space budget was discarded and the
  conversion written unbounded through `*s++`; `snprintf(buf, 10, "S%03X
  U%04d", ...)` overran its buffer (caught by a caller's buffer-canary
  test).  vsnprintf also
  wrote up to n content bytes with no NUL on truncation.  `__examin()`'s
  fifth parameter now is the space left in s and the string sink stops
  there (including the %f memcpy) while still returning the logical
  length; `vvprintf()` passes INT_MAX for vsprintf's deliberately
  unbounded sink; vsnprintf reserves the final byte for the terminator (C
  semantics: at most n-1 content bytes, always a NUL when n > 0).  New
  host test `test/host/tstvsnp.c`, 16/16 under ASan, heap-buffer-overflow
  against the pre-fix source.
- **`remove()` deletes PDS members via STOW under `DISP=SHR`, not IDCAMS
  (#127).** IDCAMS DELETE allocates its target exclusively, and MVS keeps
  one SYSDSN ENQ per DSN per address space at the highest level any
  allocation needs: with a standing allocation of the same DSN in the
  address space (a STEPLIB, a server's SYSENV DD), the shared ENQ was
  escalated to exclusive - and MVS ENQ has no way back down, so the data
  set stayed blocked for every other address space until the step ended
  (a long-running server kept SYS2.PARMLIB locked until it was restarted).
  Names of exactly the form `dsn(member)` now go to the new `__delmem()` -
  allocate `DISP=SHR`, open the BPAM DCB for OUTPUT, STOW delete, close,
  free, the pattern `__renmem()` has always used - which also routes the
  delete through OPEN's RACF gate.  Whole data sets keep the IDCAMS path.
- **`jesjob(dd=1)` walks the internal text by its records, and an abend
  mid-walk no longer leaks what it built (#126).** The parser stepped over
  raw INTXT blocks by `STRLTH` alone - a stride that matches the record
  stride for simple records and derails exactly where the record structure
  shows: the `0xFF` end-of-block marker, read as the high byte of a
  text-string length, sent `process_dd()` up to 64K past the buffer (the
  layout-dependent S0C4 storms seen reading job output), and a spanned
  record - any statement over 255 bytes arrives as SPLINE parts - could
  desynchronise the walk into stale buffer tails.  The blocks now go
  through `__jesprb()`, the hardened record
  walk `jesprint()` already uses; the key walks in
  `process_job()`/`process_exec()`/`process_dd()` are bounded by the record
  end; the chain's `spool_read()` is checked like the IOT loops have been
  since #28.  Separately, everything the walk builds is anchored in the JES
  handle while it is being built - measured before the fix, each S0C4
  leaked ~26K of half-built job arrays and buffers, which is what degraded
  a long-running server's address space - so recovery that only holds the handle frees it
  via `jesclose()`.  `test/host/tstjestx.c` grows four walk-bounding cases,
  including a storm-shaped block driven through the real `__jesprb()`.
- **`send()` honours the X'75' retry code `-2`, on a bounded budget (#120).**
  `src/dyn75/@@75send.c` tested its reply for `-1` and handed everything else
  back as a byte count — so the "would block, wait and reissue" code `-2`
  reached the caller as if it were a length. `@@75recv.c` has had the matching
  `STIMER WAIT` since forever; SEND simply never needed it, because Hercules'
  X'75' emulation called a **blocking** host `send()` straight on the emulated
  CPU thread. A client that stopped reading filled the host send buffer, the
  guest `send()` blocked, the CPU stopped making progress, and the Hercules
  watchdog killed the whole emulator by design (`impl.c` `CRASH()`).
  A Hercules patch (hyperion `1a599b0d`) made that send non-blocking and gave it the
  contract RECV and ACCEPT have always had: a non-blocking guest socket
  (`FIONBIO`) gets `-1` with `EWOULDBLOCK`, a blocking one — the default,
  `Ccom_blk = 1` — gets `-2`. On a patched emulator that gap turned a
  transiently full send buffer into a **failed transfer**: a caller that
  treats `rc <= 0` as fatal - an FTP server's data transfer, say - let any
  client slower than the server abort a download, and one that discards the
  return value would drop a response line silently. **A caller that sets
  `FIONBIO` was never affected** — it takes the `-1`/`EWOULDBLOCK` path its
  own send layer already handles.
  **The change is purely additive.** An unpatched Hercules cannot return `-2`
  from SEND at all — its `case 10` yields only `-1` or a count — so on such a
  system the new branch is dead code and behaviour is bit-identical to before.
  That is what makes libc370 the *first* thing to roll out rather than the
  last: it costs nothing on an unmodified emulator and makes every system
  ready for a patched one, whenever that arrives.
  **Only `-2` retries.** `-2` always means zero bytes went out, so the
  identical buffer is reissued unchanged and no accumulation is needed. A
  short write stays a byte count and is returned as one after a single X'75':
  a general partial-send loop would change `send()`'s semantics for every
  caller, and callers build state machines on exactly that partial return
  (an HTTP server's file sender, for one).
  **The parameter list is rebuilt for every attempt, and that is the
  load-bearing part.** `@@75.s` ends with `STM 0,15,0(11)` — it stores all
  sixteen registers back into the caller's `PL75` — and x75.c's guest-to-host
  copy loop drains R1 to zero and advances R5 past the bytes it moved, after
  which the second X'75' sets R1 = `len_out`, which is 0 for a SEND. A retry
  that reused the list would ask the emulator to send **zero bytes from a
  pointer already past the buffer**: a silent no-op that would burn the whole
  budget against a peer that had in fact drained. That is why `@@75recv.c`
  re-does its `XC` clear and re-sets R6-R9 inside its loop; R1/R5/R7/R8 are
  now re-set the same way.
  **The wait is bounded:** `SEND_STALL_MAX` 100 attempts ×
  `STIMER WAIT,BINTVL==F'10'` (0.10 s) = ten seconds without progress, then
  `-1` with `EWOULDBLOCK` so the caller can tear the session down. The numbers
  match an existing HTTP server's stall policy deliberately, so a stalled
  peer is dropped on one number instead of several. That
  errno is set in C, not fetched: `Cerr[]` was never assigned for a `-2`, so
  there is no emulator errno to ask for.
  **The 4096-byte chunking `@@75recv.c` documents is *not* mirrored here.**
  x75.c's copy loop is direction-symmetric (same 255-byte segmentation, same
  `effective_addr2 += i`), `lar_tcpip()` sets `len_in` from R1 with no cap, and
  `grep` finds no `4096`, `4095` or `0x1000` in x75.c, tcpip.c or their headers
  — whatever that comment observed, its cause is not in the guest/host copy.
  `test/host/tst75snd.c` links and executes the real `@@75send.c` against a
  fake `__75()` that reproduces the register write-back, so a fix that hoisted
  the assignments out of the loop fails it. Seven cases, both sides of the
  budget included: 39/39 green, 25/39 against the pre-fix file — and every
  check in the four "must not change" cases passes on both, which is the
  additivity claim above, stated as a number. Not covered there: the `STIMER`
  itself (the host recipe removes it with the `XC`; it is by inspection of
  `@@75send.s`, the same line `@@75recv.c` has shipped for years, and nothing
  live crosses it — SVC 47 preserves R2-R13 and R12 is reloaded behind it) and
  that `-2` actually arrives — the end-to-end gate is a send-stall
  reproduction on a patched Hercules, driven against an FTP server.
  **This reaches a running system only after a relink:** libc370 is the cc370
  sysroot and is statically linked, so a program needs `sdk/mklibc.py all` on
  its build host, a rebuild, and a deploy before the fix is anywhere.

- **A failed internal reader open no longer leaks the handle, the RPL work area
  and the DD (#115, PR #117).** `__vsam_close_intrdr()` was a stub that returned
  0, so the failure path of `__vsam_open_intrdr()` released nothing: a failed
  OPEN left the 192-byte `VSFILE`, the 80-byte RPL work area and the
  dynallocated DD behind — and the DD stayed for the life of the address space,
  because the "unallocate at close" text unit only fires on a CLOSE that never
  happens. `__vsam_close_intrdr()` now closes through `__vsclos()`, which CLOSEs
  only when the open flag is set and then frees the handle, and it releases the
  work area whose only reference is `rpl.rplarea`; `jesiropn()` unallocates the
  DD with `__dsfree()` — the TODO that stood at its `quit` label — with `errno`
  saved across the cleanup so the caller still sees why the open failed.
  **The open flag decides success, not R15:** OPEN can return a warning (4) over
  an ACB that is open and usable, and R15 was stored into `rc` and never
  cleared, so that came back as a failure, handing the caller a NULL handle
  while the cluster stayed open and the DD allocated. The DDNAME buffer also
  grows to 9 bytes and the SVC 99 blank padding is trimmed, because the name is
  used as a C string: `__txddn()` under `__dsfree()` takes its length with
  `strlen()`, and so does the `strcpyp()` into the handle, which read past the
  8-byte array as it stood. Error paths only, reviewed as read code: they need a
  forced OPEN failure to exercise and there is no host test for VSAM.
- **The internal reader's 80-byte RPL work area is freed on close (#115, PR
  #116).** `jesiropn()` `calloc`s it and hands it to VSAM through `MODCB AREA=`;
  its only reference is `rpl.rplarea`. `jesircls()` closes through `vsclose()`,
  which frees the `VSFILE` and nothing else, so every internal reader open
  leaked that block — and **one live 80-byte block pins its whole 4K heap page**
  for the life of the address space. Measured as one planted page per internal
  reader open on a server that submits jobs all day. The area is remembered
  before `vsclose()` frees the handle that holds the only pointer to it, and
  released once the CLOSE is through; `vsfile` is tested for NULL first, because
  the reads would otherwise fetch from the PSA — low-address protection stops
  stores into page zero, not fetches — and `free()` would be handed whatever
  word lives there. Verified on MVS: on a cold address space the intrdr
  open/close cycle now costs 0 bytes where it cost 4096.
- **`process_exec()` and `process_job()` bound every copy by its destination
  (#111).** Both took `len = t->len & 0x7F` — a one-byte field masked but not
  bounded, so up to **127** — and `memcpy`'d that into `process_intxt()`'s
  twelve-byte locals (`jobname`, `userid`, `stepname`, `procstep`, `program`).
  Five keys reached them: `USERK` and `JOBK`, `PGMEK`, `PROCEK` and `EXECK`.
  The values then left the frame: `process_sysout()`/`process_sysin()`
  `strcpy()` `stepname`/`procstep` into **nine**-byte `JESDD` fields, and
  `process_intxt()` `strcpy()`s `userid` into a nine-byte `JESJOB.owner` — so
  one bad length byte overran a stack buffer and a heap one. Both are clamped
  to 8 now, which is what an MVS name is and exactly what those nine-byte
  fields hold. This is the clamp PR #22 (`58319b2`, "Fix S0C4 in process_dd
  for multi-digit SPACE values") gave the sibling `process_dd()` and did not
  give these two; that PR's trigger was an ordinary JCL detail and its symptom
  was an S0C4. Red→green on the host under ASAN by `test/host/tstjestx.c`,
  which `#include`s the real translation unit and hands each parser a
  destination allocated at exactly twelve bytes: pre-fix ASAN reports
  `WRITE of size 64 ... 0 bytes after 12-byte region` inside `process_job`,
  post-fix 14/14. Case (7) is also PR #22's regression guard, which it never
  had. Whether this is the mechanism behind #108 is **not** settled — it is
  fixed on its own terms.
- **`jesopen()` no longer returns a JES handle whose spool array was never
  allocated (#108).** `try_jesopen()` checked every allocation but the
  `arrayadd()` that stores the freshly opened spool handle into `jes->js`. On
  a storage shortage that call returns -1, `jes->js` stays NULL, and the
  handle was still returned looking complete — eye catcher set, `jes->cp`
  populated. `jesjob()` and `jesprint()` then evaluated `jes->js[0]`, and on
  MVS that load *succeeds*: low-address protection stops stores into page
  zero, not fetches, so they took a non-NULL value out of the PSA, walked past
  `__jsrd4()`'s own `if (!js)` test and stored through it — a store into
  nucleus-derived storage from problem state, key 8, i.e. an S0C4 whose cause
  was a GETMAIN that failed several frames earlier. Same shape as #61 and #80:
  the allocation is not the problem, the unchecked result is. `jesopen()` now
  reports the failure and returns NULL, closing the spool data set directly
  before `jesclose()` (which reaches it only through the array that does not
  exist); both indexing sites are guarded so a handle from any other source
  cannot take the same route. `jesopen.c` also gains `clibwto.h` — it was
  calling `wtof()` with no prototype in scope, which on this target decides
  linkage (#39). Verified red→green on the host by
  `test/host/tstjesop.c`, which links the real `jesopen.c`, `jesclose.c` and
  array code and injects the failure at `calloc` so the real
  `arrayadd()`/`arraynew()` pair runs and really fails: 13/18 pre-fix, 18/18
  after.

## [1.0.2] - 2026-08-12

Mostly silent failures: paths that reported success while doing nothing, losing
data, losing storage, or building against a stale compiler. Two heap defects in
the JES2 spool record walk that any foreign or truncated block could reach, a
compare-and-swap that stored the wrong word entirely, an S0C4 on open-by-DSN
from a TSO command processor, and public prototypes for four routines consumers
had to declare by hand. Eight breaking changes: `jesprint()` twice, the atomics,
`clock64()` — which finally returns the seconds its type always claimed, in one
coordinated move with `time64()` so that `time64()`'s own value does not budge —
`racf_auth()` — which stops asking for audit suppression with the bit that
means "this is a VSAM data set", and in exchange answers 4 where it answered 0 —
`__dsalc()` — which also stops narrating its failures to the operator, the
first instalment of taking the console back from the library — `malloc()`,
which can finally fail: storage shortage returns NULL with `errno = ENOMEM`
where it used to abend S878 — and the `fopen()`/dynalloc path, which follows
suit: shortage during an open fails the call instead of abending S80A. `__txdsn()` is
the second, and the plainer case: it dumped a control block on the path where
everything had worked. `__loadhi()` — the load-into-CSA path a subsystem
depends on — carries two more of the same kind: an `fclose()` on stack residue,
in supervisor state under PSW key 0 where that is cross-key corruption rather
than an abend, and an RLD walk bounded by a continuation bit instead of the
record's byte count, which relocated past the end of the module and stored an
adcon into unmapped storage. And the build stopped throwing warnings away:
`-Wuninitialized` is on, its 23 hits are cleared — five of them real — and a
warning on an otherwise successful compile now reaches the build output.

### Added
- **The malloc subpool is a runtime value (#89).** `@@GETM` no longer assembles
  subpool 0 in: it resolves the ambient heap subpool per call from `PPAHEAPS`
  (+0x22) in the current TCB's own PPA — validated exactly like `@@PPAGET`
  tier 1, no owner-TCB fallback — and records it in the high byte of the
  rounded-size header word, which is precisely the `SP||LV` pair `@@FREEM` now
  feeds the R-form FREEMAIN. A block therefore travels with its subpool and
  `free()` needs no variant; a pre-#89 header decodes as subpool 0 unchanged.
  `@@CRT0`/`@@CRT1` inherit `PPAHEAPS` from the caller's PPA when the old
  `8(TCBFSAB)` word actually validates as one (on the first CRT of a TCB it is
  unvalidated MVS residue), so a LINKed C module allocates from its caller's
  ambient subpool and the caller's value is current again on return. New API in
  `clibos.h`: `__setsp()` (set ambient, returns previous), `__getsp()`, and
  `__getmsp(size, sp)` — the explicit-subpool `__getm()` used to pin storage
  that must survive a `FREEMAIN SP=n` reclaim. **Nothing changes until someone
  calls `__setsp(n)`**: the ambient value is 0 everywhere today, cthread TCBs
  (no PPA) stay pinned to 0 by construction, and the T0 probe (`tstsubp`)
  measured subpools 1-127 as strictly per-task on MVS 3.8, so one constant
  subpool number is enough for a caller that pins storage per task. Guard rails that came with
  it: `__getm()` now refuses a rounded size past 24 bits (it is callable
  directly and malloc's 6M cap does not protect it), and the danger inherent
  in an ambient subpool — server-lifetime storage allocated from module
  context landing in the module's subpool — is documented at the API with the
  pinning rules (`__getmsp(size, 0)` / `__setsp(0)` brackets).
- **`__cas()` — compare and swap the way the instruction does it (#48).** Stores
  `new_value` only if `*mem` is still `*expect`; returns 0 when it swapped, 1
  when it did not — and then `*expect` holds what is in memory instead, which is
  what a retry loop needs and what a plain exchange cannot tell you. `-1` for
  NULL arguments, so "did not swap" and "you passed nonsense" are
  distinguishable. This is the operation a caller needed that worked around
  the broken `__cs()` by swapping a value in and back out again: between those two swaps another thread sees a value that was
  never meant to be published. One `CS` has no such window.
- **`JESPR_TRUNC` (#23).** A new `jesprint()` stop reason: a record ran past the
  end of a block, so the block is truncated or malformed and the rest of it was
  skipped. The chain is intact and the walk continues with the next block, same
  as `JESPR_NOBUF`. Purely additive — a consumer that does not know the constant
  reports nothing for it, exactly as it does today for a block it never noticed
  was bad. A caller that reports the stop reasons should gain one `case`
  for it.
- **A spanned-record fixture from a real spool block (#44).** Case (12) of
  `test/host/tstjesprb.c` is a byte-for-byte reconstruction of a 4000-byte
  SYSOUT record captured on MVS 3.8j: a `FIRST` part carrying 3647 bytes and
  announcing 4000, then a `LAST` part with the remaining 353 in the next block.
  It pins three things at once — that `len2` counts the payload *after* the
  2-byte prefix (which closed #29), that the parts sum to exactly the announced
  total, so the clamp added for #24 cannot fire on legitimate data, and that a
  `FIRST` part filling its block to within one byte ends that block normally
  rather than as `JESPR_TRUNC`. Also worth knowing from the capture: "spanned"
  does not mean "too big for a block" — `PRLINE.len` is one byte, so *any*
  record over 255 bytes takes the spanned form, and one that fits arrives with
  `FIRST|MIDDLE|LAST` all set at once.
- **Host regression for the spool record walk, `test/host/tstjesprb.c` (#25).**
  It links and executes the *real* `__jesprb()` — unlike `test/host/tstcmtt.c`,
  which had to hand-mirror the code under test because its TU cannot be built on
  the host. 87 checks over plain records, carriage control, a spanned line inside
  one block and across two blocks, an immediate EOB, a zero-filled block, and a
  callback that stops the walk. Two cases from #25's list are deliberately
  missing: a truncated block (#23) and a MIDDLE/LAST part with no FIRST (#24) are
  red today and land with those fixes. What the spanned cases pin is the parser's
  own contract, not JES2's format — whether `len2` on a FIRST part includes the
  2-byte total-length prefix is #29 and needs a captured block from the target.
  **The same source also runs on MVS** (`jcl/tstjesprb.jcl`): cc370 compiles it
  unchanged, because since #25 the walk needs neither assembler nor I/O. That run
  adds what a host run cannot — cc370's code generation, 24-bit pointers under
  the bounds arithmetic, and the record layouts (`sizeof(PRLINE)`,
  `sizeof(SPLINE)`, the block header offsets). It does not add the memory-safety
  verdict: there is no sanitizer on MVS 3.8j, so the host ASan run stays the gate.
  87/87 on both, COND CODE 0000 on MVS 3.8j.
- **Prototypes for `loadenv()`, `tzset()`, `__exit()` and `__svc99()` (#5).**
  All four link fine — only the declarations were missing, so consumers got
  implicit-declaration warnings and carried local `extern`s. `loadenv()` is now in `clibenv.h`, `tzset()` in `time.h`,
  `__exit()` in `clibcrt.h`, and `__svc99()` moved out of the `#ifdef MUSIC`
  guard in `mvssupa.h` that hid it on MVS — SVC 99 is an MVS service. No
  `#pragma linkage` on `__svc99()`: `@@SVC99` takes the standard OS parameter
  list cc370 builds for any call, which is what the implicit declarations were
  already producing. `__exit()` is deliberately not `noreturn` — control does
  not come back, but the definition ends in a plain `return`. Verified as a
  declaration-only change: every one of the 712 generated `.s` is byte-identical
  across the change except `@@ver.s`, which bakes in the git revision.

### Changed
- **The startup version stamp reads `LIBC370 1.0.2 (<commit>)`.** It was
  `libc370 v1.0.0 (<commit>)`: the module name is uppercase like every other MVS
  message on the console it is written to, and the `v` carried nothing the
  surrounding format did not already give. `libc370_version()` is otherwise
  unchanged and no consumer parses the string — it is only logged — but anything
  grepping a job log for the old form needs the new one. The example in
  `clibver.h` moved with it so the documented and the emitted form do not drift.
- **BREAKING — `fopen()`, `__aopen()` and `__svc99()` fail on storage
  shortage instead of abending (#83).** The #81 fix left three unconditional
  GETMAINs on the open/dynalloc path: the FUNHEAD `SAVE=` dynamic save area
  in `mvsmacs.macro` — used by `@@AOPEN`, `@@ACLOSE`, `@@ALINE` and
  `@@SYSTEM`, so the S80A hit before the function's first real instruction —
  `@@AOPEN`'s DCB area plus its three buffer sites, and `@@SVC99`'s work
  area. All of them are `GETMAIN RC` now, and each failure surfaces through
  the caller's existing error contract. `__aopen()` returns **-1** when even
  the save area cannot be obtained and **-12 (-ENOMEM)** from the DCB and
  buffer sites — with everything acquired up to that point released: the
  data set is closed, an already-obtained buffer freed, the DCB area freed.
  A failed open leaks nothing. `__svc99()` returns **-1**, the SVC never
  issued, the caller's request block untouched. `ropen()` already translates
  negative `__aopen()` codes into `errno`, so it reports ENOMEM without a
  change; `fopen()` returns NULL exactly as its callers always assumed. The
  `SAVE=` failure path is generic — restore the caller's registers, R15=-1 —
  which is why the same shortage also fails `fclose()`, terminal line I/O
  and `system()` cleanly instead of abending them.
  Measured red→green on MVS 3.8j: `test/mvs/tstaopn.c` (REGION=1024K)
  exhausts its region in three phases — the #81 diagnostics fire for each —
  then calls `__aopen()` and `__svc99()` directly. Pre-fix: S80A in the
  FUNHEAD GETMAIN. Post-fix: rc -1/-1, and after free-all the same
  `__aopen()` against DD:SYSUT1 succeeds, COND CODE 0000. Deliberately
  unchanged: `@@estae.c:147`'s `GETMAIN RU` (nothing sane to do when the
  recovery environment itself cannot be built) and the CRT startup GETMAINs
  (`@@crt0/1/m` stack allocation — at program start there is no caller to
  fail back to).
- **BREAKING — `malloc()` can fail: storage shortage returns NULL instead of
  abending S878 (#81).** `@@GETM` issued `GETMAIN RU` — register form,
  *unconditional* — which does not report a shortage, it abends. So every
  `if (!p)` in this library and in every consumer was dead code for the exact
  case it was written for, and a storage race in a server surfaced as an
  S878 somewhere down whatever call chain allocated next. `@@GETM` now issues `GETMAIN RC` and returns NULL on a nonzero
  R15, and `malloc()` sets `errno = ENOMEM` on the way out. `calloc()`,
  `realloc()` and `strdup()` already propagated NULL correctly, so the whole
  family fails the way its callers always assumed. The fix sits in `@@GETM`
  itself and not in a wrapper above it because callers also call
  `__getm()` directly.
  **What changes for callers:** nothing at compile time — the change is
  source-compatible and arrives at each consumer's next relink. At run time,
  allocation checks that never ran can now run; a site that does not check
  gets a NULL dereference at the point of *use* instead of an S878 at the
  point of *allocation*. The sweep of known callers lives in #81. Programs that build
  against crent370 instead are not covered by this change at all.
  Two guards ride along because a failable allocator creates hazards ahead
  of `main()`. The CRT startup code — `@@crt0`/`@@crt1`, mainline and
  CTHREAD, plus `@@crtm` — used the `@@CRTGET` result as a base register
  without testing it; with a failable calloc behind `@@CRTSET`, that `ST`
  would land in low storage at the `CRTSAVE` offset. All five sites now test
  R15 and fail loudly (WTO + user abend **U0801**) instead of corrupting the
  PSA. And `jesiropn.c` passed an unchecked 80-byte RPL work area to
  `MODCB` — under NULL the RPL would point its record area at address 0.
  The `wtof("Out of memory, bytes needed=%u")` + save area traceback in
  `malloc()`, previously unreachable for real shortage, now fires on every
  failed allocation — it names the requester, which is exactly the
  diagnostic that was missing. That path allocates nothing itself, so it cannot
  recurse.
  `test/mvs/tstgetm.c` (with `jcl/tstgetm.jcl`, REGION=2048K) is the
  red→green probe: it drives its own region to exhaustion, which abends
  S878 before this change and must arrive as NULL with `errno = ENOMEM`
  after it, then frees everything and proves allocation works again. It
  also pins `malloc(0)` → NULL, the 6 MB `MAX_CHUNK` cap, and the size
  prefix at p-4 that `realloc()` reads.
- **BREAKING — `clock64()` returns seconds, not milliseconds (#49).** It
  divided the microseconds out by 1000, so it returned milliseconds while
  `clock64_t` and its prototype both said seconds. `mclock64()` is the same
  seven lines, which made `clock64()` a duplicate of it and left the library
  with no function returning seconds at all — while `time64_t`, `gmtime64()`
  and the whole `m*`/`u*` scaling family are built on seconds.
  **What changes for callers:** anything reading `clock64()` as milliseconds
  now gets 1/1000 of what it did. **Those callers move to `mclock64()`**,
  which is unchanged and always was milliseconds. Anything feeding
  `clock64()` to `gmtime64()` as seconds was ~56 000 years out and is now
  right. `time64()`, `mclock64()` and `uclock64()` are **unchanged in value**;
  so are `mtime64()`, `utime64()` and everything derived from them.
  This could not be a one-line change. `time64()` was cancelling the bug with
  a second wrong constant — `clock64()` then `/CLOCKS_PER_SEC` — so correcting
  `clock64()` alone would have moved the ×1000 error into `time64()` and
  broken every in-repo consumer instead of none. Both sites moved together:
  `time64()` is now a straight `clock64()` pass-through, exactly as
  `utime64()` is for `uclock64()` and `mtime64()` for `mclock64()`, and it no
  longer reads `CLOCKS_PER_SEC` at all. That macro describes `clock()`, which
  this library does not implement (`src/clib/clock.c` returns `-1`), and it is
  no longer referenced anywhere in `src/`.
  The quietest thing this could have broken is in the thread manager.
  `dispatch_work()` (`src/thdmgr/@@cminit.c`) subtracts two `time64()` values
  and tests the result for truth, so it is a raw second count — an assumption
  nothing in that file named, and one that costs no abend and no message when
  it is wrong: milliseconds would post every waiting worker on essentially
  every manager pass, seconds/1000 would stop the timer posts for up to ~16
  minutes. It is now written down at the site, and `test/mvs/tsttm64.c` case
  (9) guards it by reading `time64()` twice across a real two-second `STIMER`
  wait and requiring a difference of 1..3. It measured 2 before the change and
  2 after.
  The rest of that test landed one PR ahead of this one (#73) for exactly this
  reason. Nineteen cases on MVS 3.8j, COND CODE 0000 on both sides of the fix;
  the only two that moved are the two that record the unit contract —
  `clock64() == mclock64()` became `clock64() == time64()`, and
  `clock64()/1000 == time64()` became `clock64()*1000 == mclock64()`. Every
  invariant — the `gmtime64()` era check, the seconds-since-1970 magnitude,
  the µs/ms/s tier agreement, monotonicity, `difftime64()` and case (9) — is
  untouched and green on both sides, which is the evidence that `time64()` did
  not move.
  A caller that read `clock64()` directly and compensated with
  `/CLOCKS_PER_SEC` moves to `mclock64()`.
- **BREAKING — `racf_auth()` asks for `LOG=NONE` with the bit that means it,
  and an unprotected resource now answers 4 instead of 0 (#63).** The library
  set `0x10` under the name `RACHECK_FLAG1_LOG_NONE`. `0x10` is **`DSTYPE=V`**
  (`sysmac/racheck.macro:562`); `LOG=NONE` is `0x02`. So two things were true
  at once: the audit suppression the library asked for never happened, and
  every `CLASS=DATASET` check it issued told RACF the entity was a VSAM data
  set. **What changes for callers:** a resource with no profile answered
  `0` and now answers `4` — "not protected", which is SAF's other way of
  saying allowed. **Test `rc <= 4`, not `rc == 0`.** A denial is unchanged at 8.
  The gate on this was never the rc — it was whether suppressing the audit
  also softens a decision, and that is now measured rather than assumed.
  `test/mvs/tstracmx.c` walks eight cells against RAKF on MVS 3.8j, each with
  five `flag1` values (`00`, `10`, `02`, `04`, `12`), counting both the rc and
  the RAKF messages the check produced:
  a user who is genuinely not permitted answers **8 with every flag value** —
  in FACILITY and in DATASET, with the ACEE in the parameter list and with it
  reached through the ASXBSENV fallback — and `ATTR` still decides, `UPDATE`
  refused against a profile granting only READ. Two answers move, and only
  two: a FACILITY resource with no profile goes 0 → 4, and the audit goes
  2 RAKF lines per check → 0. `0x04` (`LOG=NOFAIL`) behaves exactly like
  `0x02` on both counts, so there is no flag that buys the silence without
  the rc: on this RAKF they are the same trade.
  `DSTYPE=V` turned out to change no outcome anywhere in the matrix — `0x00`
  and `0x10` are identical in all forty cells, including the DATASET rows
  where `DSTYPE` is consulted — so the lie in the parameter list was a lie and
  not a defect. Worth knowing for anyone reading the old code.
  One measurement is narrower than it looks: the reference system carries a
  `DATASET *` READ profile, so no data set name on it is genuinely
  unprotected, and the DATASET rows answer 0 throughout. A system without that
  catch-all should expect 4 there too.
  While in the header: every `flag1` bit is now defined and named after the
  macro (`RACHECK_FLAG1_DSTYPE_V`, `..._LOG_NOFAIL`, `..._RACFIND`,
  `..._31BIT`, `..._ENTITY_CSA`), the three `flag2` bits the macro can set are
  defined for the first time, and the four `ATTR` values were re-checked
  against `sysmac/racheck.macro:231` and are **correct** — cell (7) confirms
  `UPDATE` on target. `racf.h` now documents the return codes it hands back.
- **BREAKING — `__dsalc()` no longer writes to the operator console, and an
  unknown `DISP=` token now fails instead of being ignored (#43).** Creating a
  data set that already exists — an ordinary outcome — put three lines on the
  console: a `wtof()`, a 20-byte hex dump of the SVC 99 request block, and this
  once per attempt for a client that retries. The rc and `S99ERROR` reach the
  caller unchanged, and it is the caller, not the library, that knows whether the failure was expected.
  The dump is parked under `#if 0`, the way `@@dsfree.c` already parks the same
  one. Fourteen further calls in the routine went with it — thirteen
  `Invalid …` reports and a duplicate of the out-of-storage message `malloc()`
  writes itself. Eleven of the fourteen sat on a path that already returned an
  rc, so nothing that reached the caller changed. Three did **not**: an
  unrecognized token in any of the three
  `DISP=` positions used to be reported to the console while `err` stayed 0, so
  the allocation went ahead with no disposition text unit at all — a WTO in place
  of a return code. `__dsalc()` now returns 1 there. A caller passing a
  disposition libc370 does not parse (`DISP=(NEW,PASS)` — `PASS` is not in the
  list) gets a failed allocation where it used to get a silently different one.
  Consumers that build the option string from user input should check their rc
  handling. `@@dsalc.o` no longer references `WTOF`/`WTODUMPF`, and a minimal
  module that calls `__dsalc()` links 1,076 bytes smaller — though the WTO chain
  itself stays, because `malloc.c` and `@@crtget.c` still pull it into everything.
  `test/mvs/tstdsalc.c` is the guard, run on MVS 3.8j against both libraries:
  the pre-fix module returns 0 from all three `DISP=` cases and reports the DD it
  allocated anyway (COND CODE 0008), the fixed one returns 1 (0000). Case (6) is
  the issue's own scenario, and since "no WTO" cannot be asserted from inside the
  program it brackets the call with two WTOs of its own — in the pre-fix job log
  four `__dsalc` lines sit between the markers, in the fixed one they are
  adjacent. The rule behind this is written down in
  [`doc/consumer-notes.md`](doc/consumer-notes.md); the remaining live calls
  across the library are the rest of #43.
- **The four counters and `cthread_wait()` declare what their assembler writes
  (#55).** `__inc()`, `__uinc()`, `__dec()`, `__udec()` and `cthread_wait()`'s CS
  block all load R0 and R1 and told the compiler nothing — safe today because
  cc370 happens not to want those registers across the asm, which is a register
  allocator's mood rather than a guarantee. `cthread_wait()` is the telling one:
  its *first* asm block declares `"1", "14", "15"`, the CS block below it
  declared nothing. Their retry labels (`AGAIN`, and `INCIT`/`SWAPIT` in the
  inc/dec pair) were file-scope in the generated assembler, so a second inline
  loop in any of those files — or merging the near-identical TUs, which is
  tempting — would have produced a duplicate symbol. Now named after their
  function. Verified as a no-op: of the 12 changed lines in each generated `.s`
  (4 in `@@ctwait.s`), **none** has a cause other than the label name. The four
  counters also gained their first tests, `test/mvs/tstatom.c` (8)-(10),
  including the documented wrap at the limits — `__inc()` at `INT_MAX` goes to
  0, not to `INT_MIN`, which is what makes them counters and not fetch-and-add.
- **BREAKING — `__cs()` is gone, and it was broken; use `__swap()` or `__cas()`
  (#48).** Two things were wrong with it and only one of them was a typo. The
  inline assembler did `L 1,0(,%2)` where `%2` holds the value, so it stored the
  word *at* `new_value` rather than `new_value`: nothing useful when that looked
  like low storage, an S0C4 when it looked like protected storage. And it never
  was a compare and swap — the `CS` retry loop turns the instruction's
  comparison into an unconditional exchange, because the caller never gets to
  say what it expected. So the exchange survives as `__swap()`, correct this
  time, and `__cas()` is the operation the name always promised. Nothing in the
  library called it, and no known caller depended on it. `test/mvs/tstcs.c` became `test/mvs/tstatom.c` and covers both
  functions, including the slot-claim pattern that motivated
  `__cas()`.
- **`TSTJESLG` drives the shipping walk instead of a copy of it (#45).** The
  probe reconstructed what `jesprint()` would do with its own `scanblk()`, a
  hand-written mirror — and the mirror had drifted: it advanced a spanned part
  by `4 + len2` where the walk advances `4 + 2 + len2` past a `FIRST` part's
  length prefix. On a data set of 500-byte records it reported half the lines
  `jesprint()` actually reads, while printing "jesprint() would print N lines"
  about code it never ran. Exactly the failure `test/host/tstcmtt.c` warns about
  in its own case. Since #25 there is no need for a mirror: the probe now calls
  `__jesprb()` and reports what it emitted, the longest line (over 255 bytes
  means the record was spanned) and `JESPRB.reason` per block.
- **BREAKING — `jesprint()`'s return value is a status, not the callback's rc
  (#26).** `rc` started as 503, became 404 or 0 — and was then overwritten by
  every `prt()` call, so on a completed walk the caller received whatever the
  *last* callback returned. Three meanings in one `int`: a callback returning a
  byte count leaked it into `rc`, a data set that printed 0 lines was
  indistinguishable from one that printed 500, and a callback returning a
  *positive* value was indistinguishable from "JES2 unusable". `jesprint()` now
  returns 0, 404 or 503 and nothing else; a callback that stops the walk is
  `st->reason == JESPR_STOPPED` with its rc in `st->prtrc`, and the lines that
  went out are `st->lines`. **Consumers that read the negative rc must move to
  `st` and relink**; one that already reads `st->reason`/`st->prtrc` needs
  no change. The break is
  silent — the signature is unchanged, so an un-updated consumer compiles and
  simply stops noticing.
- **`jesprint()`'s record walk extracted to `__jesprb()` (#25).** The block and
  record parser moved into `src/jes/jesprb.c`, an asm-free translation unit
  (buffer in, lines out) with its own `src/jes/jesprb.h`; `jesprint()` keeps the
  `spool_read()` chain, the jobkey/dsid checks and the `EX`/`TR` translate on its
  side of the emit callback. The spanned-line state (`prbuf`/`blksize`/`linelen`)
  and the line count moved into a `JESPRB` the caller carries across blocks,
  because a spanned line legitimately continues into the next block. Behaviour is
  otherwise unchanged: the two known memory-safety defects in the walk (#23, #24)
  are still there, deliberately — they now have somewhere to be tested from.
  One side effect worth the line: `esc_print()`'s stack frame drops from 1128 to
  104 bytes. Its HTML-escaping branch has been `#if 0` since output went to
  `text/plain`, but the `char buf[1024]` it used was declared unconditionally and
  cc370 allocated it on every call. Peak stack on the print path goes from 1304
  bytes (`jesprint` + `esc_print`) to 400 (`jesprint` + `__jesprb` + `esc_print`).
- **BREAKING — `jesprint()` reports why it stopped (#21, #22, PR #31).** `rc` was
  set to 0 before the block loop and every early exit left it there: a failed
  `spool_read()`, a block belonging to another job and a block belonging to
  another dsid all returned "success, nothing printed", indistinguishable from a
  genuinely empty data set. An MVS I/O error was swallowed and reported as
  success — the reason #4 was unanalysable. `jesprint()` now fills a `JESPRST`
  out-parameter (`st` may be NULL) with the stop reason, the blocks accepted and
  lines emitted, the MTTR it stopped on and the callback's rc. `JESPR_OPENEND`
  separates a still-open data set (foreign block *after* accepted blocks) from a
  purged one (`JESPR_FOREIGN`, first block foreign) — without it a consumer would
  answer 410 Gone for a running job's log it had just read 350 lines from. The
  chain follow is now bounded (#22): a self-chaining block ends the walk with
  `JESPR_LOOP`, `JESPR_MAXBLK` caps the iterations. The print callback gains a
  `void *arg`, removing the need to route context through the per-task GRT.
  **Every caller must be updated and relinked.** The overloaded return
  value is deliberately unchanged; untangling it is #26. Verified on MVS 3.8j
  with `test/mvs/tstjeslg.c` `PARM=',PRINT'` — on-target only, since
  `spool_read()` is a BDAM READ/CHECK and the TU carries file-scope assembler.

### Fixed
- **`__loadhi()` relocated past the end of the module (#100).** The RLD walk let
  the T bit (`x'01'` — "the next item shares these two ESD ids and is 4 bytes
  rather than 8") decide when to stop, adding the size of the *next* item rather
  than the current one. That comes out exact only because the first item always
  costs 8 and the last was assumed to have the bit clear; when the last item in a
  record has it **set**, the sum lands 4 bytes short of `rldcnt`, the loop runs
  once more, and the walk reads an item the record does not contain. What sits
  there is the point: `__aread()` does no deblocking for `RECFM=U`
  (`asm/@@aread.asm`) — it issues a READ for `BLKSIZE` and hands back the buffer,
  so behind a short record lies the tail of the previous, longer one. That
  residue reads as a perfectly plausible RLD item whose 3-byte offset can be
  anything, and `store()` wrote the adcon into unmapped storage: **S0C4,
  deterministic, no diagnostic**, and moving as soon as the module grows. An
  older ld370 emits that stale bit routinely — one real module has two such
  records, another 65. The walk now runs to `rldcnt`, capped at the
  length `__aread()` actually returned, and the T bit only selects the next
  item's size; a stale bit on the last item ends the loop instead of stepping
  past it, so every module an older ld370 already produced — which is everything
  now installed — relocates correctly. MVS program fetch bounds its own walk the
  same way. Two more from the same report ride along: `process_rldr()` refuses an
  offset that is not inside the module instead of storing through it and counts
  the refusals (accumulated across the module and reported **once**, with the
  total), and `__loadhi()` now **tests** `relocate_load()`'s return code, which it
  previously discarded — a module that did not relocate cleanly is freed and the
  load fails rather than being published to the caller, because half-relocated
  code in CSA is worse than no module at all. The control/text interleaving the
  report suspected is not what fails: ld370 emits control and RLD records
  separately, established by walking a real module's record stream before
  touching any code. Red→green on the host over real module bytes —
  `test/host/tstrldwk.c` links and executes the real `@@loadhi.c` against the 252
  bytes at offset 27383 of an ld370-linked load module, one of the records whose
  last item carries the stale bit. Pre-fix ASan reports `heap-buffer-overflow,
  READ of size 1, 0 bytes after the 252-byte region` — the phantom item's flag
  byte; post-fix 8/8, 41 items walked and not 42. `fetch()`/`store()` are out of
  the test's reach (`@@loadhi.c` holds addresses in `unsigned`, correct on the
  24-bit target and truncating on a 64-bit host), so every case passes `size = 0`;
  that arithmetic is unchanged.
- **The build compiles with `-Wuninitialized`, and a warning now reaches the
  build output (#102).** #99 was not a toolchain blind spot — cc370 finds it and
  always could. Two separate things kept it from ever being seen. The flag was
  never set: `sdk/mklibc.py` compiled with `-O1` and no warning options at all,
  and in this gcc 3.4.6 `-Wall` does **not** imply `-Wuninitialized`, it has to be
  named. And the output was thrown away: `compile_c()` captured cc370's stderr
  but only looked at it when the compile had *failed*, so a warning on an
  otherwise successful compile reached nobody. Both change together — warnings
  are collected and printed after the compile pass — since setting the flag alone
  would have changed nothing observable. The 23 hits in the sources that go into
  `libc.a` are cleared, and five were real: `rand()` returned stack residue as a
  random number when `__crtget()` came back NULL (0 now); `recv()` with
  `len <= 0` never entered the read loop and then tested and returned an unset
  `rc` (0 now, which is also the right answer for a zero-length receive);
  `__fseek()` left `newpos` unset for a `whence` that is none of `SEEK_SET`,
  `SEEK_CUR` or `SEEK_END` and every use below read garbage — **an unknown
  `whence` is rejected with -1** and `SEEK_END` folds into the same if/else chain,
  so there is exactly one decision on `whence`; `fgets()` decided on residue for
  `n <= 1` — `fgets(s, 1, fp)` now does what C99 7.21.7.2 asks (store the
  terminator, return `s`) and **`n < 1` returns NULL without touching the
  buffer**, since with no room even for the terminator there is nothing that may
  be stored; and `__start()`'s `progLen` is assigned only inside the branch that
  *sets* `GRTFLAG1_TSO` but read under a test of the flag itself, which lives in
  the GRT, is address-space wide and is only ever OR'd in — a second `__start()`
  with a non-TSO parm reached the read with `progLen` unset and used it as a loop
  bound and a pointer increment. Initialized to 0 so the read is at least
  defined; the design question behind it is #105. The rest the compiler cannot
  prove (`@@estae.c`, `@@tmrid.c`, 14 in `vvscanf.c`, `tm64gmtr.c`) and are
  initialized to say so. The reporting half earned its keep immediately by
  surfacing a warning cc370 had been issuing by default all along —
  `strcpyp()` discarding qualifiers from its source pointer — left alone here
  because the fix widens a header that ships into every consumer's sysroot
  (#104). Verified two ways: 23 cc370 warnings before and 0 after over the ten
  directories that make up `libc.a`, and an independent clang pass over all 784
  TUs in `src/` agrees, its only remaining hits being three in `src/wip`, which
  is not built. `-Werror=uninitialized` is deliberately not proposed yet.
- **`__loadhi()` called `fclose()` on stack residue (#99).** `FILE *fp` was
  declared without an initializer but tested at the common exit, and it is
  assigned only after the module has been LOADed, the CDE located and the CSA
  storage obtained — so the three failure paths before that point (the LOAD
  failing, `clib_find_cde()` returning NULL, the subpool 241 GETMAIN coming back
  empty) all reached `quit:` with `fp` holding whatever the stack happened to
  contain. Those three are exactly the failures a caller most needs to see: the
  diagnostic WTO is issued, but the address space then dies of a second,
  unrelated abend before the caller can act on the return code, so a legible
  `cannot GETMAIN 25784 bytes from subpool 241` is followed by something that
  looks like a storage overlay somewhere else entirely — and whether it abends at
  all depends on stack residue, so the same error reproduces differently between
  builds. Its callers run `__loadhi()` in supervisor state
  under PSW key 0, where a garbage `fp` that happens to point into mapped storage
  raises no protection exception at all: `fclose()` writes, and the failure
  degrades from an abend into silent cross-key corruption. Every other variable
  tested at `quit:` was already initialized at its declaration, so this was an
  oversight rather than intent. Verified statically, which is the right check for
  this class — the symptom depends on stack contents, so no runtime test
  reproduces it reliably, while the dataflow is decidable at compile time: three
  clang *used uninitialized* diagnostics and cc370's own
  `'fp' might be used uninitialized` before, silent after. Two dead declarations
  turned up by the same pass went with it.
- **A caught abend now costs ~nothing durable: the dead program's runtime is
  torn down (#96).**  After #93 a caught abend of a LINKed program still cost
  a fixed ~172K: 40K of ambient-subpool heap, and 132K that NO subpool
  release could reach.  The composition probe (`test/mvs/tstcrtlk.c`) pinned
  it by experiment: not abandoned module copies (a 128K-bigger inner module
  leaks the same), not RTM recovery (a local caught abend costs 0K), but the
  dead program's own runtime — above all the three stdio FILEs `@@start.c`
  eagerly fopens for every C program (`*SYSPRINT`, `*SYSTERM`, SYSIN), still
  OPEN after the abend with their DCBs and buffers deliberately pinned
  outside the ambient heap subpool.  New `__ppahrv()` (`src/clib/@@ppahrv.c`),
  called from the #93 walk for each validated abandoned PPA before its
  stack+PPA block is FREEMAINed, mirrors `__exit()` on the dead CLIBGRT:
  fclose() every FILE in the dead `grtfile` array (CLOSE is legal — same
  TCB, the task is not terminating, and fclose validates each FILE's own
  eyecatcher), free the env/wsa/devtb elements and arrays, free the
  atexit/on_exit registration arrays WITHOUT running the dead functions,
  then free the CLIBGRT and the CLIBCRTs as `@@GRTRES`/`@@CRTRES` would
  have.  Everything is validated before it is trusted (24-bit pointers,
  eyecatchers, never the survivor's GRT) — what does not validate is left
  alone, a leak instead of a corruption.  Measured red→green on MVS 3.8j:
  172K per abend steady state, 132K of it release-proof (RC 8 on
  the probe's 16K thresholds) → ~0K per abend (COND CODE 0000);
  the #93 probe's drain verdict improves from 3 to 4 of 4 1M blocks,
  tstsplnk and tstecbtw stay green.  Together with #93 the fixed
  per-caught-abend cost goes from ~427K to ~0.
- **`ecb_timed_waitlist()` no longer waits on an ECB nothing will post (#94).**
  The STIMER REAL failure code stored by `ERRET=SAVERC` was never read: the
  WAIT ran regardless, and for a caller-local ECB the timer exit is the only
  poster in the address space — a failed STIMER meant a task frozen in that
  WAIT forever, the shape of a wedged server worker, reachable
  exactly when storage is tight enough for STIMER REAL to fail.  The macro's ERRET path reaches the
  fall-through only via `LTR 15,15`, so rc is 0 exactly when the timer
  exists; on nonzero the call now unparks the plist slot from `fsa[0]`,
  skips the WAIT and returns `-rc` — no ECB is touched, callers own the
  retry policy — with one WTO per task the first time (`CRTFLAG_TMRFAIL`:
  a per-call WTO would flood the console precisely when the system is
  already starving).  `ecb_timed_wait()`/`ecb_timed_waitarray()` propagate
  the new return; `cthread_timed_wait()` keeps its contract — its callers
  loop on their own deadlines, which now bound the degradation to a tight
  poll instead of a dead worker.  The STIMER asm also became a proper
  output-operand asm: the old form used R0 without declaring it and stored
  rc through a pointer the compiler knew nothing about — harmless only while
  nobody read rc.  New probe `test/mvs/tstecbtw.c` + `jcl/tstecbtw.jcl`:
  timer-post semantics, the parked `fsa[0]` word across every call, and a
  drained-region leg (~5.7M malloc'd away, wtof-only while drained).  On a
  healthy system LSQA is fenced from the region, so STIMER survives a full
  problem-state drain — outcome B on both libcs (pre-fix and
  post-fix, both COND CODE 0000): the freeze itself needs
  a degraded production state, and the probe documents which outcome
  it saw (a pre-#94 libc hangs at the marked call on such a system; the
  fixed one returns the error).  What the runs do prove: the guard never
  false-triggers under storage pressure, and the timer path is unchanged
  when the timer exists.
- **A caught abend in a LINKed program no longer leaks its 256K stack (#93).**
  `try()`'s retry path unhooked the dead program's PPA from `8(TCBFSAB)` (#89)
  and then discarded the address — the stack+PPA block `@@CRT0` obtained as
  one subpool-0 GETMAIN (~262K: `MAINSTK` alone is 65536 fullwords) stayed
  allocated for the life of the address space, 61% of the fixed ~427K
  per-caught-abend cost measured from the caller's side.
  `call()` now walks `PPASAVE` from the abandoned head back to its snapshot —
  so a dead program that itself LINKed a dead program releases the whole
  chain — validating every hop the way `@@PPAGET` does (non-zero, 24-bit,
  `PPAEYE`), and FREEMAINs each block conditionally (RC) with the
  `PPASUBPL||PPASTKLN` pair `@@EXITA` frees: garbage at `8(TCBFSAB)` frees
  nothing, and a bad request inside abend recovery is a return code, not a
  second abend.  The unhook happens before the walk, so a second abend
  re-enters the retry with the chain already popped and frees nothing twice.
  Scope stays inside #89's decision: `@@AOPEN`'s DCBs, `@@SVC99` and
  `@@ESTAE` remain untouched — the task does not terminate and MVS closes
  nothing, so handing those back would convert a leak into corruption; only
  the stack block provably has no outside pointers (its owner's RB was purged
  by RTM before the retry point).  The `__try()` twin in `@@try.c` carries
  the same walk so the copies do not drift.  New probe `test/mvs/tstppafr.c`
  (+ `tstppamd.c`, `tstppain.c`, `jcl/tstppafr.jcl`): 6 single-level plus
  3 nested caught S0C1s must cost nothing durable — the verdict counts how
  many of four 1M mallocs fit in REGION=6M, sized against the ~434K measured
  per-abend cost so neither IEFUSI generosity nor fragmentation can decide
  it — while normal returns must not double-free and garbage at `8(TCBFSAB)`
  must free nothing.  Verified red→green on MVS 3.8j: pre-fix
  (RC 8, 0 of 4 blocks fit), post-fix (COND CODE 0000, 3 of 4, and
  successive inner stacks land at the SAME address — the block demonstrably
  comes back).  The out-of-scope CLIBCRT/stdio remainder (~170K per caught
  abend, the rest of the ~427K) still leaks by design.  `tstsplnk`'s
  red control leg leaned on the pre-#93 leak and moves from five to six
  unreclaimed 1M abends — green again.
- **`@@AOPEN`'s buffer-1 cleanup exists again (#90).** The failure path
  "buffer 1 obtained, VBS record area not" was written as
  `FREEMAIN R,LV=(0),A=(1),SP=SUBPOOL`, which the FREEMAIN macro rejects
  (IHB019: `SP=` is not allowed with `LV=(0)`) and then MEXITs having
  generated **no code at all** — and as370 treats the severity-12 MNOTE as a
  printing no-op, so the build stayed green and the #83 failure path silently
  leaked the buffer precisely when storage was already short.  The statement
  now packs the subpool into R0's high byte (`ICM R0,8,=AL1(SUBPOOL)`), the
  idiom `@@EXITA` and `@@AREAD` already use.  Red→green on MVS 3.8j with
  `test/mvs/tstabuf.c`: a carved 22K window (the only hole in an exhausted
  region) feeds three failing opens of a RECFM=VS data set whose 28K record
  area cannot fit — pre-fix each open leaks buffer 1 into the window and an
  18K probe no longer fits (RC 8); post-fix the window survives
  intact (COND 0000), and the #83 probe `tstaopn` stays green.
- **`try()` no longer resumes with a dead LINKed program's runtime environment
  (found by #89's T4 probe).** When a C program entered through LINK abends
  under an ESTAE, its `@@EXITA` never runs, so the PPA its `@@CRT0` chained
  at `8(TCBFSAB)` stayed there after the retry — and every CRT-anchored libc
  call in the surviving caller (stdio, `__crtget()`, since #89 the ambient
  heap subpool) resolved through the dead program's environment.  Under a
  worker that keeps running, that is a server's state after an abend in a module
  it LINKed; in the probe it was an immediate S0C4.  `___try()`'s `call()` now
  snapshots the word before dispatching the protected function and restores
  it on the retry path, so a caught abend leaves the caller's own runtime
  current.  The abandoned PPA and stack still leak (subpool 0 by #89's scope
  decision), but they are no longer *live*.  The unreachable `__try()` twin
  in `@@try.c` carries the same guard so the copies do not drift.  Verified
  red→green with `test/mvs/tstsplnk.c` (S0C4 → COND 0000,
  eight caught S0C1s, `8(TCBFSAB)` asserted after each).
- **The CRT/GRT anchor tier no longer dereferences NULL (#85).**
  `__crtget()`/`__grtget()` can return NULL — "CRT for TCB not found", and
  since #82 a failed constructor is a second route — but ~25 sites
  dereferenced the anchor unchecked: the stdio anchors (`stdin`/`stdout`/
  `stderr` themselves), the env family, `atexit()`/`on_exit()`, cthread
  push/pop/find, the mutex family, the socket table, `gmtime()`,
  `__dsalc()`, `clib_apf_setup()`, `__wsaget()` and the whole timer family.
  Every listed site now fails through its function's own contract (NULL,
  -1, a zero id, or a no-op — whatever its callers already handle). Riding
  along, the ignored-rc class: `atexit()`/`on_exit()`/`cthread_push()` pair
  their func/arg array adds with a rollback so the two arrays can never
  desynchronize; `newthread()` fails the create instead of returning a
  thread that `cthread_find()` and cleanup would never see; `@@listvl` and
  the JES job/DD walks log, free and stop instead of silently dropping the
  element; the six timer creators free the TQE and return id 0 when it
  could not be queued, instead of leaking one that would never fire. And
  the one route a *healthy* program could actually reach — `@@CRT0`/
  `@@CRT1` ignored `@@GRTSET`'s rc, leaving a program running with no GRT
  (no stdio anchors, no env) when the GRT calloc failed — is closed the
  way #82 closed the CRT route: WTO + user abend U0801 at startup.
  The NULL branches themselves require a TCB running C code without a
  CRT — the unsupported situation itself — so they are guards, not
  black-box-testable behavior (same standing as the `failed()` guard from
  #9). What is testable is that no touched happy path moved:
  `test/mvs/tstanchr.c` exercises the anchors, an env round trip, the exit
  hooks (handler firing visible as a WTO in the job log), cthread
  push/pop, the mutex family, `gmtime()` and a timer that really fires —
  COND CODE 0000 on both sides of the change.
- **`calloc()` no longer masks `nmemb * size` to 24 bits (#84).** The old
  `((nmemb * size) + 7) & 0x00FFFFF8` rounded up to 8 — fine — but also
  truncated the product to 24 bits, and there was no overflow check at all.
  Measured on MVS 3.8j before the fix: `calloc(1, 0x1000009)` returned a
  valid **16-byte** block for a 16 MB request, and `calloc(0x8001, 0x20000)`
  returned **128 KB** for a 4 GB one — no message, no NULL, the caller
  overran the heap at first use. Now any product a 32-bit `size_t` cannot
  hold (including the +7 rounding) is refused with NULL and
  `errno = ENOMEM`; the untruncated total goes to `malloc()`, whose 6 MB
  `MAX_CHUNK` cap rejects merely-huge requests, so nothing that used to
  work stops working — `test/mvs/tstcaloc.c` pins that with a zero-checked
  `calloc(1, 5M)` next to the two truncation cases (assertion-red: COND
  CODE 0008 before, 0000 after) plus the exact-2^32 wrap and the prefix
  word `realloc()` reads.
- **`open_vatlst()`'s "unable to open" diagnostic names the data set again
  (#59).** The message had two `%s` conversions and one argument, so `vwtof()`
  → `vsprintf()` formatted as a `char *` a word nothing had stored into: in the
  generated code the parameter list was two words long and `vsprintf` read a
  third. Garbage in the best case, an S0C4 in the worst — in the handler for a
  failure that had just been detected and was about to be reported cleanly by
  returning NULL. The line runs only once `fopen()` has already failed, i.e. on
  a missing or misspelled VATLST member, so the code that could turn a
  recoverable "no VATLST, carry on without comments" into an abend was the code
  that only ever ran when something was already wrong. The data set name is
  passed now — the built one, `SYS1.PARMLIB(member)`, not the string the caller
  handed in — and the `@@listvl:` prefix is gone from both live messages in the
  file, since `__func__` already names the routine. `@@listvl.c` also gains
  `#include "clibwto.h"`, which is the part worth remembering: `wtof()` was an
  implicit declaration there, and with no prototype in scope there is no format
  checking at all, which is how a two-`%s`-one-argument call could sit in a
  shipped library. Generated code is unchanged apart from internal function
  indices. A sweep of the whole library with `format(printf)` attributes
  temporarily attached to `wtof()`/`wtodumpf()`/`wtorf()` found this to be the
  only *too few arguments* in `src/` (two *too many* remain, in
  `@@abrpt.c:336` and `tm64ltmr.c:69`, where the surplus is ignored). The probe
  is `test/mvs/tstlstvl.c` with `jcl/tstlstvl.jcl`; its COND CODE only proves
  the call survived and kept the volume list, because a WTO cannot be read back
  from inside the program — the message itself is checked in the job log,
  between markers the probe writes itself. Run on MVS 3.8j against both
  libraries, and the pre-fix log is the argument in one line: for
  `__listvl(NULL, 0, "NOSUCHM")` it read
  `unable to open "NOSUCHM"` — the caller's string, still lying in the varargs
  slot, not the `SYS1.PARMLIB(NOSUCHM)` that was actually attempted — and for a
  full DSN it read `unable to open "   "`. Two calls, two different wrong
  values, which is what an unwritten word looks like. With the fix both name
  their data set, and the control window with `vatlst=NULL` stays empty. COND
  CODE 0000 either way: this one is not decided by the return code.
- **`__txdsn()` stops dumping the DALMEMBR text unit to the operator console,
  and checks the text units it builds before storing them (#60).** Every
  allocation of a data set name carrying a member —
  `__dsalc("dsn=SYS1.MACLIB(IEFZB4D0);disp=shr")`, and so every
  `fopen("DD:x(member)")` that goes through dynamic allocation — wrote a hex
  dump of the text unit to the console. Not on failure: on the success path,
  with no `if` in front of it. On MVS 3.8j the console is the SYSLOG (#4), and
  unlike #43 there is no judgement call attached — nothing had gone wrong. The
  same line held a second defect: the text unit was dumped *before* it was
  checked, so a failed `calloc` gave `wtodumpf(NULL, …)` — a plausible-looking
  dump of low storage in place of an allocation failure — and then
  `arrayadd(txt99, NULL)`. The DALDSNAM unit two lines below was passed to
  `arrayadd()` unchecked in the same way. Both are checked now, and the
  function reports through the return value every caller already reads
  (`if (err) goto quit`); a unit that was built but could not be added is freed
  rather than leaked, since `FreeTXT99Array()` only reaches what made it into
  the array. The NULL matters more than a defensive check usually would:
  `__dsalc()` ORs the high-order bit into the last array element and hands the
  list to SVC 99, so a NULL element is a text unit at address 0.
  `test/host/tsttxdsn.c` links and executes the real `@@txdsn.c` — 22/22 with
  the fix, 14/22 and eight failures against the pre-fix source. The generated
  `@@txdsn.s` no longer references `WTODUMPF`, and dropping the call also
  retires the implicit declaration it needed (the file includes neither
  `clib.h` nor `clibwto.h`), so it compiles clean under `cc370 -Wall -Werror`:
  one of the 129 translation units in #39, off the list for free.
- **`racf_login()` and `racf_logout()` stop taking the address-space-wide ASXB
  ENQ, and `racf_logout()` stops re-pinning a foreign ACEE (#64).** #58 removed
  the ENQ and the ASXBSENV poke from `racf_auth()`; it was never the only entry
  point holding either. `racf_login()` bracketed itself in `lock(asxb)` without
  ever touching ASXBSENV — RACINIT ENVIR=CREATE returns the new ACEE through the
  `ACEE=` pointer — so the ENQ bought nothing and cost every login an
  address-space-wide serialization point. `racf_logout()` did worse: it read
  ASXBSENV on entry, parked the ACEE being deleted there, and wrote the *observed
  value* back on exit. In a server with one TCB per user that observed value is
  routinely another session's ACEE, so the restore re-pinned an identity whose
  owner had already moved on — a chain a
  server could not close from its side. The delete needs no poke: the ACEE travels in the
  parameter list at offset X'34', which is where RACINIT looks first.
  One thing did **not** survive contact with the target. The issue expected
  RACINIT to clear ASXBSENV itself when it holds the ACEE being deleted, and the
  first cut of this fix dropped libc370's hand-coded clear on that assumption.
  It does not: `test/mvs/tstracfl.c` case (4) caught the field still holding the
  dead pointer, which is worse than holding none, because the next authorization
  decision follows it. So the clear stays — as a `__cas()` against the dead
  pointer, which cannot clobber a concurrent writer and therefore still needs no
  ENQ. That is the first use in the library of the compare-and-swap added in #48.
  Verified on MVS 3.8j against both libraries: pre-fix a caller holding the ASXB
  lock loses it across `racf_login()` and `racf_logout()` (cases 2 and 3), and a
  worker TCB parking NULL in ASXBSENV reads back a foreign ACEE 14 times in 1210
  loops while the main task churns 200 login/logout pairs (case 6) — COND CODE
  0008. With the fix all three are clean and the run is 0000. Consumers see no
  API change; a caller's ABEND-recovery DEQ of the ASXB loses its subject
  once this ships.
- **`racf_auth()` passes the ACEE in the RACHECK plist instead of writing it
  into ASXBSENV (#58).** It used to authorize against a caller-supplied ACEE by
  parking it in ASXBSENV for the length of the RACHECK and restoring it after,
  holding an address-space-wide ENQ on the ASXB to keep concurrent callers out.
  The ENQ only serialized `racf_auth()` against other `racf_auth()` calls: any
  caller switching identity for another reason — which every multi-TCB server
  does, since data set OPEN authorizes against ASXBSENV — was not serialized
  against it, and the save/restore could then leave a foreign ACEE parked there.
  RAKF fails open when it finds zero. The RACHECK parameter list has had an ACEE
  field at offset X'18' all along, which RAKF resolves before falling back to
  ASXBSENV (`SRCLIB/ICHSFR00.hlasm:111-115`), and `racf_login()` already did the
  equivalent for RACINIT. Setting it touches nothing shared, so the check is
  race-free by construction rather than by locking; `acee == NULL` leaves the
  field zero and the ASXBSENV fallback behaves exactly as before, so callers see
  no API change. Consumers that DEQ the ASXB defensively after an ABEND inside
  `racf_auth()` keep working — the DEQ becomes a no-op —
  and can drop that cleanup once they require this version.
  A second defect goes with the ENQ, and it is the one that could be proven on
  target: `lock()` returns 8 when you already hold the lock, `racf_auth()`
  ignored that and DEQd unconditionally, so a caller holding the ASXB lock
  across the call **had it released out from under them** and its own `unlock()`
  then failed. `test/mvs/tstracau.c` case (3), run on MVS 3.8j against both
  libraries: pre-fix `testlock()` comes back 0 (gone) and `unlock()` 8 (not
  ours), COND CODE 0008; fixed, 8 and 0, COND CODE 0000. `racauth.o` no longer
  references `@@LK`/`@@LKUNLK` at all.
- **The PDDB scan was bounded by the read buffer, not by the IOT (#28).**
  `jesjob()` scanned the PDDBs of an IOT from `cp->pddb1` to the end of the
  3664-byte read buffer and relied on hitting a zero `PDBDSKEY` to stop — for a
  spin IOT holding a single PDDB that meant walking 2500 bytes of whatever the
  block happened to contain, on nothing but that terminator. The IOT carries the
  real bound: `IOTPDDBP`, "OFFSET BEYOND LAST PDDB IN IOT" (`haspiot.h`). It is
  used now, treated as an upper bound on the area rather than the address past
  the last entry — measured on the target it is 1828 for an IOT whose PDDBs
  start at 908 and are 104 bytes apart, and 920 is not a multiple of 104, so
  requiring an entry to *fit* below it could drop a legitimate last PDDB.
  Coming out of the same untrusted block, a value that does not land between the
  first PDDB and the buffer is ignored in favour of the old bound, so the scan
  can only ever get tighter. Two things came with it: the buffer clamp now
  leaves room for a whole `__PDDB`, closing an over-read the old bound allowed
  for an entry starting in the last 103 bytes; and the three `spool_read()`
  calls that feed these scans have their return code checked, without which the
  new bound would be read out of the *previous* IOT still sitting in the buffer.
  Verified on MVS 3.8j by running `jesjob()` through `TSTJESLG` before and
  after: 108 DD lines over five job filters, including STCs with spin IOTs,
  byte-identical.
- **Heap over-read walking the records of a block (#23).** The loop test was
  `line->len != EOB && p < eob` — C evaluates `&&` left to right, so the record
  header was dereferenced *before* the bounds test that was supposed to protect
  it. `p` advanced by up to 258 bytes per iteration from a position only
  required to be `< eob`, so a malformed, truncated or foreign block read past
  the end of the `calloc`'d buffer — and handed those bytes to the print
  callback as a line. Every record is now measured against the end of the block
  before anything in it is read. A header that *is* readable and then points
  past the end means the block is malformed and ends that block's walk with the
  new `JESPR_TRUNC`; a tail too short to hold another header is the ordinary end
  of a block — a full block has no room for an EOB byte and a zero-padded one
  walks 3 bytes at a time into exactly that, so reporting either as truncated
  would have cried wolf on every padded block. Verified red→green under
  `-fsanitize=address`: the old walk reports `heap-buffer-overflow, READ of size
  200, 0 bytes after the 128-byte region`.
- **Heap overflow reassembling a spanned line (#24).** Two ways past the end of
  `prbuf`, both now closed. A `MIDDLE`/`LAST` part with no `FIRST` opening the
  line copied into whatever buffer an *earlier, possibly much shorter* line had
  left behind — the `!prbuf` guard only caught the case where no spanned line
  had ever been seen; the walk now tracks whether a line is actually open. And
  the parts were only checked against the announced total *after* the `memcpy`
  had already run past the end. ASan on the old walk: `WRITE of size 60` into an
  8-byte region. The check is against *this* line's announced total, which is
  now tracked separately: `prbuf`'s size is the largest total seen so far
  because the buffer only ever grows, so clamping against it would measure a
  short line against a long predecessor's buffer and let it overrun its own
  announcement by thousands of bytes unnoticed (case (11) pins it). When a block gives up, what was already assembled is handed to
  the callback as a truncated line — a visible fragment beats a line that
  silently disappears — and the reassembly state is then dropped, so the next
  block cannot append across the gap and produce a line that never existed on
  the spool. The four cases are in `test/host/tstjesprb.c` (7)-(10); run it
  under ASan, without it three of them pass against the broken walk too.
- **`__stow()` assembled to nothing (#32, PR #33).** as370 has no STOW operation
  code and there is no `stow` macro in the macro library, so the mnemonic
  produced no instruction at all: the func letter was left in R15 and returned as
  the return code (rc=195 = X'C3' = 'C'). Every PDS directory operation was a
  silent no-op and `__renmem()` never changed a directory entry. The SVC 21
  linkage is now built by hand from `SYS1.MACLIB(STOW)` and `IHBINNRA` — R1 =
  DCB, R0 = area, function encoded by *negating* those registers (LCR), not by a
  function code byte. The build no longer accepts a non-zero as370 return code:
  as370 writes an object file even when it flags statements, and `assemble()`
  only checked that the file existed, so a missing macro shipped a silently wrong
  object into `libc.a`. Verified red and green on MVS 3.8j with
  `test/mvs/tststow.c`, which forces three different STOW return codes (0/8/4)
  out of one data set — old libc returned 195/195/195 with the directory
  unchanged.
- **SVC 99 parmlist built in the caller's storage (PR #19, contributed by
  [@mainframed](https://github.com/mainframed) of
  [MVS-sysgen/RAKF](https://github.com/MVS-sysgen/RAKF)).** `@@SVC99` set the
  high-order bit SVC 99 requires by modifying the caller's parameter list in
  place, relying on that storage being writable. It is not when a cc370 program
  is entered as a TSO command processor: the parameter list cc370 emits sits in
  the module's read-only static storage, so the store took an S0C4 with interrupt
  code 4. Any program opening a data set by DSN faulted at READY while working in
  batch, since `fopen()` reaches SVC 99 through
  `__fpshr`/`__fpold`/`__fpnew`/`__fpstar`. The one-word parmlist is now built in
  the work area the routine already GETMAINs; the caller's storage is never
  written. Found through RAKF's `ADDUSER`, diagnosed and fixed in the same pull
  request — the first outside contribution to this library.
- **The build reused stale generated `.s` (#8).** `compile_c()` skipped `cc370
  -S` whenever the `.s` was newer than its `.c`, which is not a staleness test:
  a `.c` mtime says nothing about the headers it includes, the flags it was
  compiled with, or the code generator that compiled it. A fixed cc370 and an edited `include/*.h` both left the old `.s` in
  place, so `libc.a` kept the old object code with nothing in the build output
  to show a skip — the miscompile was only caught by reading the raw object
  bytes. Every `.c` is now compiled on every build, which costs ~7s for all 712
  and is the whole of what the skip saved; a full `make build` goes from 1.3s to
  8.2s. `make clean` additionally removes the generated `.s` (only those with a
  `.c` sibling), which `rm -rf build/sdk` never did. **If you have built
  libc370 before, rebuild: the installed `libc.a` may contain object code from
  an older compiler.**
- **`__listpd()` leaked one allocation per directory entry (#34, PR #35).** Every
  PDSLIST entry was allocated twice and only the second pointer kept, so the
  first block could be freed neither by the caller nor by `__freepd()` — one
  block lost per entry returned, on every call. The cost is larger than the entry
  size suggests: `USE_MEMMGR` is not defined, so `malloc()` takes the `__getm()`
  path, which rounds every request to `(size + 8 + 63) & ~63` — 64 bytes for a
  member with no user data, 128 with full ISPF statistics, roughly 128 KB per
  listing of a 1000-member PDS. The storage is not reclaimed at the end of the
  request either: a server that runs its programs with `__linkds()` on a pooled
  worker task
  that loops until shutdown, so subpool 0 blocks accumulate for the life of the
  address space, and because `__getm()` issues `GETMAIN RU` the exhaustion
  surfaces as an S80A rather than a NULL from `malloc()`. `-Wall` could not catch it — the variable is used after the second
  assignment, so no dead-store warning fires; a sweep across libc370 and its known callers found no other site.

## [1.0.1] - 2026-07-26

Recovery-path hardening. No interface or ABI change; identical behavior for
well-formed inputs.

### Fixed
- **`cmtt_get_array` MTT-walk bounds (#14, PR #15).** Both walk loops now
  validate the whole entry (`start + 10 + mtentlen <= mttendpt`) and reject a
  negative `mtentlen` (`>= 0`, so a legitimate zero-length entry still advances
  and is not dropped). Fixes the over-read and the
  backward-jump non-termination that drove unbounded `array_add` until GETMAIN
  failed — the mechanism of an S878 in a reader of the console log. Adds
  host regression `test/host/tstcmtt.c` (over-read, backward-jump, zero-length
  survival).
- **SDWACLUP guard on the reachable `failed()` (#16).** Mirrors the cleanup-only
  guard into `@@@try.c` (`___try`, reached by `try()`); v1.0.0's #10 guarded only
  the unreachable `@@try.c` copy. At termination the reachable exit now emits one
  CRT-free WTO and returns RC=0 instead of requesting a retry under a torn-down
  CRT. Review-only (`SDWACLUP` is RTM-set at real termination, not host-testable).
  `recovery()`/`@@abrpt.c` unchanged.

## [0.1.0] - 2026-02-16

Repository restructured for focused MVS C runtime development.

### Changed
- Moved active C modules into `src/` hierarchy:
  `clib`, `cmtt`, `time64`, `dyn75`, `jes`, `racf`, `thdmgr`
- `asm/`, `include/`, `maclib/` remain at top level

### Added
- `doc/` directory for documentation
- `samples/` directory for example programs
- `jcl/` directory for JCL procedures
- `VERSION` file (0.1.0)
- `CHANGELOG.md`

### Removed
- Legacy modules moved to `legacy` branch:
  `emfile`, `ipc`, `miniz`, `modmap`, `os`, `pdf`, `pdf2`, `pdfprt`,
  `resident`, `srb`, `svc`, `test`

See tag `v0.0.0-legacy` for the pre-restructure snapshot.
