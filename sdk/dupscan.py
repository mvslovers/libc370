#!/usr/bin/env python3
"""Report external names that more than one archived libc370 module exports.

The archive namespace is flat and 8 characters wide, and ld370 resolves an
autocall against the first object that offers the name -- so two objects
exporting one name means the link order decides which is used, and nothing
pins it.

Issue #151 is why this exists. `@@ERRNO` was exported by `@@errno.s` as a
function prologue and by `@@get@er.s` as a `DC F'0'`, while `errno.h` makes
every `errno` in the ecosystem a *call* to that name -- so the resolution
decided whether `errno` reached the per-task accessor or branched onto four
zero bytes. The other three (`@@LKUNTF`, `@@LKUNTR`, `JESJOBFR`) were
byte-identical twins from a mistyped filename, harmless but equally unpinned.

Names are read from the generated .s / hand-written .asm, where they are
declared in plain text:

    ENTRY <NAME>                       explicit ENTRY directive
    <NAME>   PDPPRLG ...,ENTRY=YES     function prologue that entries itself
    <NAME>   CSECT                     named control section

Two kinds of module are skipped, because neither is ever an archive member and
a gate that cries wolf gets ignored:

  * anything exporting @@MAIN -- mklibc.py drops those objects from libc.a by
    design ("a stray main must not land in libc.a"), so the handful of test and
    demo programs carrying one never share the namespace;
  * the crt startfiles (STARTUPS below) -- they are copied out as crt0.o /
    crt1.o / crtm.o and are mutually exclusive by construction: exactly one is
    linked into any load module, so the @@CRT0 / CTHREAD / @@CTEXIT names they
    share cannot collide.

    python3 sdk/dupscan.py [path ...]      default: src asm

Run standalone it walks the tree; run from mklibc.py it is handed the exact
source list being archived, and its module count then equals the archive's
member count. The two can differ by any orphaned generated .s left on disk after
its .c was deleted -- such a file is not archived (the build globs .c) but is
still on disk until `make clean`. One exists today, `src/clib/@@cs.s`, residue
of #48/#54.

Counting members to check this: `ar t` lists the archive symbol index as a
member named `/`, so filter it out (`ar t libc.a | grep -vc '^/$'`) or the count
comes out one too high.

Exit 1 if any name is exported by more than one archived module.
"""
import os
import re
import sys

# mirrors mklibc.py's STARTUPS -- kept in step by hand, and the build gate
# passes its own list so a drift here cannot silently narrow the scan
STARTUPS = ("@@crt0", "@@crt1", "@@crtm")

ENTRY_DIR = re.compile(r"^\s+ENTRY\s+(\S+)", re.I)
PROLOGUE = re.compile(r"^(\S+)\s+PDPPRLG\b.*\bENTRY=YES\b", re.I)
CSECT = re.compile(r"^(\S+)\s+CSECT\b", re.I)


def externals(path):
    """every external name this module offers to the linker"""
    names = set()
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if line[:1] in ("*", "."):
                continue
            m = ENTRY_DIR.match(line) or PROLOGUE.match(line) or CSECT.match(line)
            if m:
                names.add(m.group(1).upper())
    return names


def scan(paths, startups=STARTUPS):
    """-> (duplicates {name: [module, ...]}, modules scanned, non-archived skipped)"""
    owners, files, skipped = {}, 0, 0
    stems = tuple(s.lower() for s in startups)
    for path in paths:
        stem = os.path.basename(path).rsplit(".", 1)[0].lower()
        if stem in stems:
            skipped += 1                  # startfile, not archived -- see docstring
            continue
        names = externals(path)
        if "@@MAIN" in names:
            skipped += 1                  # never archived -- see the docstring
            continue
        files += 1
        for name in names:
            owners.setdefault(name, []).append(path)
    dups = {n: sorted(p) for n, p in owners.items() if len(p) > 1}
    return dups, files, skipped


def report(dups, files, skipped, tag="dupscan"):
    """print the result; -> 0 clean, 1 duplicates found"""
    print("[%s] %d archived modules, %d non-archived skipped" %
          (tag, files, skipped))
    if not dups:
        return 0
    for name in sorted(dups):
        print("[%s] DUPLICATE EXTERNAL %-8s exported by %d modules:" %
              (tag, name, len(dups[name])))
        for p in dups[name]:
            print("          %s" % p)
    print("[%s] %d duplicate external name(s) -- ld370's autocall order would "
          "decide which object is linked (see #151)" % (tag, len(dups)))
    return 1


def collect(roots):
    out = []
    for root in roots:
        for dirpath, _, filenames in os.walk(root):
            if os.sep + "wip" in dirpath:
                continue
            for fn in sorted(filenames):
                if fn.endswith((".s", ".asm")):
                    out.append(os.path.join(dirpath, fn))
    return sorted(out)


if __name__ == "__main__":
    roots = sys.argv[1:] or ["src", "asm"]
    paths = collect(roots)
    if not paths:
        print("[dupscan] no .s/.asm under %s -- build first" % " ".join(roots))
        sys.exit(2)
    sys.exit(report(*scan(paths)))
