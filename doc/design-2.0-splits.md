# libc370 2.0 phase 1 — the splits (proposal)

**Status: proposal, 2026-09-30, for review in its PR.** Part of #256. Once
agreed, the destinations go into `sdk/headermap.tsv` (the `split` rows get
their per-declaration targets there) and each split lands as its own PR,
gated like the moves.

Every name below was inventoried from the headers with clang's AST (so a
declaration hidden behind a macro or a line break is not missed), and every
"users" figure is a sweep of the twelve consumer repos at their default
branch, fetched 2026-09-30: httpd, mvsmf, ftpd, ufsd, ufsd-utils, httplua,
httprexx, lua370, rexx370, lstring370, nsf370, brexx370. It counts calls in
code, not mentions in comments (a first pass counted both, and mvsMF's
comment about `__fflush()` read as a use). "—" means no consumer calls it.

## Rules the proposal follows

- **ISO/POSIX first.** A standard header gets exactly its standard (D2, #250).
- **Implementation names a standard macro expands to stay in that header.**
  `stdin` expands to `__gtin()`, `errno` to `__errno()`, `setjmp` to
  `__setj()`, `assert` to `__assert()`, the `<ctype.h>` macros to `__isbuf`.
  They are reserved names, which ISO allows there, and a consumer's code
  calls them without naming them.
- **Function names do not change** (#246). Where a declaration moves, its
  symbol stays; where nothing uses a duplicate, the declaration and its TU go
  (#250), and the gate lists the dropped symbols.
- **Internals leave the installed tree in phase 1 already, but only the parts
  a split cuts off**, into a new `src/internal/`. Whole internal headers
  (`clibwsa.h`, `@@memmgr.h`, ...) still move in phase 2, as planned. No
  internal header takes an ISO name (D1's reason applies: a quoted
  `#include "stdio.h"` would find it first).

## 1. `clibio.h` → `stdio.h` + two small headers + internal

`stdio.h` today includes `clibio.h` and nothing else (besides an `#if 0`
block). The split turns that round: `stdio.h` becomes the real header.

| declarations | users | 2.0 |
|---|---|---|
| the ISO part: `FILE`, `fpos_t`, all ISO functions and macros, `_FILE_*` flags (`feof`/`ferror` read them) | all | `stdio.h` |
| `__gtin`, `__gtout`, `__gterr` | via `stdin`/`stdout`/`stderr` | `stdio.h` (macro rule) |
| `__dsalc`, `__dsalcf`, `__dsfree` | httpd, mvsmf, ftpd, ufsd | `mvs/dynalloc.h` |
| `__renmem`, `__delmem` | mvsmf, ftpd / — | `mvs/pds.h` (new, see 5) |
| `__fabandon` | ftpd | `mvs/file.h` (new: MVS extensions to a `FILE`) |
| `__fflush`, `__caller`, `__fflnl`, `__fpswt`, `__fpupc`, `__fgetc`, `__fgets`, `__fputc`, `__fputs`, `__fread`, `__reopen`, `__fseek`, `__fwrite`, `__fpterm`, `__fpfree`, `__fptmp`, `__ddbusy`, `vvprintf`, `vvscanf` | — | `src/internal/fileio.h` |

## 2. `clibstr.h` → `string.h` + `strings.h` + `libc370/strutil.h`

| declarations | users | 2.0 |
|---|---|---|
| the ISO part, `strdup` | all | `string.h` |
| `strcasecmp`, `strncasecmp` | brexx370 | `strings.h` (new, POSIX) |
| `strcpyp`, `memcpyp` | httpd, httplua | `libc370/strutil.h` (new) |
| `__patmat` | httpd, mvsmf, httplua | `libc370/strutil.h` |
| `memclr` | — | `libc370/strutil.h` |
| `stricmp`, `strncmpi` | — | **removed**, with `stricmp.c`, `strncmpi.c` (#250: same code as the POSIX pair) |

## 3. `mvssupa.h` → no `mvs/bsam.h`

The plan expected a public BSAM API here. There is none: every `__a*`
function is used only inside libc370.

| declarations | users | 2.0 |
|---|---|---|
| `__aopen`, `__aread`, `__awrite`, `__aclose`, `__adisc`, `__system` | — | `src/internal/bsam.h` |
| `__getm`, `__freem` | httpd, mvsmf / — | `mvs/storage.h` (new, see 5) |
| `__getclk`, `__gettz` | mvsmf / — | `mvs/clock.h` (new; `mvs/time.h` would break D1) |
| `__svc99`, `__dynal` | ftpd, ufsd, httplua, nsf370 / — | `mvs/dynalloc.h` |
| `idcams`, `__idcams` | ftpd / — | `mvs/idcams.h` (new) |

## 4. `socket.h` → the POSIX headers + `libc370/socket.h`

As #250 lists it, plus `<netdb.h>` and `<sys/select.h>`, which the design
table left out:

| declarations | 2.0 |
|---|---|
| `socket`, `bind`, `connect`, `listen`, `accept`, `send`, `recv`, `getsockname`, `getpeername`, `struct sockaddr`, `struct linger`, `AF_*`, `SOCK_*`, `SOMAXCONN` | `sys/socket.h` |
| `select`, `fd_set`, `FD_*`, `struct timeval` | `sys/select.h` |
| `struct in_addr`, `in_addr_t`, `struct sockaddr_in`, `IPPROTO_*`, `INADDR_*`, `htonl`/`htons`/`ntohl`/`ntohs` | `netinet/in.h` |
| `inet_aton` (and #51's `inet_addr`/`inet_ntoa` when they come) | `arpa/inet.h` |
| `gethostbyname`, `gethostbyaddr`, `struct hostent` | `netdb.h` |
| `closesocket`, `ioctlsocket`, `FIONBIO`, `FIONREAD`, `selectex`, `getaddrbyname`, `struct clientid` — non-POSIX (#250) | `libc370/socket.h` |
| `CLIBSOCK` and `__so*` (from `clibsock.h`) — httpd walks `grt->grtsock` as `CLIBSOCK *` | `libc370/socket.h` (question A) |
| `__75*`, `__75vect`, `OR`/`BOR` | `src/internal/dyn75.h` |

`sys/`, `netinet/` and `arpa/` are new directories under `include/`; the
install copies them since #257.

## 5. `clibos.h` → by topic

| declarations | users | 2.0 |
|---|---|---|
| `__cas`, `__swap`, `__inc`, `__dec`, `__uinc`, `__udec` | ufsd, nsf370 | `s370/atomic.h` (new: CS/CDS, the architecture) |
| `BLDL`, `DE12`/`DE14`/`DE76`, `__bldl`, `__stow` | — | `mvs/pds.h` (new, with `__renmem`/`__delmem`) |
| `__ascb`, `__xmpost` | ufsd, nsf370 | `mvs/xmem.h` (new: across address spaces) |
| `__super`, `__prob`, `__pswkey`, `PSWKEY*`, `__isauth`, `__issup`, `__sudo`/`super_do`, `__sukydo`/`super_key_do`, `clib_apf_setup`, `clib_auth_cde`, `clib_auth_name`, `__steplb` | httpd, ftpd, ufsd, nsf370, brexx370 | `mvs/apf.h` (already there with `__autask`/`__austep`) |
| `getmain`, `freemain`, `__setsp`, `__getsp`, `__getmsp` | httpd, mvsmf, ufsd, rexx370, nsf370 | `mvs/storage.h` (new, with `__getm`/`__freem`) |
| `__load`, `__delete`, `__loadhi`, `clib_find_cde`, `__call` | httprexx, rexx370, ufsd, nsf370 | `mvs/link.h` (already there) |
| `clib_identify_cthread` | nsf370 | `mvs/thread.h` |

## 6. Non-standard names in the ISO headers

| name | today | users | 2.0 |
|---|---|---|---|
| `sleep` | `time.h` | httpd, brexx370 | `unistd.h` (new, POSIX) |
| `usleep` | nowhere (exists) | httpd, which declares it itself | `unistd.h` (#250 item 1) |
| `setenv`, `unsetenv`, `putenv` | `mvs/env.h` | rexx370, brexx370 | `stdlib.h` (#250) |
| `bcopy` | `stdlib.h` | — | **removed**, with `bcopy.c` (#250: gone from POSIX in 2008) |
| `__tzset`, `__tzget` | `time.h` | — / httpd, mvsmf | stay (reserved names; `__tzget` is used) |
| `__userExit` | `stdlib.h` | — | `src/internal/` |
| `vvprintf`, `vvscanf` | `stdio.h` | — | `src/internal/fileio.h` (see 1) |

The inventory above is of functions and variables. The macros each ISO
header defines get the same check in the PR that splits it.

## Questions for review

- **A. `CLIBSOCK`.** httpd reads libc370's socket table directly. Proposal:
  `libc370/socket.h` declares it, as it is part of what the DYN75 socket
  layer exposes; an accessor instead would be an API change for 2.x.
- **B. `clibthdi.h` is not internal.** The table marks it internal, but httpd
  and ftpd use the thread-manager API itself (`cthread_manager_init`,
  `cthread_queue_add`, the `CTHDMGR`/`CTHDWORK` structs and states).
  Proposal: merge it into `mvs/thread.h`, which it already includes, rather
  than `src/internal/`.
- **C. `clib64.h`** moved to `libc370/int64.h` as planned. Retiring it for
  `long long` changes code and symbols, so it is not phase-1 material; it
  stays a 2.x question (cc370#467/#468 still affect 64-bit shifts).
- **D. `src/internal/` in phase 1.** Only for the parts a split cuts off;
  `mklibc.py` and the host-test recipes get `-I src/internal`. Phase 2 then
  moves the whole internal headers into the directory that already exists.

## Order of the PRs

1. `clibos.h` and `mvssupa.h` (they create `mvs/storage.h`, `mvs/pds.h` and
   the other new `mvs/` headers the rest refers to)
2. `clibstr.h` + the `strings.h`/`unistd.h`/`stdlib.h` moves of section 6
3. `clibio.h`
4. `socket.h` + `clibsock.h`
5. `clibthdi.h` into `mvs/thread.h`, if B is agreed

Each keeps the gate: identical assembler for every TU; where a declaration
is removed (`stricmp`, `strncmpi`, `bcopy`), exactly those TUs and symbols
are allowed to go.
