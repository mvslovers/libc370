# Migrating a consumer from libc370 1.x to 2.0

**Status: the maps are complete for phases 1 and 2 on the branch `2.0`; the
migration script that reads them is still to be written (#245).** This
document says what the maps mean and how to apply them, so that a script, an
agent or a person migrates every consumer the same way.

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

Against `v1.0.8`, on 2026-09-30, after phase 2: 5471 public names in 1.x,
4601 in 2.0, 871 gone — 595 with nothing in their place (miniz, PDF, emfile,
ipc, memmgr, the module-map API: declared, never built), 255 now internal,
11 to crypto370, 10 replaced by another name. A sweep of the consumers'
default branches (httpd, mvsmf, ftpd, ufsd, ufsd-utils, httplua, httprexx,
lua370, lstring370, nsf370, brexx370, rexx370, crypto370) found no call of a
gone name outside the crypto functions: the other hits were comments, the
consumers' own identifiers (`RES`, `res`) or their own copies of the printf
engine.
