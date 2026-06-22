# libc370

The C runtime / **libc for MVS 3.8j** (TK4-, TK5, MVS/CE) — the target library
of the [cc370](https://github.com/mvslovers/cc370) cross-toolchain.

A reentrant C runtime: the standard C library (`stdio`, `stdlib`, `string`,
`time`, …) plus the MVS runtime it is built on (the C startup, GETMAIN-based
storage, dataset I/O) and MVS extras (JES2, ISPF, RACF, SMF, a thread manager,
crypto). It is also cc370's *libgcc* — the compiler-support routines live here,
so there is no separate libgcc.

Originally created as **crent370** by Michael Dean Rayborn; now the cc370 target
libc, maintained by the [mvslovers](https://github.com/mvslovers) community.

## Build & install

Host-native — pure `cc370 -S → as370 → ar370`, **no mbt and no MVS round-trip**.
You need the [cc370](https://github.com/mvslovers/cc370) toolchain built and
installed first (`cc370`, `as370`, `ld370`, `ar370` on `PATH`).

```sh
make install     # build + install into the cc370 sysroot
make             # build only (into build/sdk)
make clean
```

`make install` produces and drops the four things cc370 looks for — into the
sysroot it derives from the driver itself (`cc370 -dumpmachine`):

| Artifact | Sysroot location | Effect |
|----------|------------------|--------|
| headers (`stdio.h` … + `clib*.h`) | `<sysroot>/include` | `cc370 -c foo.c` finds them with no `-I` |
| `libc.a` (the runtime) | `<sysroot>/lib` | `-lc` pulls it |
| `crt0.o` / `crt1.o` / `crtm.o` | `<sysroot>/lib` | the startup variants (separate startfiles) |
| macros (SYS1.MACLIB + PDP) | `<sysroot>/macros` | `as370` finds them with no `-I` |

After that the toolchain is self-contained:

```sh
cc370 hello.c -o hello.xmit      # compile + assemble + link + package; runs on MVS via RECV370
```

## Layout

```
src/clib/     C standard library + the C runtime control blocks
src/cmtt/     console message translation
src/dyn75/    SVC 75 dynamic allocation / TCP-IP socket layer
src/jes/      JES2 spool interface
src/racf/     RACF security interface
src/thdmgr/   thread manager
src/time64/   64-bit time functions
asm/          hand-written assembler (incl. @@crt0/@@crt1/@@crtm startups)
include/      C headers
maclib/       PDP / crent assembler macros
sysmac/       vendored SYS1.MACLIB members
sdk/          mklibc.py — the build-and-install engine (driven by the Makefile)
```

## License

BSD 2-Clause — see [LICENSE](LICENSE).
