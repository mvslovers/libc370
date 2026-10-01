#!/usr/bin/env python3
"""movehdr -- carry out the 1:1 header moves of 2.0 phase 1 (#256).

Takes the rows of sdk/headermap.tsv whose target starts with PREFIX, and for
each header still at its 1.x name:

  1. git mv include/<today> include/<target>
  2. rewrites every #include of it in the tracked sources -- src/, include/,
     test/ -- keeping the delimiter ("" or <>) and the file's bytes otherwise,
     so CRLF files stay CRLF and the diff is the include line and nothing else

and then fails if any tracked file still includes an old name.  Comments and
documentation that mention a header by name are left alone; say so in the PR
if one of them matters.  Merges (several rows, one target) and splits are not
this tool's job: it refuses a target that more than one row maps to.

Usage:  python3 sdk/movehdr.py ibm/jes2/        # move everything under it
        python3 sdk/movehdr.py mvs/wto.h         # or exactly one target
"""
import os, re, sys, subprocess, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "sdk"))
import headermap

SCOPE = ("src/", "include/", "test/")


def git(*args):
    r = subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout


def include_re(name):
    return re.compile(rb'(#[ \t]*include[ \t]*)([<"])' + re.escape(name.encode()) + rb'([>"])')


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    prefix = sys.argv[1]
    rows = headermap.load()
    per_target = collections.Counter(r["target"] for r in rows)
    moves = [r for r in rows
             if r["target"] not in headermap.KEYWORDS
             and r["target"] != r["today"]
             and (r["target"] == prefix or (prefix.endswith("/") and r["target"].startswith(prefix)))]
    if not moves:
        sys.exit(f"[movehdr] no row targets {prefix}")
    shared = sorted({r["target"] for r in moves if per_target[r["target"]] > 1})
    if shared and not prefix.endswith("/"):
        sys.exit(f"[movehdr] {prefix} is a merge target, not a 1:1 move")
    if shared:
        # a directory holds 1:1 moves and merges side by side (mvs/): move
        # the former, leave the merges to be done by hand
        print(f"[movehdr] skipping merge targets: {', '.join(shared)}")
        moves = [r for r in moves if r["target"] not in shared]

    inc = os.path.join(ROOT, "include")
    todo = [r for r in moves if os.path.exists(os.path.join(inc, r["today"]))]
    for r in todo:
        dst = os.path.join(inc, r["target"])
        if os.path.exists(dst):
            sys.exit(f"[movehdr] {r['target']} exists already")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        git("mv", f"include/{r['today']}", f"include/{r['target']}")
        print(f"[movehdr] include/{r['today']} -> include/{r['target']}")

    files = [f for f in git("ls-files").splitlines() if f.startswith(SCOPE)]
    pats = [(include_re(r["today"]), r["target"].encode()) for r in moves]
    total = 0
    for f in files:
        p = os.path.join(ROOT, f)
        if not os.path.isfile(p):
            continue
        data = open(p, "rb").read()
        if b"\0" in data:
            continue                        # binary
        new, n = data, 0
        for rx, tgt in pats:
            new, k = rx.subn(lambda m: m.group(1) + m.group(2) + tgt + m.group(3), new)
            n += k
        if n:
            open(p, "wb").write(new)
            total += n
            print(f"[movehdr] {n:3} include(s) rewritten in {f}")

    # nothing may still include an old name -- read back from the files, not
    # from the count above
    left = []
    for f in files:
        p = os.path.join(ROOT, f)
        if not os.path.isfile(p):
            continue
        data = open(p, "rb").read()
        for rx, _ in pats:
            if rx.search(data):
                left.append(f)
    print(f"[movehdr] {len(todo)} header(s) moved, {total} include line(s) rewritten")
    if left:
        print(f"[movehdr] still including an old name: {', '.join(sorted(set(left)))}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
