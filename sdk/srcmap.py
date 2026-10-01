#!/usr/bin/env python3
"""srcmap -- carry out and check the source moves of 2.0 phase 3 (#278, D9).

sdk/srcmap.tsv holds one row per library source (.c, .asm) and per private
header under src/: where it is today, where it goes, and why.  The target
follows src/ mirrors include/: a source sits with the public header that
declares what it defines (src/stdio/, src/mvs/dynalloc/, src/ext/int64/), the
POSIX network family and its DYN75 provider in src/net/, compiler support in
src/s370/, programs with a main() in attic/.

move PREFIX   git mv every row whose target starts with PREFIX and that is
              still at its old place, then rewrite every mention of the old
              path in the tracked files that build or test the library --
              rooted includes ("src/jes/jesprb.h"), test/host/run.sh, a test
              that #includes a source, the build comments in test/ and jcl/ --
              byte for byte, so CRLF files stay CRLF.  History (CHANGELOG.md,
              TODO.md, doc/) and attic/ are left as written.
check         every row is at exactly one of its two places (a row whose
              target is its own path lists a file that stays, so that the
              map is complete); every source and
              private header in the tree has a row; no file in that scope
              still names the old path of a row that has moved.

A source keeps its basename: the archive member is <stem>.o and ld370
resolves autocalls by member name, so a renamed file changes the library
(sdk/gate.py reports it as a member difference).

Usage:  python3 sdk/srcmap.py move src/stdio/
        python3 sdk/srcmap.py check
"""
import os, re, sys, glob, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(ROOT, "sdk", "srcmap.tsv")
# the files whose path mentions must follow a move
SCOPE = ("src/", "include/", "test/", "jcl/", "sdk/", "Makefile", ".github/")
SKIP = ("sdk/srcmap.tsv",)


def git(*args):
    r = subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout


def load():
    rows = []
    for i, line in enumerate(open(MAP, encoding="utf-8"), 1):
        if line.startswith("#") or not line.strip():
            continue
        cells = line.rstrip("\n").split("\t")
        if len(cells) != 3:
            sys.exit(f"{MAP}:{i}: want 3 tab-separated cells")
        rows.append({"today": cells[0], "target": cells[1], "why": cells[2], "line": i})
    return rows


def mention(path):
    # the path as a whole word: not followed by more of a file name
    return re.compile(re.escape(path.encode()) + rb"(?![\w@$#.])")


def scoped_files():
    return [f for f in git("ls-files").split()
            if f.startswith(SCOPE) and f not in SKIP and os.path.isfile(os.path.join(ROOT, f))]


def exists(p):
    return os.path.exists(os.path.join(ROOT, p))


def cmd_move():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    prefix = sys.argv[2]
    todo = [r for r in load() if r["target"].startswith(prefix) and exists(r["today"])]
    if not todo:
        sys.exit(f"[srcmap] nothing left to move under {prefix}")
    for r in todo:
        if exists(r["target"]):
            sys.exit(f"[srcmap] {r['target']} exists already")
        os.makedirs(os.path.dirname(os.path.join(ROOT, r["target"])), exist_ok=True)
        git("mv", r["today"], r["target"])
    n = 0
    for f in scoped_files():
        p = os.path.join(ROOT, f)
        raw = open(p, "rb").read()
        t = raw
        for r in todo:
            t, k = mention(r["today"]).subn(r["target"].encode(), t)
            n += k
        if t != raw:
            open(p, "wb").write(t)
    print(f"[srcmap] {len(todo)} moved under {prefix}, {n} mention(s) rewritten")
    return cmd_check()


def cmd_check():
    rows = load()
    bad = 0
    state = {"moved": 0, "pending": 0, "stays": 0}
    for r in rows:
        a, b = exists(r["today"]), exists(r["target"])
        if r["today"] == r["target"]:           # listed so that the map is complete
            if not a:
                print(f"[srcmap] {r['today']} is gone"); bad += 1
            state["stays"] += 1
            continue
        if a and b:
            print(f"[srcmap] {r['today']} and {r['target']} both exist"); bad += 1
        elif not a and not b:
            print(f"[srcmap] {r['today']} is gone and {r['target']} does not exist"); bad += 1
        else:
            state["moved" if b else "pending"] += 1
    # every source and private header in the tree has a row
    known = {r["today"] for r in rows} | {r["target"] for r in rows}
    tree = [os.path.relpath(p, ROOT) for ext in ("c", "asm", "h")
            for p in glob.glob(os.path.join(ROOT, "src", "**", f"*.{ext}"), recursive=True)]
    tree += [os.path.relpath(p, ROOT) for p in glob.glob(os.path.join(ROOT, "asm", "*.asm"))]
    stray = sorted(set(tree) - known)
    if stray:
        print(f"[srcmap] in the tree but in no row: {', '.join(stray)}"); bad += 1
    # nothing that builds or tests still names a moved file by its old path
    moved = [r["today"] for r in rows if r["today"] != r["target"] and not exists(r["today"])]
    if moved:
        pats = [(t, mention(t)) for t in moved]
        for f in scoped_files():
            raw = open(os.path.join(ROOT, f), "rb").read()
            for t, rx in pats:
                if t.encode() in raw and rx.search(raw):
                    print(f"[srcmap] {f} still names {t}"); bad += 1
    print(f"[srcmap] {len(rows)} rows: {state['moved']} moved, {state['pending']} pending, "
          f"{state['stays']} staying"
          + (f"; {bad} problem(s)" if bad else "; consistent"))
    return 1 if bad else 0


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    fn = {"move": cmd_move, "check": cmd_check}.get(cmd)
    if not fn:
        sys.exit(__doc__)
    sys.exit(fn())
