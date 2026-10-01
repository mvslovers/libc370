# Brief: migrate one consumer to libc370 2.0

Hand this to an agent working in a consumer repository, with `<REPO>` filled
in (e.g. `mvslovers/ufsd`). It is the whole instruction; everything else is
where it says to read. Per-consumer notes follow the brief.

---

You migrate **`<REPO>`** from libc370 1.x to 2.0. Work on a branch
`libc370-2.0`. Do not commit, push, open a PR or touch MVS without the
maintainer's OK.

## What happened in libc370

- 2.0 is a **hard cut** on the branch `2.0` of mvslovers/libc370 — not on
  `main`, not released. Its state is the rolling tag **`v2.0.0-dev`** (moved
  forward after merges into `2.0`). 1.0.8 is the last 1.x and what consumers
  build against today; `edge` is 1.x too — do not use it.
- **Headers** moved (`<clibwto.h>` → `<mvs/wto.h>`), were merged, split
  (`clibos.h`, `clibio.h`, `clibstr.h`, `socket.h`, `mvssupa.h` have no single
  successor) or left the sysroot. There are no compatibility headers.
- **Function names did not change**, except the ones in `sdk/removed.tsv`.
- **Some interfaces changed on purpose** (`JESJOB` grew, `DSLIST.catnm`,
  `in_addr_t`/`inet_aton()`, `__listpd()` NULL on ENOMEM, …): CHANGELOG
  `[Unreleased]` → *Changed*, and the *Changed meaning* table in
  `doc/migration-2.0.md`.
- **Crypto** (SHA-256, Blowfish, base64) moved to mvslovers/crypto370 1.0.0;
  `clibb64.h` is `base64.h` there.

## Read first — in a libc370 checkout of `v2.0.0-dev`

- `doc/migration-2.0.md` — the maps and the **per-file procedure**. Follow it;
  it is the specification, this brief is not.
- `sdk/headermap.tsv`, `sdk/names.tsv`, `sdk/removed.tsv`, `sdk/splits/*.json`
- `CHANGELOG.md`, section `[Unreleased]`
- `<REPO>/CLAUDE.md` and the ecosystem `CLAUDE.md` one directory up.

## Do

1. Rewrite every libc370 include per the procedure. A split header is
   replaced by the headers that declare the names the file **uses**
   (`sdk/names.tsv`), not by all of them. The consumer's own headers win over
   a libc370 header of the same basename.
2. Names in `sdk/removed.tsv`: replace only where `now` names a replacement,
   and read its note. `internal` or `-` → do not improvise; list it.
3. crypto370: only if the code uses it — add `"mvslovers/crypto370" =
   ">=1.0.0"` to `[dependencies]` and include its header.
4. Point the build at 2.0 — the only two lines outside the sources:
   - `project.toml`: `[toolchain] libc370 = "2.0.0-dev"` (becomes the tag
     `v2.0.0-dev` for `release.yml`)
   - `.github/workflows/build.yml`: `libc370_ref: v2.0.0-dev` under `with:`
     of the mbt build job — without it the build CI floats on libc370 `main`,
     which is 1.x
5. Nothing else. No refactoring, no reformatting, no unrelated fixes.

## Build locally against 2.0 — never in the shared toolchain

cc370 finds its sysroot next to its binary (`<prefix>/bin/cc370` →
`<prefix>/cc370`; `mbt/mk/mbt.mk`, `sdk/mklibc.py`). The shared toolchain in
`~/.local` holds libc370 1.0.8 and stays that way: other projects (rexx370 and
every unported consumer) build against it. **Never run `mklibc.py install`
with `~/.local/bin` first on `PATH`.**

2.0 lives in a second toolchain, `~/.local/opt/cc370-libc2/`, built from
`v2.0.0-dev`. Build the consumer with

    PATH=$HOME/.local/opt/cc370-libc2/bin:$PATH make

and check that mbt picked it up: `make -p -n | grep '^SYSROOT'` must name
`~/.local/opt/cc370-libc2/cc370`. If the second toolchain is missing or its
libc370 is older than the tag, stop and say so — do not build it yourself
(how it is built is below the brief, for the maintainer).

## Prove it

- It builds with `-Wall -Werror` and no implicit declaration.
- **Assembler:** generate the `.s` of every TU twice, against 1.0.8 (the
  shared toolchain, the consumer unmigrated) and against 2.0 (the second
  toolchain, migrated), and compare. Identical, except TUs that use an
  interface change above — explain each of those in one line. Any other
  difference is a wrong include, not noise. (This is libc370's own
  `sdk/gate.py` method.)
- `make test-host`, if the project has host tests.

## Report

Files changed; include lines by kind (moved / split / removed); every
`removed.tsv` or `internal` case with `file:line` and a proposal; crypto370
yes or no; the `.s` comparison (N identical, M different, a reason for each);
anything that surprised you. Say what you did not run — a check you skipped
is not a check that passed.

## Rules

Commit and PR text in English and never mentioning AI. In zsh, write flags
inline — an unquoted `$VAR` is not split. Anything beyond rewriting includes:
stop and ask.

---

## Per-consumer notes

**mvslovers/ufsd** (first, decided 2026-10-01). Its public `include/libufs.h`
is shipped in ufsd's lib tarball and includes `"time64.h"` and `"clib64.h"`
(2.0: `ext/time64.h`, `ext/int64.h`). ftpd, httpd and mvsMF take ufsd with open
`>=` ranges and only `mbt.lock` holds them on today's ufsd. So **no ufsd
release from the ported tree** until those three are ported too — the port
lives on its branch (and may be merged on ufsd's own say-so), but `make
release` waits.

---

## For the maintainer: the second toolchain

Built 2026-10-01 from `v2.0.0-dev` at `c86e80a`. A copy of the shared
toolchain's four cc370 parts, 11 MB, with libc370 2.0 installed into the
copy's own sysroot:

| part | from | note |
|---|---|---|
| `bin/cc370` | `~/.local/bin/cc370` | a real copy — the driver finds everything relative to its own path |
| `bin/{ar,as,ld,cmplmd,dasm,file,idrdump,xmit}370` | — | relative links `../cc370/bin/<tool>`, as in `~/.local/bin` |
| `lib/cc370/`, `libexec/cc370/` | `~/.local/lib/cc370`, `~/.local/libexec/cc370` | `cc1`; the `as`/`ld`/`ar` links in it are relative |
| `cc370/` | `~/.local/cc370` | the sysroot; `mklibc.py install` replaces its headers, `libc.a` and macros |

Measured after the build: the copy's driver searches
`~/.local/opt/cc370-libc2/…/cc370/include` and runs its own `cc1`; mbt reads
`2.0.0-dev` from its `libc.a` and 1.0.8 from the shared one, which kept its 153
1.x headers; in a consumer, `SYSROOT` follows `PATH`.

**Refresh** it when `v2.0.0-dev` moves (the libc370 part) or cc370 is
reinstalled (the whole copy — the driver, `cc1` and the assemblers are copies
of the day they were taken):

    # libc370 only, from a checkout of the tag
    git worktree add --detach /tmp/libc2 v2.0.0-dev
    cd /tmp/libc2
    PATH=$HOME/.local/opt/cc370-libc2/bin:$PATH python3 sdk/mklibc.py build
    PATH=$HOME/.local/opt/cc370-libc2/bin:$PATH python3 sdk/mklibc.py install

    # after a cc370 reinstall: remove ~/.local/opt/cc370-libc2, copy the four
    # parts again as in the table, then the libc370 step above

`mklibc.py install` writes into the sysroot of the first `cc370` on `PATH`. Run
without the `PATH=` prefix, it overwrites the shared 1.0.8 toolchain — that is
the one mistake this arrangement exists to prevent.
