# Migrating a consumer from libc370 1.x to 2.0

**Status: the maps are complete on the branch `2.0`.** This document says
what the maps mean and how to apply them, so that an agent or a person
migrates every consumer the same way. There is no migration script: an agent
does it, briefed by `doc/migration-2.0-agent.md` (decided 2026-10-01).

2.0 is a hard cut (D4 in `doc/design-2.0.md`): no compatibility headers, no
deprecated names. Function names do not change — only where they are
declared — with the exceptions listed in `sdk/removed.tsv`.

## The maps

All three are tab-separated, one row per entry, comments start with `#`.
`sdk/headermap.py check` keeps them consistent with the tree and runs in CI.

| file | one row per | maintained |
|---|---|---|
| `sdk/headermap.tsv` | 1.x header: `today  target  users  note` | by hand |
| `sdk/names.tsv` | public name in 2.0: `name  header(s)` | generated from `include/` |
| `sdk/removed.tsv` | 1.x name no public header declares any more: `name  was  now  note` | by hand, enforced |

`sdk/names-1.x.tsv` is the frozen list of every public name in `v1.0.8`,
the other side of the comparison that `sdk/removed.tsv` must cover.

A header's `target` in `sdk/headermap.tsv` is one of

- a path under `include/` — the header moved; include it by that name
- the same name — an ISO C header, unchanged
- `split` — its declarations went to several headers; resolve by name
- `internal` — it left the sysroot for `src/`; a consumer cannot include it
- `removed` — gone; the note says why

A row's `now` in `sdk/removed.tsv` is one of

- `<hdr> name` — use that name from that header instead (read the note:
  `bcopy` → `memmove` swaps the first two arguments)
- `crypto370 <hdr> name` — the name moved to crypto370: add
  `"mvslovers/crypto370"` to `[dependencies]` and include its header
- `internal` — still in libc370, but private; a consumer that uses it needs
  a decision, not a rewrite
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
   apply `now`. Rows with `internal` or `-` stop the automatic migration
   for that file and go to a person, with the note.
5. **Keep the delimiter convention**: public headers as `<…>`. No consumer
   ever includes `"src/…"` — those paths do not exist in the sysroot.

A consumer's own header that shares a basename with a libc370 header (httpd
has its own `RES`, brexx370 its own `printf.h`) is not libc370's: resolve
includes against the consumer's own tree first.

## Changed meaning — a person decides

The maps place names; they cannot say that a name kept its place and changed
what it means. Those cases are listed here. A script finds the uses and hands
them over; it does not rewrite them.

| name | 1.x | 2.0 | known uses (2026-10-01) |
|---|---|---|---|
| `in_addr_t` | `struct in_addr` | `unsigned long`, the address itself | httpd `credentials/src/credtest.c:47` casts to `in_addr_t *` for `inet_aton()`: becomes `struct in_addr *`; brexx370 `compat/jccompat.c:368`, whose `inet_addr()` libc370 now provides |
| `inet_aton()` | 0 = address, -1 = none; second argument `in_addr_t *` | 1 = address, 0 = none (BSD); `struct in_addr *` | httpd `credtest.c` ignores the result; brexx370 already expects BSD's |
| `__listpd()` | storage running out returned the records collected so far | NULL with `errno` `ENOMEM` | ftpd `ftpd#mvs.c:941` (`LIST`/`NLST`) reports NULL as "No data sets found" -- better moved to `__walkpd()`; `:1191` and mvsMF `dsapi.c:736` look up one member and are not affected |

## New in 2.0 that a consumer asked for — optional

Nothing here has to change for a consumer to build against 2.0; each entry is
something a consumer worked around, which it may now drop.

| new | replaces | consumer |
|---|---|---|
| `__walkpd()` in `<mvs/dslist.h>` (#80): a PDS directory member by member through a callback, nothing allocated, the callback may stop | ftpd: `__listpd()` for `LIST`/`NLST`, one allocation per member (the region on a large PDS); mvsMF: its own copy of the directory walk (`dsapi.c`, "mirrors the walk in libc370's `__listpd()`") | ftpd, mvsMF |
| `idcams_sysprint()` in `<mvs/idcams.h>` (#71): every SYSPRINT line with its IDC message number, through a callback; `idcams()` is unchanged | mapping `idcams()`'s condition code to words, which cannot tell "not found" (IDC3012I) from "refused" (IDC3203I) | ftpd (ftpd#87); mvsMF calls `idcams()` too |
| `JESJOB.submit_time64` and `JESJOB.sysid` in `<mvs/jes2.h>` (#79): when the job was submitted, and on which system | nothing -- z/OSMF's `exec-submitted` stayed empty, `exec-system` unanswered | mvsMF (mvsmf#208, mvsmf#209) |
| `inet_addr()`, `inet_pton()`, `inet_ntop()`, `inet_ntoa()` in `<arpa/inet.h>` (#51) | ftpd's `sscanf("%u.%u.%u.%u")`; brexx370's own `inet_addr()` and `inet_ntoa()` | ftpd, brexx370 |
| `DSLIST.catnm` in `<mvs/dslist.h>` (#50): the catalog `__listds()` found each entry in, from LISTCAT's `IN-CAT` line; NULL when unknown. Owned by the list, freed by `__freeds()` | mvsMF `dsapi.c:1572` writes `"catnm": ""` for every entry | mvsMF |
| `fopen(dsn, "w…,unit=…,volser=…")` and `DATASET_UNIT` / `DATASET_VOLSER` (#172): a data set `fopen()` creates goes to the unit and volume named; one that is not mounted is refused at once (`S99NOMNT`) instead of waiting on `IEF238D`. Without them the request is the 1.x one | ftpd `STOR` (`ftpd#mvs.c:1972-1994`): `__dsalcf()` with `UNIT=`/`VOLSER=`, `__dsfree()`, then `fopen("wb,rlse")` on the data set by name. The detour also opens a window in which an abend leaves an empty data set catalogued (the comment at `:1985`), and `__dsalcf()` still waits on `IEF238D` (#305) | ftpd |

## Verifying a migrated consumer

- It builds against the 2.0 sysroot with `-Wall -Werror`. An implicit
  declaration means step 3 missed a name.
- Its generated assembler is identical to the 1.x build, TU by TU, except
  where step 4 replaced a name on purpose, or where the TU uses a struct or
  signature the 2.0 interface changes changed (each is a CHANGELOG entry) — the
  method of libc370's own `sdk/gate.py`. Any other difference is a missed
  include, not noise.

## For libc370 developers

A change that moves, renames or removes a public header or name updates the
maps in the same PR: `sdk/headermap.tsv` by hand, `python3 sdk/names.py
render`, and a row in `sdk/removed.tsv` for each name that leaves the public
headers. CI refuses the PR otherwise — a name that disappears without a row,
a row whose replacement does not exist, or a stale `sdk/names.tsv`.

## Measured

Against `v1.0.8`, on 2026-10-01 (after phases 2 and 3, #51 and #71): 5481
public names in 1.x, 4610 in 2.0, 880 gone — 603 with nothing in their place
(miniz, PDF, emfile, ipc, memmgr, the module-map API: declared, never built),
256 now internal, 11 to crypto370, 10 replaced by another name. A sweep of the consumers'
default branches (httpd, mvsmf, ftpd, ufsd, ufsd-utils, httplua, httprexx,
lua370, lstring370, nsf370, brexx370, rexx370, crypto370) found no call of a
gone name outside the crypto functions: the other hits were comments, the
consumers' own identifiers (`RES`, `res`) or their own copies of the printf
engine.
