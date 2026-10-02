# libc370

The C runtime / **libc for MVS 3.8j** (TK4-, TK5, MVS/CE) — the target library
of the [cc370](https://github.com/mvslovers/cc370) cross-toolchain.

A reentrant C runtime: the standard C library (`stdio`, `stdlib`, `string`,
`time`, …) plus the MVS runtime it is built on (the C startup, GETMAIN-based
storage, dataset I/O) and MVS extras (JES2, ISPF, RACF, SMF, a thread manager).
Since 2.1 the compiler-support routines (cc370's *libgcc*) and the
prologue macros ship with cc370 itself (`libcc370rt.a`), so libc370 needs
**cc370 1.1.0 or later**; every header checks it (`<sys/_cc370.h>`).

Originally created as **crent370** by Michael Dean Rayborn; now the cc370 target
libc, maintained by the [mvslovers](https://github.com/mvslovers) community.

**2.0 reorganised the public headers.** Coming from 1.x, read
[doc/migration-2.0.md](doc/migration-2.0.md); 1.0.8 (tag `v1.0.8`, branch `1.x`)
is the last 1.x release. Crypto (SHA-256, Blowfish, base64) is
[crypto370](https://github.com/mvslovers/crypto370) since 2.0.

## Build & install

Host-native — pure `cc370 -S → as370 → ar370`, **no mbt and no MVS round-trip**.
You need the [cc370](https://github.com/mvslovers/cc370) toolchain built and
installed first (`cc370`, `as370`, `ld370`, `ar370` on `PATH`).

```sh
make install     # build + install into the cc370 sysroot
make             # build only (into build/sdk)
make clean
```

`make install` produces and drops the four things cc370 looks for — all into the
one sysroot it derives from the driver itself (`cc370 -dumpmachine` is `cc370`, so
the sysroot is `<prefix>/cc370/`, default `~/.local/cc370/`):

| Artifact | Location | Effect |
|----------|----------|--------|
| headers (`stdio.h` …, `mvs/`, `ext/`, `ibm/`, `s370/`) | `<sysroot>/include` | `cc370 -c foo.c` finds them with no `-I` |
| `libc.a` (the runtime) | `<sysroot>/lib` | `-lc` pulls it |
| `crt0.o` / `crt1.o` / `crtm.o` | `<sysroot>/lib` | the startup variants (separate startfiles) |
| macros (SYS1.MACLIB + PDP) | `<sysroot>/macros` | `as370` (real binary in `<sysroot>/bin`) finds them via `<exedir>/../macros`, no `-I` |

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
maclib/       PDP / libc370 assembler macros
sysmac/       vendored SYS1.MACLIB members
sdk/          mklibc.py — the build-and-install engine (driven by the Makefile)
```

## License

BSD 2-Clause — see [LICENSE](LICENSE).
