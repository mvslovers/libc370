# Writing code against libc370

The conventions and constraints a caller runs into that neither the compiler nor
the headers announce. Building and installing the library is in
[`README.md`](../README.md); the startup modules are in
[`startup.md`](startup.md); the toolchain itself is documented in
[cc370](https://github.com/mvslovers/cc370).

## External names are 8 characters

An OS/360 object deck carries external names in 8-byte EBCDIC ESD fields
(cc370, `docs/object-module-format.md`). There is nowhere to put a longer name,
so every function libc370 exports is given an explicit link name in the header:

```c
CTHDTASK *cthread_create(void *func, void *arg1, void *arg2)  asm("@@CTCRTE");
void      cthread_delete(CTHDTASK **task)                     asm("@@CTDEL");
```

About 450 of these live in `include/`. Consequences worth knowing:

* The C name and the link name are independent. A link map, an ld370
  "unresolved ER" message and an ABEND all show the 8-character name — grep
  `include/` for it to get back to the C function.
* Two C functions must not carry the same alias; the linker only sees the alias.
* The repo names each source file after its alias in lowercase
  (`@@ctcrte.c` → `@@CTCRTE`). Autocall resolves by symbol, not by file name, so
  a mismatch links fine — it just makes maps unreadable.

Your own code needs the same treatment for anything it exports.

## Autocall pulls whole members — one function per translation unit

`ar370` writes `libc.a` with a symbol index over every member's ESD entries;
`ld370` resolves unresolved ERs by pulling the **entire member** that defines the
symbol. The unit of granularity is therefore the object file, i.e. the
translation unit.

Put two functions in one `.c` and a caller that needs one gets both — code and
its static data — in the load module. On a 24-bit target, where the private
region is the scarce resource, that is the difference that matters, which is why
libc370 is 712 mostly one-function TUs. Apply the same rule to your own `.a`.

## What is actually in `libc.a`

`sdk/mklibc.py` compiles `src/{clib,cmtt,crypto,dyn75,jes,os,racf,smf,thdmgr,time64}`
plus `asm/*.asm`. **`src/wip/` is never built.**

`make install` copies *every* `include/*.h` into the sysroot, whether or not
something implements it. The archive is the authority on what a call will
actually resolve against:

```sh
ar370 t build/sdk/libc.a        # members + every exported symbol
```

Today it exports 740 symbols, and none of them back these headers: `clibmz.h` /
`clibmzi.h` (miniz) and `clibpdf.h` / `clibpdfi.h` compile and then fail at link.
`clibsrb.h` is a half case — its two out-of-line entries (`SRBGMAIN`, `SRBFMAIN`)
are not in the archive, but the `inline_srb_*` variants beside them are
`static __inline` and work. `clibres.h` is `static __inline` throughout, so it
needs no member at all.

Two build-side guards are worth knowing because they change what ships:

* An object that defines `@@MAIN` is excluded from the archive — a stray `main()`
  cannot leak into `libc.a`.
* `as370`'s return code is trusted, not the existence of the `.o`. as370 writes
  an object even when it flagged a statement, and a missing macro silently drops
  the instruction; that is how `__stow()` once shipped as a no-op (#32). A
  flagged assembly now deletes the object and fails the build.

`crt0.o`, `crt1.o` and `crtm.o` are startfiles that deliberately sit **outside**
`libc.a`, so the linker takes exactly one — pick it per program, see
[`startup.md`](startup.md).

## VL parameter lists are yours to build

cc370 does not set the high-order bit on the last parameter of a call. Where an
MVS service expects a VL-style list, set it by hand:

```c
txt99[count] = (TXT99 *)((unsigned)txt99[count] | 0x80000000);   /* jesiropn.c:64 */
```

In-tree examples: `src/jes/jesiropn.c:64`, `src/clib/@@fildef.c:75`,
`src/clib/@@fpfree.c:35` (SVC 99 text-unit lists) and `src/clib/iefssreq.c:12`
(SSOB). The same bit marks the last entry of an ECBLIST for `WAIT`
(`src/clib/@@ecbtw.c:9`) — different mechanism, same manual bookkeeping.

## Signals are reentrant, but shared per address space

`signal()` / `raise()` keep no writable static in the load module: the handler
table is a heap copy obtained from `__wsaget()` (`src/clib/@@sighdl.c`), keyed by
the address of the static initializer and held in an array on the **GRT**.

The GRT is per address space, so the handler table is too — it is *not*
per-thread. A handler installed on one thread is in force for every thread in the
address space. Reentrancy is safe; isolation between threads is not there.

For failures, the library's own mechanism is ESTAE, not signals:

```c
int rc = try(func, arg);   /* include/clibtry.h; rc = 0x00sssuuu, sss/uuu = abend code */
```

`try()` expands to `___try`; a near-duplicate `__try` also exists and is dead —
see #17.

## Dataset I/O is BSAM (and EXCP); VSAM is a separate API

`fopen()` and friends go through the assembler dataset layer (`asm/@@aopen.asm`
and the `@@a*` routines). It carries three DCB templates, but only two are
reachable: EXCP for tape, BSAM for everything else — the branch to the QSAM
template is commented out (`*DEFUNCT` at `@@aopen.asm:209`). The open-mode table
at the top of that file is the reference. `fopen()` takes both
`"DD:ddname(member)"` and a dataset name.

VSAM has no stdio path. What exists is `src/clib/@@vs*.c`, ACB/MODCB/GENCB via
inline assembler. It is built into `libc.a`, but **no test covers it** — verify
against your own data set before relying on it. The same caveat applies to
`setjmp`/`longjmp` (`include/setjmp.h`, `src/clib/longjmp.c`,
`asm/@@longj.asm`): shipped and built, untested.

## 64-bit arithmetic is software

The target has no native 64-bit integer (`clib64.h`: "our target machine has 32
bit integers maximum"). libc370 ships a small bignum — `__64`, 16-bit limbs, in
`include/clib64.h` and `src/clib/@@64*.c` — which is what `src/time64` is built
on. Use it where you would otherwise reach for `long long`.

## Assembler macros are vendored — nothing comes from MVS

`sysmac/` holds the SYS1.MACLIB members (including the JES2 ones: `$pso`,
`$pddb`, `$sjb`, `$cmb`, `$tqe`), `maclib/` the crent/PDP macros; the crent
macros win on a name collision. `as370` gets both via `-I`, and `make install`
copies them to `<sysroot>/macros`, which an installed as370 finds by default
(otherwise `AS370_MACLIB=`).

So JES2 code assembles on the host with no `SYS1.HASPSRC` and no `MAC2=` anywhere.

## Crypto

Blowfish (`bfishkey`, `bfishenc`, `bfishdec`) and SHA-256 (`sha256i`, `sha256u`,
`sha256t`, `sha256f`) are in `src/crypto` and in `libc.a`, one function per TU
for the reason above — a program that hashes does not drag in the cipher.

## Linking a server module for httpd

The libc-facing part of the contract (httpd 4.0.0, its `project.toml`):

* Each module links `startup = "crt1"` plus `src/cgistart.c`. `crt1` is the full
  runtime with thread creation disabled — a module builds its own C runtime and
  does not attach threads of its own.
* `cgistart` opens `HTTPDOUT` / `HTTPDERR` / `HTTPDIN` as `stdout` / `stderr` /
  `stdin` — never `SYSPRINT` / `SYSTERM` / `SYSIN`, which the server needs free
  for the utilities it drives. httpd's own `httpstrt.c` enforces this: if any of
  the three is allocated to the STC it WTOs and exits before starting.
* httpd `__load()`s the modules at startup and calls them through the HTTPX
  function vector; a module never links against server code directly.
