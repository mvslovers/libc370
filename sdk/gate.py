#!/usr/bin/env python3
"""gate -- prove a change leaves the generated code alone (#256).

Phase 1 of 2.0 moves, renames, merges and splits headers.  None of that may
change a single instruction, and none of it may change what libc.a exports.
This builds the base and the working tree and compares three things:

  1. every generated .s, byte for byte  (src/clib/@@ver.s, the build stamp,
     is expected to differ and is skipped)
  2. the archive: its members and its symbol table (ar370 t)
  3. the compiler warnings, by message; a warning inside a header that moved
     counts as the same warning
  4. implicit declarations, from a second pass with -Wimplicit-function-
     declaration: a split that loses a prototype can leave the assembler as
     it was (an int function called through an invented int declaration),
     so the first three would not see it

A change that is meant to alter code -- #39 step 2, #68 step 1 -- lists what
it alters in sdk/gate-allow.txt, one entry per line with the reason:

    asm     src/clib/@@b64dec.s   memset() now declared: inline MVCL
    symbol  B64ENC                renamed in crypto370
    member  sha256i.o             moved to crypto370

An entry that matches nothing that differs is an error too, so the file cannot
outlive the change it was written for: the next PR finds its entries stale and
empties it.

Usage:  python3 sdk/gate.py [BASE]      BASE defaults to origin/2.0
Exit:   0 identical (or every difference allowed), 1 not, 2 could not build
"""
import os, re, sys, shutil, subprocess, tempfile, concurrent.futures as cf

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALLOW = os.path.join(ROOT, "sdk", "gate-allow.txt")
STAMP = "src/clib/@@ver.s"


def run(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def build(tree):
    """mklibc.py build in tree -> (stdout, ok)."""
    r = run([sys.executable, os.path.join(tree, "sdk", "mklibc.py"), "build"], cwd=tree)
    return r.stdout + r.stderr, r.returncode == 0


def asm_files(tree):
    out = {}
    src = os.path.join(tree, "src")
    for d, _, files in os.walk(src):
        if os.path.relpath(d, src).split(os.sep)[0] == "wip":
            continue                        # never built
        for f in files:
            # only what mklibc.py generates: a .s beside its .c.  A .s whose
            # .c is gone is a leftover the build never touches (@@cs.s
            # outlived @@cs.c by months) and says nothing about this change.
            if f.endswith(".s") and os.path.exists(os.path.join(d, f[:-2] + ".c")):
                p = os.path.join(d, f)
                out[os.path.relpath(p, tree)] = open(p, "rb").read()
    return out


def archive(tree):
    """(members, symbols) of tree's libc.a, from ar370 t."""
    r = run(["ar370", "t", os.path.join(tree, "build", "sdk", "libc.a")])
    members, symbols = set(), set()
    for line in r.stdout.splitlines():
        if line.rstrip().endswith("bytes"):
            members.add(line.split()[0].rstrip("/"))
        elif line.startswith("  ") and line.strip():
            symbols.add(line.strip())
    return members, symbols


def warnings(log):
    """compiler warnings as a multiset of 'file-basename: message'.

    The path and line number are dropped: a header that moves, or gains a
    line, keeps its warnings.  What counts is that no new message appears."""
    out = []
    for line in log.splitlines():
        m = re.match(r"\s*(\S+?):\d+(?::\d+)?: warning: (.*)", line)
        if m:
            out.append(f"{os.path.basename(m.group(1))}: {m.group(2)}")
    return sorted(out)


def implicit(tree):
    """'file: implicit declaration of function X' for every library TU."""
    tus = []
    src = os.path.join(tree, "src")
    for d, _, files in os.walk(src):
        if os.path.relpath(d, src).split(os.sep)[0] == "wip":
            continue
        tus += [os.path.relpath(os.path.join(d, f), tree) for f in files if f.endswith(".c")]
    incs = ["-I", "include", "-I", "src/thdmgr", "-I", "src/time64"]
    if os.path.isdir(os.path.join(tree, "src", "internal")):
        incs += ["-I", "src/internal"]

    def one(tu):
        r = run(["cc370", "-O1", "-fsyntax-only", "-Wimplicit-function-declaration"]
                + incs + [tu], cwd=tree)
        return [f"{tu}: {m.group(1)}" for m in
                re.finditer(r"warning: (implicit declaration of function \S+)", r.stderr)]
    out = []
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        for found in ex.map(one, sorted(tus)):
            out += found
    return sorted(out)


def load_allow():
    allow = {"asm": {}, "symbol": {}, "member": {}}
    if not os.path.exists(ALLOW):
        return allow
    for n, line in enumerate(open(ALLOW, encoding="utf-8"), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 2)
        if len(parts) < 3 or parts[0] not in allow:
            sys.exit(f"{ALLOW}:{n}: want '<asm|symbol|member> <name> <reason>'")
        allow[parts[0]][parts[1]] = parts[2]
    return allow


def main():
    base = sys.argv[1] if len(sys.argv) > 1 else "origin/2.0"
    rev = run(["git", "-C", ROOT, "rev-parse", "--short", base]).stdout.strip()
    if not rev:
        print(f"[gate] no such base: {base}")
        return 2
    allow = load_allow()
    tmp = tempfile.mkdtemp(prefix="libc370-gate-")
    wt = os.path.join(tmp, "base")
    try:
        r = run(["git", "-C", ROOT, "worktree", "add", "--detach", wt, rev])
        if r.returncode:
            print("[gate] worktree:", r.stderr.strip())
            return 2
        print(f"[gate] base {base} ({rev}) vs working tree")
        blog, bok = build(wt)
        hlog, hok = build(ROOT)
        if not (bok and hok):
            print("[gate] build failed:", "base" if not bok else "working tree")
            print((blog if not bok else hlog)[-2000:])
            return 2

        found = {"asm": set(), "symbol": set(), "member": set()}
        problems = 0

        # 1. assembler
        ba, ha = asm_files(wt), asm_files(ROOT)
        same = 0
        for p in sorted(set(ba) | set(ha)):
            if p == STAMP:
                continue
            if p not in ba or p not in ha or ba[p] != ha[p]:
                found["asm"].add(p)
            else:
                same += 1
        for p in sorted(found["asm"]):
            how = "only in base" if p not in ha else "only in tree" if p not in ba else "differs"
            ok = p in allow["asm"]
            print(f"[gate] asm     {p}: {how}" + (f"  (allowed: {allow['asm'][p]})" if ok else ""))
            problems += not ok
        print(f"[gate] asm     {same} identical, {len(found['asm'])} not "
              f"({len(ba)} in base, {len(ha)} in tree)")

        # 2. archive
        bm, bs = archive(wt)
        hm, hs = archive(ROOT)
        for kind, b, h in (("member", bm, hm), ("symbol", bs, hs)):
            for x in sorted(b ^ h):
                found[kind].add(x)
                ok = x in allow[kind]
                print(f"[gate] {kind:7} {x}: {'dropped' if x in b else 'added'}"
                      + (f"  (allowed: {allow[kind][x]})" if ok else ""))
                problems += not ok
            print(f"[gate] {kind:7} {len(h)} in tree, {len(b ^ h)} changed")

        # 3. warnings
        bw, hw = warnings(blog), warnings(hlog)
        extra = list(hw)
        for w in bw:
            if w in extra:
                extra.remove(w)
        for w in extra:
            print(f"[gate] warning new: {w}")
        problems += len(extra)
        print(f"[gate] warning {len(bw)} in base, {len(hw)} in tree, {len(extra)} new")

        # 4. implicit declarations
        bi, hi = implicit(wt), implicit(ROOT)
        extra = list(hi)
        for w in bi:
            if w in extra:
                extra.remove(w)
        for w in extra:
            print(f"[gate] implicit new: {w}")
        problems += len(extra)
        print(f"[gate] implicit {len(bi)} in base, {len(hi)} in tree, {len(extra)} new")

        # an allowance that matched nothing has outlived its change
        for kind in allow:
            for x in sorted(set(allow[kind]) - found[kind]):
                print(f"[gate] stale allowance: {kind} {x} is not different")
                problems += 1

        print(f"[gate] {'PASS' if not problems else f'FAIL, {problems} problem(s)'}")
        return 1 if problems else 0
    finally:
        run(["git", "-C", ROOT, "worktree", "remove", "--force", wt])
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
