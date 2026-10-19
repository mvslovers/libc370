# LIBC/370 (libc370)

LIBC/370 is the C runtime / **libc for MVS 3.8j** (TK4-, TK5, MVS/CE) — the target
library of the [CC/370 (cc370)](https://github.com/mvslovers/cc370) cross-toolchain.

A reentrant C runtime: the standard C library (`stdio`, `stdlib`, `string`,
`time`, …) plus the MVS runtime it is built on (the C startup, GETMAIN-based
storage, dataset I/O) and MVS extras (JES2, ISPF, RACF, SMF, a thread manager).
Since 2.1 the compiler-support routines (CC/370's *libgcc*) and the
prologue macros ship with CC/370 itself (`libcc370rt.a`); since 2.8 LIBC/370
needs **CC/370 1.6.0 or later**, and every header checks it (`<sys/_cc370.h>`).

Originally created as **crent370** by Michael Dean Rayborn; now the CC/370 target
libc, maintained by the [mvslovers](https://github.com/mvslovers) community.

**2.0 reorganised the public headers.** Coming from 1.x, read
[docs/migration-2.x.md](docs/migration-2.x.md); 1.0.8 (tag `v1.0.8`, branch `1.x`)
is the last 1.x release. Crypto (SHA-256, Blowfish, base64) is
[crypto370](https://github.com/mvslovers/crypto370) since 2.0.

## Compatibility with CC/370

LIBC/370 2.8 and later needs **CC/370 1.6.0 or later, below 2**. The compiler
helpers (`libcc370rt.a`) and the prologue macros belong to CC/370; headers,
`libc.a` (with the startup `@@CRT0`), `crtm.o` and LIBC/370's own macros to
LIBC/370; SYS1.MACLIB and SYS1.AMODGEN to maclib370, which only an `as370` from
CC/370 1.6.0 finds by itself. Every header checks the compiler
(`#error "libc370 needs cc370 1.6.0 or later"`), and CC/370 in turn needs a
LIBC/370 recent enough for its link line.

| LIBC/370 | needs CC/370 |
|---|---|
| 2.8.x and later | `>= 1.6.0, < 2` |
| 2.4.x - 2.7.x | `>= 1.4.0, < 2` |
| 2.1.x - 2.3.x | `>= 1.1.0, < 2` |
| 2.0.x and older | 1.0.0 |

| CC/370 | needs LIBC/370 |
|---|---|
| 1.4.x and later | `>= 2.3.0` |
| 1.1.x - 1.3.x | `>= 2.1.0` |

The requirement is written once, in `sdk/cc370.json`. Who owns what, why, and
the checklist for cutting a release:
**[internals/releasing.md](internals/releasing.md)**.

## C99 coverage

[docs/c99-audit.md](docs/c99-audit.md) lists every function, macro and type of
the C99 library (clauses 7.2-7.24) with what LIBC/370 provides: declared or
not, as C99 declares it or not, in `libc.a` or not. `sdk/c99audit.py`
regenerates it. The open gaps have issues (#336-#341). `<complex.h>`,
`<fenv.h>` and `<tgmath.h>` are left out on purpose (#342): no project uses
them, and `<fenv.h>`'s IEEE rounding modes and exception flags have no
counterpart in S/370 hexadecimal floating point.

## Installing a release

The easy way is CC/370's `install.sh` from its
[latest release](https://github.com/mvslovers/cc370/releases/latest): it
installs CC/370 and the newest LIBC/370 that fits it into `$PREFIX` (default
`~/.local`), checksums verified.

With Homebrew (macOS, Linux), from the
[mvslovers tap](https://github.com/mvslovers/homebrew-tap):

```sh
brew trust mvslovers/tap            # once: Homebrew 7 loads a dependency only
                                    # from a trusted tap
brew install mvslovers/tap/cc370    # CC/370 and, as its dependency, LIBC/370
brew install mvslovers/tap/libc370  # LIBC/370 (and maclib370) without CC/370
```

Naming a formula trusts it, but only that formula: its dependencies from the
tap need `brew trust`. Up to 2.6.3 LIBC/370 had none, so
`brew install mvslovers/tap/libc370` worked without it (measured on macOS and
Linux: CC/370's `homebrew.yml`, run 37109391271, job "brew libc370 alone,
untrusted tap"). Since 2.7.0 it depends on maclib370 (#490) and needs
`brew trust mvslovers/tap` as well.

The formula installs the sysroot under its `libexec` (`include/`, `lib/`,
`macros/`), which Homebrew does not link beside the host's own C library;
CC/370 links it into its own tree. The formula is rendered by this
repository's release workflow (`sdk/homebrew/libc370.rb.in`).

Each LIBC/370 release (from 2.1.0) also carries the pieces on their own:
`libc370-<v>-sysroot.tar.gz` (unpack into the CC/370 sysroot,
`<prefix>/cc370`), `libc370-dev_<v>_all.deb` and
`libc370-devel-<v>.noarch.rpm` (into `/usr/lib/cc370/cc370`, depending on
CC/370's packages), `libc370-<v>-metadata.json` and `SHA256SUMS`.

From 2.7.0 the packages and the formula also depend on
[maclib370](https://github.com/mvslovers/maclib370) (#490), the SYS1.MACLIB and
SYS1.AMODGEN of MVS 3.8j, which installs into
`<sysroot>/maclib370/macros`. Its `.deb`/`.rpm` come from its own release
page; `apt`/`dnf` install it together with LIBC/370 when the files are named
in one command (with CC/370's package, if CC/370 is not installed yet).
The tarball does not check dependencies: unpack maclib370's tarball into
`<prefix>/cc370/maclib370` yourself.

## Build & install

Host-native — pure `cc370 -S → as370 → ar370`, **no MBT (mbt) and no MVS round-trip**.
You need the [CC/370](https://github.com/mvslovers/cc370) toolchain built and
installed first (`cc370`, `as370`, `ld370`, `ar370` on `PATH`).

```sh
make install     # build + install into the CC/370 sysroot
make             # build only (into build/sdk)
make clean
```

The library is compiled with `-Os`: 2.9% smaller than `-O1`, and the MVS
test series runs identically with either (#344). `LIBC370_OPT=-O1 make`
builds the `-O1` variant for a comparison.

`make install` produces and drops the four things CC/370 looks for — all into the
one sysroot it derives from the driver itself (`cc370 -dumpmachine` is `cc370`, so
the sysroot is `<prefix>/cc370/`, default `~/.local/cc370/`):

| Artifact | Location | Effect |
|----------|----------|--------|
| headers (`stdio.h` …, `mvs/`, `ext/`, `ibm/`, `s370/`) | `<sysroot>/include` | `cc370 -c foo.c` finds them with no `-I` |
| `libc.a` (the runtime) | `<sysroot>/lib` | `-lc` pulls it |
| `crtm.o` | `<sysroot>/lib` | the nested startup (a startfile); the C startup `@@CRT0` is inside `libc.a` |
| macros (vendored SYS1.MACLIB + LIBC/370's `maclib/`; the prologue macros are CC/370's) | `<sysroot>/macros` | `as370` (real binary in `<sysroot>/bin`) finds them via `<exedir>/../macros`, no `-I` |

Before it copies the headers, `make install` removes the ones the previous
LIBC/370 install put there, listed in `<sysroot>/share/libc370/include-files.txt`
(before that list existed: every header name LIBC/370 has shipped), so a header a
release dropped does not linger. It deletes no other file: headers another
toolchain installed into the same `include/` stay. A manual install from the
release tarball should do the same - remove the files that list names, then
unpack - rather than clear `include/`.

After that the toolchain is self-contained:

```sh
cc370 hello.c -o hello.xmit      # compile + assemble + link + package; runs on MVS via RECV370
```

## Layout

```
include/      C headers, installed: ISO C at the top, ext/ (portable extensions),
              mvs/ (MVS API), s370/, ibm/ (IBM data areas), POSIX sys/ netinet/ arpa/
src/          the implementation, mirroring include/ -- src/stdio/, src/ext/time64/,
              src/mvs/jes2/, ...; assembler beside its C (the crt startups in
              src/mvs/crt/); src/net/ is the network family, src/net/dyn75/ its
              DYN75 provider; src/s370/ the 64-bit arithmetic the compiler calls;
              src/internal/ shared private headers, never installed
attic/        code kept but never built
maclib/       LIBC/370 assembler macros
hasp/         JES2 mapping macros from HASPSRC (SYS1.MACLIB is maclib370's)
sdk/          mklibc.py — the build-and-install engine (driven by the Makefile)
```

## License

BSD 2-Clause — see [LICENSE](LICENSE).
