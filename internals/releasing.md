# Releasing libc370, and its rules with cc370

libc370 is the target library of [cc370](https://github.com/mvslovers/cc370).
The two are released separately, but each release states which releases of the
other it works with. This page is the libc370 side of those rules, for the
people who install the pair and for anyone (human or agent) cutting a libc370
release. cc370 keeps the same page from its side, and the first three
sections below are word for word the same in both; the decisions behind them
are in [cc370#523](https://github.com/mvslovers/cc370/issues/523).

**The libc370 side is written in exactly one place: `sdk/cc370.json`**
(`"min"`, `"below"`). `sdk/package.py requires {deb,rpm,range,number,notes}`
derives every other statement from it, and `sdk/headermap.py check` (CI)
fails when `include/sys/_cc370.h` disagrees or a header does not include it.

## Who owns what

| cc370 | libc370 |
|---|---|
| `libcc370rt.a` — every helper the compiler emits (`@@MULDI3` … `@@FFSSI2`, the `-ftrapv` helpers) | headers, `libc.a` (with the startup `@@CRT0` since 2.3.0), `crtm.o` |
| the prologue macros `PDPTOP`, `PDPPRLG`, `PDPEPIL` (`<sysroot>/macros`) | every other macro: `maclib/` and the vendored `sysmac/` |
| `__CC370__` = MAJOR·10000 + MINOR·100 + PATCH (a `-dev` suffix is dropped) | the check of it in every header (`<sys/_cc370.h>`) |
| the link line: `-lcc370rt -lc -lcc370rt`, `--entry @@CRT0`, no startfile (since 1.4.0); for `main` the compiler emits `EXTRN @@CRT0` | the startup `@@CRT0`, a member of `libc.a` since 2.3.0 (libc370#159); libc370 2.4.0 installs no startfile copies |

## Who needs which version (2026-10-05)

| | needs | enforced by |
|---|---|---|
| libc370 2.4.x | cc370 >= 1.4.0, < 2 | `#error` in every header (`__CC370__ < 10400`), `.deb` Depends / `.rpm` Requires, `metadata.json` — all derived from libc370's `sdk/cc370.json` |
| libc370 2.1.x - 2.3.x | cc370 >= 1.1.0, < 2 | `#error` in every header (`__CC370__ < 10100`), `.deb` Depends / `.rpm` Requires, `metadata.json` — all derived from libc370's `sdk/cc370.json` |
| cc370 1.4.x | libc370 >= 2.3.0 | the packages: `LIBC_MIN` 2.3.0 -- the link no longer names `crt0.o`, and an older `libc.a` has no `@@CRT0` member (libc370#159) |
| cc370 1.1.x - 1.3.x | libc370 >= 2.1.0 | the packages only: `Depends: libc370-dev (>= 2.1.0)`, `Breaks`/`Replaces: libc370-dev (<< 2.1.0)` (RPM: `Requires`/`Conflicts` on `libc370-devel`), because libc370 2.0 still shipped the three macro files |
| cc370 1.0.0 | libc370 <= 2.0.x | the old arrangement: helpers and macros still in libc370 |
| projects built with mbt | both, pinned | `[toolchain]` in `project.toml` |

## The rule

A release waits for the other project only when it needs a *higher* minimum
of it — and then the one it needs is released first.

## maclib370

Not part of the sections above, which cc370 keeps word for word in its own
copy. [maclib370](https://github.com/mvslovers/maclib370) is SYS1.MACLIB and
SYS1.AMODGEN of MVS 3.8j as a package of its own (mvslovers/mbt#234),
installed into `<sysroot>/maclib370/macros`; `as370` searches that
directory after `<sysroot>/macros` and `<sysroot>/libc370/macros` from the
first cc370 release after 1.5.0 (cc370 4d1fca9).

- libc370's packages and Homebrew formula **depend** on it, with the range
  written once, in **`sdk/maclib370.json`** (`"min"`, `"below"`).
  `sdk/package.py requires {deb,rpm,maclib370-range,notes}` derives the
  `.deb` Depends, the `.rpm` Requires, `metadata.json`
  (`requires.maclib370`) and the notes line from it; `sdk/checkpkg.sh`
  checks them, and `sdk/pairtest.sh` installs maclib370's package beside the
  pair and checks that libc370 without it is refused (#490).
- The build does not use it yet. `sysmac/` keeps its IBM copies, and
  libc370's `maclib/` and `sysmac/` still win over maclib370 on a name.
  Dropping the copies is a release of its own, checked by comparing the
  objects of `libc.a` assembled with and without `sysmac/` (`FREEPOOL` is
  known to differ).
- maclib370 is released first when libc370 needs a higher minimum of it,
  as with cc370 (*The rule*).

## Releasing libc370

1. **The cc370 named as the minimum is released.** `release.yml` builds with
   cc370 `v<min>` from `sdk/cc370.json`, never `main`; without that tag the
   release cannot build.
2. **A normal release** (new functions, fixes): nothing to do in cc370.
3. **A release that needs a newer cc370 feature:** release that cc370 first,
   then raise `"min"` in `sdk/cc370.json` in the change that starts needing
   it. The package dependencies, the metadata and the notes follow from it
   by themselves; the number and the message in `include/sys/_cc370.h` are
   edited in the same change (`sdk/package.py requires number` gives the
   number), and CI fails until the two agree. Raising the minimum is a
   **minor** release (D6 in `internals/design-2.0.md`; argued in #313).
4. **Never** add a compiler helper or a `pdp*` prologue macro to libc370.
   Both are cc370's (see *Who owns what*). `sdk/package.py` refuses a package
   that contains one of the three macro files; the helpers have no such guard
   -- their sources are simply gone from `src/s370/`.
5. **An incompatible API change** is libc370 3.0.0; consumers move their
   `[toolchain]` pin.
6. **Release commit** on a `release/X.Y.Z` branch: `VERSION` and the dated
   `CHANGELOG.md` section, through a PR. Before it, the manuals (ML01-0003,
   ML01-0004, `docs/books/`): the edition notice of both (`ml01-000N.typ`,
   "This edition applies to ...") names the release whose behaviour the text
   describes, and `guide/pg-migrate.typ` has the release's row when it changes
   what a caller notices. A PR that changed documented behaviour has already
   updated the books or named the stale section in its body; a release that
   changes no documented behaviour needs no book change. After the merge, an
   annotated tag `vX.Y.Z` ("libc370 X.Y.Z") on the merge commit -- only after
   checking with the coordinating mbt session.
7. **Check what `release.yml` attached** before announcing:
   `libc370-<v>-sysroot.tar.gz`, `libc370-dev_<v>_all.deb`,
   `libc370-devel-<v>.noarch.rpm`, `libc370-<v>-metadata.json` (its
   `"requires"` ranges, cc370 and maclib370), the two manuals `ml01-0003-E.pdf` and
   `ml01-0004-E.pdf` (built from the tag by build.yml's `books` job) and
   `SHA256SUMS` (`sha256sum -c`, the PDFs included); the build log names
   the cc370 tag it built with. The release's `pair` job must be green: it
   installs the new packages beside cc370's and maclib370's under apt and
   dnf, and links a program with the pair (`.github/workflows/pair.yml`, `sdk/pairtest.sh`).
   The `homebrew` job renders `sdk/homebrew/libc370.rb.in` with the new
   version and the tarball's sha256 and pushes it to `mvslovers/homebrew-tap`
   (commit `libc370 <version>`); edit the template there, never the tap's copy.
8. **Rework the release page** (`gh release edit vX.Y.Z --notes-file …`).
   `release.yml` pastes the CHANGELOG section; a reader of the page has
   never heard of the ecosystem. Give it a lead paragraph (what the release
   is, whether it is a drop-in, the cc370 range), "Read this first" for the
   changes a caller notices, a before/now table of the fixes, how to
   install, then the full changelog. **No consumer project names and no job
   numbers or system names from private systems** — on the page or in
   `CHANGELOG.md`; describe the effect in libc370's own terms. v2.2.0 is
   the worked example.

## Releasing cc370

cc370's own checklist, the other half of these rules:
[cc370 `internals/releasing.md`](https://github.com/mvslovers/cc370/blob/main/internals/releasing.md#releasing-cc370).
