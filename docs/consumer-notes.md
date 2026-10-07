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

`sdk/mklibc.py` compiles every `.c` and assembles every `.asm` under `src/`
(2.0 layout: `src/` mirrors `include/`, `sdk/srcmap.tsv` says where each file
came from). **`attic/` is never built.**

`make install` copies *every* `include/*.h` into the sysroot, whether or not
something implements it. The archive is the authority on what a call will
actually resolve against:

```sh
ar370 t build/sdk/libc.a        # members + every exported symbol
```

Four header families had no member behind them at all — miniz, PDF,
`emfile` and `ipc` — and were removed for 2.0 (#243, #248). `make install`
copies and never deletes, so a sysroot installed before that still carries
them; they compile there and fail at link, as they always did.
`clibsrb.h` left `include/` with them: it had no user, and its
`inline_srb_freemain()` freed from subpool 0. Fixed, it waits in
`attic/mvs/srb.h`, which is not installed. `clibres.h` is
`static __inline` throughout, so it needs no member at all.

Two build-side guards are worth knowing because they change what ships:

* An object that defines `@@MAIN` is excluded from the archive — a stray `main()`
  cannot leak into `libc.a`.
* `as370`'s return code is trusted, not the existence of the `.o`. as370 writes
  an object even when it flagged a statement, and a missing macro silently drops
  the instruction; that is how `__stow()` once shipped as a no-op (#32). A
  flagged assembly now deletes the object and fails the build.

The C startup `@@CRT0` is a member of `libc.a` (#159): a program's `main` refers
to it, so automatic library call brings it in, and cc370 from 1.4.0 names no
startfile on the link line. The only startfile left is `crtm.o`, the nested
startup, which sits outside `libc.a` because it defines `@@CRT0` too — see
[`startup.md`](startup.md).

## VL parameter lists are yours to build

cc370 does not set the high-order bit on the last parameter of a call. Where an
MVS service expects a VL-style list, set it by hand:

```c
txt99[count] = (TXT99 *)((unsigned)txt99[count] | 0x80000000);   /* jesiropn.c:64 */
```

In-tree examples: `src/mvs/jes2/jesiropn.c:64`, `src/stdio/@@fildef.c:75`,
`src/stdio/@@fpfree.c:35` (SVC 99 text-unit lists) and `src/mvs/subsys/iefssreq.c:12`
(SSOB). The same bit marks the last entry of an ECBLIST for `WAIT`
(`src/mvs/ecb/@@ecbtw.c:9`) — different mechanism, same manual bookkeeping.

## Signals are reentrant, but shared per address space

`signal()` / `raise()` keep no writable static in the load module: the handler
table is a heap copy obtained from `__wsaget()` (`src/signal/@@sighdl.c`), keyed by
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

## The console belongs to the program, not to the library

A libc370 routine reports a failure **through its return value**. It does not
WTO, and it does not dump a control block to the operator. Only the caller knows
whether a failure was expected — `__dsalc()` cannot tell an attempt to create a
data set that is meant to exist already from a real environmental error, so it
reports neither and returns the rc either way. On a path where nothing failed
there is not even a judgement call to make: `__txdsn()` dumped the DALMEMBR text
unit to the console on every successful allocation of a DSN with a member, until
#60.

Two things make this stricter here than on a system with a log file:

* On MVS 3.8j the console **is** the SYSLOG. A client retrying in a loop turns a
  per-failure message into a per-attempt one, and #4 measures how little room
  there is.
* An unconditional `wtof()` puts `WTOF` → `VWTOF` → `WTODUMPF` → `WTODUMP` in the
  autocall closure of every module that touches the routine — roughly 3.7 KB of a
  24-bit private region for a message nobody asked for.

Where a diagnostic is worth keeping for the next hunt, the convention is to park
it rather than delete it:

```c
    err = __svc99(&rb99);
#if 0 /* debugging */
    if (err) {
        wtof("%s: __svc99() err=%d", __func__, err);
        wtodumpf(&rb99, sizeof(RB99), "%s RB99", __func__);
    }
#endif
    if (err) goto quit;                         /* src/mvs/dynalloc/@@dsalc.c */
```

A parked block is a note, not working code: it is never compiled, so its format
strings drift out of step with the signatures around it. Expect to fix it up
before it runs again.

The rule is not yet true everywhere. As of #43 there are live `wtof()` /
`wtodumpf()` calls left in the shipped library, and two of them decide the
footprint question above for everyone: `malloc.c` and `@@crtget.c` are pulled by
practically every program, so the WTO chain is linked into practically every
module regardless of what the rest of the library does. The exception the sweep
keeps is the path with no return value to carry the news — an out-of-storage
message on a path that then abends is the only trace anyone gets.

Your own code is the other side of this contract: if you want a failure on the
console, write it there yourself, where you know what it means.

## `racf_auth()` has two "allowed" answers, not one

SAF answers an authorization check with a return code, and **two of its values
mean the access may proceed**:

| rc | meaning |
|----|---------|
| 0  | permitted |
| 4  | the resource is not protected — no profile covers it |
| 8  | not authorized |

So the test is `rc <= 4`, never `rc == 0`. A caller that tests for 0 reads "no
profile exists" as a denial, and on a system where the profile was never
defined that means refusing everything.

Until #63 the distinction was invisible: `racf_auth()` set the wrong flag bit
in the RACHECK parameter list and an unprotected resource always answered 0.
It now asks for `LOG=NONE` with the bit that means it, and answers 4 there —
measured on MVS 3.8j, along with the part that matters more: a user who is
genuinely **not** permitted still answers 8, in every class, with the ACEE
passed in the parameter list and with it reached through ASXBSENV
(`test/mvs/tstracmx.c`).

Whether "no profile exists" *should* mean allow is the caller's policy
decision, not the library's — and it is a decision worth making deliberately
rather than inheriting. A server gating logins on a FACILITY resource that
nobody defined is letting everyone in, which may or may not be what its
operator expects.

One thing that comes with the fix: `LOG=NONE` suppresses RAKF's audit
messages, and on MVS 3.8j the console is the SYSLOG (#4). A busy server no
longer writes two lines per authorization decision. If you *want* the audit
trail, the library can no longer give it to you — say so on the issue and it
becomes a parameter.

## A module run from the LNKLST cannot write its own statics

Deploy a load module into a system library on the LNKLST and it can **read** its
writable statics but not **store** into them — the first store is an S0C4. The
same module, byte for byte, writes them happily when it is fetched from a
private library through a STEPLIB.

Measured on MVS 3.8j (2026-08-06) with a probe that WTOs each step, from
`SYS2.LINKLIB` (APF-authorized and on LNKLST) versus a private LINKLIB:

| link | from LNKLST | via STEPLIB |
|---|---|---|
| `ld370 --ac 1` | reads ok, **S0C4** on store | — |
| `ld370` (no AC) | reads ok, **S0C4** on store | — |
| `ld370 --norent` | reads ok, **S0C4** on store | — |
| `cc370` driver link | reads ok, **S0C4** on store | stores fine |

So it is the library the module is fetched from that decides, not `AC(1)`, not
the RENT attribute, and not who did the linking. That is the same constraint
libc370 lives under, and why `signal()` keeps its handler table on the heap via
`__wsaget()` instead of in the load module (see above): **keep mutable state in
automatic storage or on the heap** if the module may ever be installed
system-wide. One `static int` counter is enough to abend it on the first write.

Consumers deploying into their own LINKLIB and naming it on a STEPLIB — which
is what `mbt`'s `make deploy` produces — are not affected.

Note that stdio buffers are lost on an abend even after `fflush()`: the DCB is
never closed, so the trailing block never reaches the SYSOUT data set. A probe
that may abend should say where it is with `wtof()`, which reaches the job log
immediately; that is how the table above was measured after SYSPRINT came back
empty three times.

## Authorized programs: link AC=1 into an `-iebcopy` member

Anything calling a libc370 routine that issues `MODESET KEY=ZERO,MODE=SUP` —
`racf_login()`, `racf_logout()`, `racf_set_acee()` — has to be linked **AC=1** and
fetched from an APF-authorized library, or the step ends S047.

`racf_auth()` is no longer on that list (#197). It switches only when the
caller is authorized, and otherwise issues the `RACHECK` from problem state,
which SVC 130 answers the same way (`test/mvs/tstracun.c`).

The AC lives in the **directory entry**, not in the module. A bare member file
(`ld370 -o PROG` without `-iebcopy`, or `cc370 -o prog` without
`-flinker-output=`) has no directory entry, so it carries no AC: with and
without `-Wl,--ac,1` it is byte-identical. Link into an `-iebcopy` member,
whose directory entry holds the AC and the entry point, and pack that —
`ld370 --pack` keeps both:

```sh
cc370 -O1 -Iinclude -c prog.c -o prog.o
ld370 --entry @@CRT0 --ac 1 -iebcopy -o PROG prog.o -L<lib> -lc
ld370 --pack PROG=PROG.iebcopy -o out -xmit --dsn <LOADLIB>
```

(`cc370 -Wl,--ac,1 -flinker-output=iebcopy` sets it the same way.)

A module that lost its AC looks exactly like a working one until it runs, and
the S047 arrives with an empty SYSPRINT for the reason above. Filed against the
toolchain as mvslovers/cc370#37.

To check whether the AC took: an authorized program's `WTO` appears in the job
log **without** the `+` prefix that marks a problem-program message.

`test/mvs/tstracau.c` is a worked example of both sections.

## Dataset I/O is BSAM (and EXCP); VSAM is a separate API

`fopen()` and friends go through the assembler dataset layer (`src/stdio/@@aopen.asm`
and the `@@a*` routines). It carries three DCB templates, but only two are
reachable: EXCP for tape, BSAM for everything else — the branch to the QSAM
template is commented out (`*DEFUNCT` at `@@aopen.asm:209`). The open-mode table
at the top of that file is the reference. `fopen()` takes both
`"DD:ddname(member)"` and a dataset name.

**The last block of a data set is written at CLOSE**, so an out-of-space
on it shows up in a close and nowhere else: `fclose()` returns `EOF`
(#182), `rclose()` returns -1 and a `+` stream turning from writing to
reading sets `ferror()` (#228), each with `errno` `ENOSPC` or `EIO`. An
`fflush()` does not write it, and cannot be used to find out early.
**`freopen()` cannot fail on it**, because C99 7.19.5.4 ignores a failure to
close the old file. If the tail matters, set `errno = 0` before the
`freopen()` and treat `ENOSPC`/`EIO` afterwards as a lost tail on the old
file. Any other `errno` value there means nothing. Better still,
`fclose()` + `fopen()`, which report it directly.

VSAM has no stdio path. What exists is `src/mvs/vsam/@@vs*.c`, ACB/MODCB/GENCB via
inline assembler. It is built into `libc.a`, but **no test covers it** — verify
against your own data set before relying on it. The same caveat applies to
`setjmp`/`longjmp` (`include/setjmp.h`, `src/setjmp/longjmp.c`,
`src/setjmp/@@longj.asm`): shipped and built, untested.

## Writing to a TSO TMP: `fopen("*PUTLINE", "w")`

Under the TSO terminal monitor program, PUTLINE is the one correct way to
write a line. In a batch TMP (`PGM=IKJEFT01`) it writes through the TMP's own
SYSTSPRT, in order with the TMP's messages. In the foreground it is meant
for the terminal; that is not measured yet, only the batch TMP is. A second DCB on SYSTSPRT has no ordering against the TMP, and TPUT
does nothing in a batch TMP.

`fopen("*PUTLINE", "w")` (or `"a"`, the name in any case) opens such a stream
(#463):

- Each line is one PUTLINE (`DATA`, `TERMPUT=EDIT`). A line longer than 252
  bytes goes out as several PUTLINE lines.
- The TMP's ECT and UPT come from the LWA, so the stream works for TSO
  `CALL`, for a command processor, and for a program the TMP started any
  other way. No CPPL is needed.
- Without a TMP the open returns `NULL` with `errno` `ENODEV`, so a caller
  can fall back to its DD.
- The name is reserved: it never opens a DD called `PUTLINE`. Reading
  (GETLINE) is not offered yet, `"r"` gives `EINVAL`.

To send all of a program's output there, open it in `__premain()` and set
`stdout` (and `stderr`). The streams the startup would open are then left
alone:

```c
int __premain(char *parm, char *pgmname, void **pgmr1)
{
    FILE *fp = fopen("*PUTLINE", "w");

    if (fp) stdout = fp;        /* no TMP: stdout stays SYSPRINT */
    return 0;
}
```

Measured on MVS: TSO `CALL`, a command and the `__premain()` route each put
their lines into SYSTSPRT between the TMP's prompts; a 300-byte line arrives
whole, wrapped by TSO; plain batch gets `NULL` and `ENODEV`.

## 64-bit arithmetic is software

The target has no native 64-bit integer (`clib64.h`: "our target machine has 32
bit integers maximum"). libc370 ships a small bignum — `__64`, 16-bit limbs, in
`include/ext/int64.h` and `src/ext/int64/@@64*.c` — which is what `src/ext/time64` is built
on. Use it where you would otherwise reach for `long long`.

A `long long` divide or modulo compiles into a call to a compiler helper
(`@@UDIVDI`, `@@DIVDI3`, ...). Since 2.1.0 those live in cc370's
`libcc370rt.a`, not in `libc.a` (#313); the cc370 driver and mbt link it
(`-lcc370rt -lc -lcc370rt`). Only a hand-written `ld370` line without
`-lcc370rt` leaves them unresolved.

### `__64` is big-endian by design — it cannot be tested on a host

`__64` is a union of three views over the same eight bytes, and different
operations read different views:

| operation | view | assumes |
|---|---|---|
| `__64_cmp`, `__64_or`, `__64_copy` | `.u64` | native order |
| `__64_div`, `__64_sub` | `array[]` of `uint16_t` | `array[0]` = most significant halfword |
| `__64_from_u32`, `__64_from_i32`, `__64_to_i32`, `__64_lshift_one_bit` | `u32[]` | `u32[0]` = high word |

On S/370 all three coincide and the code is correct. On a little-endian host
they do not — and the failure is the dangerous kind: the sources **compile,
link and run**, and simply answer wrong. A host build of the `src/ext/time64`
scaling returns 0 for both `/1000` and `/1000000`. Nothing announces that the
harness is measuring nothing, so the obvious next move is to "fix" the expected
values until the test is green.

Word size is not the problem, so `-m32` does not rescue it: an ILP32 x86 host is
still little-endian. Nor is there an arrangement that works — feeding operands
in big-endian `array[]` layout does not help, because `__64_div_u32()` builds
its divisor with `__64_from_u32()` and `__64_div()` compares with `__64_cmp()`
on the way round the loop. Separately, `include/time64.h` refuses to compile at
all under `__LP64__`, and the `@@64*` entry points are S/370 assembler with no
native equivalent.

**The testing contract that follows:** `__64` behaviour — and anything built on
it, which is all of `src/ext/time64` — is verified on MVS (or a big-endian ILP32
target) only. A host test may check expected-value *literals*, but must not
exercise `__64` itself. `test/mvs/tsttm64.c` and `test/host/tsttm64vec.c` are
the worked example: the arithmetic vectors run on the target, and the host
companion includes no libc370 header and calls no libc370 function — it only
re-derives the expected quotients with native 64-bit division, which catches a
transposed digit in the table without pretending to have exercised the library.

This is a property of the design, not a defect.

## Assembler macros are vendored — nothing comes from MVS

`sysmac/` holds the system macros **libc370 itself assembles** — 120 of its 123
members, drawn from SYS1.MACLIB, SYS1.AMODGEN and (for `$pso`, `$pddb`, `$sjb`,
`$cmb`, `$tqe`) HASPSRC. It is deliberately **not** a general SYS1.MACLIB
mirror; the criterion, the three exceptions and what a project does when it
needs something else are below. `maclib/` holds libc370's own macros; they
win on a name collision. `as370` gets both via `-I`, and `make install`
copies them to `<sysroot>/macros`, which an installed as370 finds by default
(otherwise `AS370_MACLIB=`).

So JES2 code assembles on the host with no `SYS1.HASPSRC` and no `MAC2=` anywhere.

**What gets into `sysmac/` — and what does not.** The criterion is *libc370
assembles it itself*: a member belongs here when one of the hand-written
`.asm` under `src/`, one of the `.s` generated from `src/`, or one of the `maclib/`
macros reaches it, directly or as an inner macro. Measured 2026-08-30, that is
**120 of the 123** members. `sysmac/` is not a general SYS1.MACLIB mirror and
must not grow into one — a project that needs a macro libc370 does not use
brings its own and points `as370` at it with `-I`, which is searched **before**
the sysroot, so it still gets the other 120 for free. The alternative — adding
on request — quietly makes libc370 the ecosystem's system macro library and
leaves it maintaining members it never assembles. That was decided on #155,
where a COBOL-74 code generator's output wanted `SPIE`, `TIME`, `WTOR` and
`PUTX`: real needs, all four declined here, because the generator decides its
own macro set and nothing in this repo can anticipate it.

Three members predate the rule and fail it. `GENCB` and `TESTCB` have no user
anywhere in the ecosystem — dead weight, harmless, left alone. `XCTL` does:
rexx370's `asm/irxtmpw.asm:110` (`XCTL EP=IKJEFT01`, a declared build source),
and rexx370 carries no macro directory of its own, so it takes it from
`<sysroot>/macros`. **Removing `xctl.macro` would break that build.** It is the
standing reminder that `<sysroot>/macros` is a published surface: what goes in
is a decision, and what comes back out is a breaking change.

## Crypto

Blowfish, SHA-256 and base64 left libc370 in 2.0 for
[crypto370](https://github.com/mvslovers/crypto370) (#244), still one function
per TU for the reason above — a program that hashes does not drag in the
cipher. `clibb64.h` is `base64.h` there, and the base64 symbols are `B64ENC`/
`B64DEC` instead of `@@B64ENC`/`@@B64DEC`; the C names did not change. Up to
libc370 1.x they are in `libc.a`.

## Linking a server module for httpd

The libc-facing part of the contract (httpd with mbt 2.2.0 and libc370 2.4.0,
its `project.toml`):

* A module names no startup object: `@@CRT0` comes out of `libc.a` (#159). It
  lists `src/cgistart.c` in its `sources`. `cgistart` defines its own `@@START`,
  which wins over libc370's as an explicit object. A CGI or display module
  calls no thread function, so the thread driver `CTHREAD` is not linked and
  its startup issues no IDENTIFY.
* mbt 2.2.0 searches libc370 ahead of a project's dependencies. A module that
  takes its `@@START` from a dependency's archive instead of naming the object
  (mvsMF's module from httpd's library, for example) sets `dep_startup = true`;
  otherwise mbt stops the build.
* `cgistart` opens `HTTPDOUT` / `HTTPDERR` / `HTTPDIN` as `stdout` / `stderr` /
  `stdin` — never `SYSPRINT` / `SYSTERM` / `SYSIN`, which the server needs free
  for the utilities it drives. httpd's own startup enforces this from its
  `__premain()` hook (`src/httpstrt.c`, libc370 2.4.0 and later): if any of the
  three is allocated to the STC it WTOs and ends before `main()`.
* httpd runs a module with LINK (`__linkds()`, `src/httplink.c`) and hands it
  the server through the HTTPX function vector; a module never links against
  server code directly.
