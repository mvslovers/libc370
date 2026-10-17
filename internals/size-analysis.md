# Size analysis of libc.a (2026-10-16, libc370 2.16.0)

What a C program links from libc.a, and which references hold the most of it in
place. Follow-up to #564, after 2.16.0 made the start-up small: an empty
`main()` now links 14.8 KB, but a program that uses stdio still links about
73 KB.

## Method

Twelve small programs stand for typical uses. Each one was linked against
libc370 2.16.0 with `cc370 -O1 ... -Wl,--map,X.map,--xref`, and the maps were
read with `sdk/sizemap.py`. The tool builds the reference graph between
library members and computes:
- the size;
- for every single reference, how much would be left out if that reference
  were gone;
- the size after a set of references is removed, which models a fix.

A cut models removing the reference, not the code that would replace it. The
figures under "after" are therefore upper bounds on the saving.

| program | what it does |
|---|---|
| p01empty | `int main(void){return 0;}` |
| p02puts | `puts("HELLO")` |
| p03printf | `printf("%s %d\n", ...)` |
| p04sprintf | `sprintf()` into a buffer, then `wto()` - no stream |
| p05fread | `fopen("DD:IN","r")`, `fgets()` loop, `fclose()` |
| p06fwrite | `fopen("DD:OUT","w")`, `fputs()`, `fclose()` |
| p07string | `malloc()`, `strcpy()`, `strcat()`, `strlen()`, `free()` |
| p08time | `time()`, `localtime()`, `strftime()`, `wto()` |
| p09ll | `long long` multiply, divide, remainder, shift |
| p10thread | `cthread_create()` |
| p11sock | `socket()` |
| p12getenv | `getenv("FOO")` |

## Findings

**F1 - `fopen()` links all of printf** (`fopen -> sprintf`). Holds 14.8-15.5 KB
in every program that opens a stream, including `stdout` through `puts()`. The
only use is `appmem()` in `src/stdio/fopen.c`, which builds
`"DD:%.*s(%.*s)"` for a probe open. Building the string by hand removes it.
Small change, low risk.

**F2 - formatting into a string links the stream layer**
(`@@examin -> @@fputc`, `vvprintf -> @@fputc`, `vvprintf -> @@fputs`). The one
formatter serves both `vfprintf()` and `vsprintf()` and calls `__fputc()` and
`__fputs()` by name. `__fputc()` brings in `__fpswt()`, and with it
`__fpopen()`, `__aopen()`, `__fflush()` and `__fgetc()`: 17.2 KB for a program
that only formats into a buffer, and the same 17.2 KB in every program whose
internal diagnostics use `wtof()`. The fix is to pass the output routine as a
pointer, so that only the `FILE` entry points (`vfprintf()` and friends) name
`__fputc()`. Medium change: the formatter is the hot path of all output.

**F3 - the environment links stdio** (`loadenv -> fopen`). `getenv()` loads
SYSENV on first use since 2.16.0, through `fopen()`/`fgets()`. That puts 33.9 KB
of stdio into any program that reads an environment variable. That includes
every program using a time function, because `tzset()` reads `TZ`:
p08time is 78.8 KB. Reading the DD with the record layer (`ropen()`/`rread()`)
or BSAM directly keeps it out. The saving shown leaves out the reader that
replaces it, a few KB. Medium change.

**F4 - internal diagnostics through `wtof()`/`sprintf()` outside the
start-up.**
- Thread start (`@@ctcrtx`, `@@ctdel`), the abend report (`@@abrpt`), the lock
  routine `@@lkrnuf` (`vsprintf`) and the 64-bit helpers `@@64mul` and
  `@@64lshw` all reach the printf family that way (`wtodump`/`wtodumpf`
  too, but only callers that dump storage link them).
- p10thread: 33.2 KB of its 58.5 KB.
- Same remedy as in 2.16.0: `src/internal/wtofmt.h` and `wto()`. Mechanical,
  low risk per site.

**F5 - `fopen()` links every kind of name.** `__stdlz()` opens `stdout` with
`fopen("*SYSPRINT")`. `fopen()` references the handlers for every name form:
- data set by name (`__fpnew`, `__fpold`, `__fpshr`);
- temporary (`__fptmp`);
- TSO (`__fpput`, `__fpget`, `__getpfx`).

A program that only writes to `stdout` needs none of them. A lean open path
for the standard streams, or the handlers referenced weakly through a table
that only the name parser fills, saves about 10 KB more for `puts`/`printf`
programs. Medium change.

**Kept on purpose:**
- **`malloc()`'s out-of-storage trace back** (`@@wtotb -> @@@try ->
  @@estae`, 3.2-4.8 KB in every program). It is the only evidence of where a
  shortage happened.
- **`printf()` always links the floating-point conversions** (`@@dblcvt`,
  2.1 KB, and the 64-bit division they use). The format string is only known
  at run time, so the alternative would be a second `printf()` without
  floating point, which is out of scope here.

## Estimated effect

Bytes linked from libc.a, now and with each fix (cut model, upper bound):

| program | now | F1 | F2 | F3 | F4 | F1-F4 | F1-F5 |
|---|---|---|---|---|---|---|---|
| p01empty | 14,806 | - | - | - | - | 14,806 | |
| p02puts | 73,856 | -14,812 | - | - | - | 59,044 (-20%) | 48,712 (-34%) |
| p03printf | 73,888 | - | -716 | - | - | 73,172 (-1%) | 62,840 (-14%) |
| p04sprintf | 48,616 | - | -17,194 | - | - | 31,422 (-35%) | |
| p05fread | 72,604 | -15,528 | -716 | - | - | 57,076 (-21%) | |
| p06fwrite | 73,608 | -15,016 | -204 | - | - | 58,592 (-20%) | |
| p07string | 15,070 | - | - | - | - | 15,070 | |
| p08time | 78,808 | -15,528 | -716 | -33,788 | - | 45,020 (-42%) | |
| p09ll | 17,258 | - | - | - | - | 17,258 | |
| p10thread | 58,548 | - | -17,194 | - | -33,214 | 25,334 (-56%) | |
| p11sock | 15,914 | - | - | - | - | 15,914 | |
| p12getenv | 72,512 | -15,528 | -716 | -33,912 | - | 38,600 (-46%) | |

The columns don't add up: the fixes overlap, because several of them
cut paths to the same members.

## Done

**F1 and F4** (branch `feature/size-f1-f4`, 2026-10-17). The mutex functions
build their ENQ name through `src/internal/mtxname.h` and no longer call
`lock_resf()` and the other `_resf` functions, so `@@lkrnuf` drops out with
them. Measured against 2.16.0, in bytes linked from libc.a:

| program | 2.16.0 | F1+F4 |
|---|---|---|
| p02puts | 73,856 | 59,036 |
| p03printf | 73,888 | 72,264 |
| p04sprintf | 48,616 | 47,260 |
| p05fread | 72,604 | 57,068 |
| p06fwrite | 73,608 | 58,584 |
| p08time | 78,808 | 63,272 |
| p10thread | 58,548 | 27,666 |
| p12getenv | 72,512 | 56,976 |

p01empty, p07string, p09ll and p11sock are unchanged. On MVS (mvsdev
JOB02398/02400/02402, `test/mvs/tstf14.c`): the member refusal, the mutex between
two tasks, the dummy-thread report and the abend report behave and read the
same as with 2.16.0. The abend report is identical line for line once the
addresses are masked.

**F3** (branch `feature/size-f3`, 2026-10-17). `__envlz()` reads the DD
through `__envdd()` (`__aopen()`/`__aread()`), and the line rules moved
from `loadenv()` to `__envln()`, which both use; `@@fpopen.c`'s DUMMY
check became `__dddum()` for both. Against F1+F4:

| program | F1+F4 | F3 |
|---|---|---|
| p08time | 63,272 | 32,957 |
| p12getenv | 56,976 | 25,025 |
| p05fread | 57,068 | 57,468 |

p05fread grows by 400 bytes: it still links `getenv()` through `fopen()`
(`@@fpstar`), and `@@envdd` plus `@@envln` take the place of `loadenv`.
The rest move by less than 300 bytes. On MVS (mvsdev JOB02404,
`test/mvs/tstenvdd.c`): FB with comments, sequence numbers and trailing
blanks, the fallback to ENVIRON, a DUMMY SYSENV, a VB SYSENV, a SYSENV
held open by `fopen()` (#184), and no DD all give the same environment as
with 2.16.0.

**F2** (branch `feature/size-f2`, 2026-10-17). `vvprintf()` and
`__examin()` take the output functions as arguments: `vfprintf()` passes
`__fputc` and `__fputs`, `vsprintf()` and `vsnprintf()` pass NULL. Against
F3: p04sprintf 47,272 -> 30,386, p03printf 72,240 -> 72,696, the rest
unchanged. The printf programs grow by 456 bytes: 112 for the two
arguments, 344 for splitting `vvprintf()` into one function per job
(#577, cognitive complexity 55 -> under 25). On MVS (mvsdev JOB02408/02410,
`test/mvs/tstfmt.c`): `printf()`, `fprintf()`, `sprintf()` and
`snprintf()` of the same set of conversions give byte-identical SYSPRINT
and OUT against 2.16.0, with %n, truncation and EBADF checked.

## Proposed order

1. **F1 and F4.** Mechanical and low risk, the same technique as 2.16.0. F1
   alone takes 15 KB off every stdio program.
2. **F3**, the environment without stdio. Time functions and `getenv()` no
   longer pull in streams.
3. **F2**, the formatter with an output routine. Its larger saving needs
   care, since it sits on every output path.
4. **F5**, a lean open path for the standard streams.

Each with a red/green MVS run of the behaviour it touches, as for #564.

## Reproducing

```sh
make build
cc370 -O1 -L build/sdk -Iinclude prog.c -o P -Wl,--map,prog.map,--xref \
      -flinker-output=iebcopy
python3 sdk/sizemap.py < prog.map              # size and the costliest references
python3 sdk/sizemap.py --path @@fpopen < prog.map
python3 sdk/sizemap.py --cut fopen:sprintf vvprintf:@@fputc < prog.map
```
