#!/usr/bin/env python3
"""sizemap.py - what holds a load module's library members in place.

Reads a map written by `ld370 --map FILE --xref` (cc370 ... -Wl,--map,FILE,--xref)
and builds the reference graph between the members (and `main`).  It answers
the questions of #564's size work without guessing:

  sizemap.py MAP                  size, member count, and the single
                                  references whose removal saves the most
  sizemap.py MAP --path MEMBER    one chain from the entry to MEMBER
  sizemap.py MAP --cut A:B ...    the size with those references removed
                                  (A references B; members by object name,
                                  e.g. fopen:sprintf)

The graph follows the V-type references the map lists per section.  A weak
reference that some other member resolved is listed as V too and counts as an
edge; one left unresolved (WX in the map's symbol list) pulls nothing and is
not one.  Sizes are section lengths.  A cut models a change that removes that reference, not the code
that would replace it.

MAP is read only below the current directory, or below SIZEMAP_ROOT when
that is set.
"""
import os
import re
import sys
from collections import deque

ROOTS = ("@@crt0", "main")       # the entry point and the program itself


def load(path):
    """Members and references of the map at path (a file the caller names)."""
    base = os.path.realpath(os.environ.get("SIZEMAP_ROOT", os.getcwd()))
    path = os.path.realpath(os.path.join(base, path))
    if os.path.commonpath([base, path]) != base:
        raise SystemExit(f"{path}: outside {base} (set SIZEMAP_ROOT)")
    if not os.path.isfile(path):
        raise SystemExit(f"{path}: not a file")
    secs, cur = [], None
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = fh.readlines()
    for line in lines:
        m = re.match(r"^\S*\s+(PC|SD)\s+[0-9A-F]{6}\s+([0-9A-F]{6})\s+(\S+)", line)
        if m:
            k = re.search(r"\(([^)]+)\.o\)", m.group(3))
            cur = {"name": k.group(1) if k else "main", "len": int(m.group(2), 16),
                   "defs": set(), "refs": set()}
            secs.append(cur)
            continue
        m = re.match(r"^\s+\+[0-9A-F]+\s+V\s+(\S+)", line)
        if m and cur:
            cur["refs"].add(m.group(1))
            continue
        m = re.match(r"^ {2}(\S+)\s+[0-9A-F]{6}\s*$", line)
        if m and cur:
            cur["defs"].add(m.group(1))
    mem = {}
    for s in secs:
        d = mem.setdefault(s["name"], {"len": 0, "defs": set(), "refs": set()})
        d["len"] += s["len"]
        d["defs"] |= s["defs"]
        d["refs"] |= s["refs"]
    owner = {x: n for n, d in mem.items() for x in d["defs"]}
    graph = {n: {owner[r] for r in d["refs"] if r in owner and owner[r] != n}
             for n, d in mem.items()}
    return mem, graph


def reach(graph, cuts=frozenset()):
    seen, stack = set(), list(ROOTS)
    while stack:
        n = stack.pop()
        if n in seen or n not in graph:
            continue
        seen.add(n)
        stack.extend(m for m in graph[n] if (n, m) not in cuts)
    return seen


def size(mem, members):
    return sum(mem[x]["len"] for x in members)


def path_to(graph, target):
    """One chain from the entry to target, or None."""
    par, q, seen = {}, deque(ROOTS), set(ROOTS)
    while q:
        n = q.popleft()
        for m in graph.get(n, ()):
            if m not in seen:
                seen.add(m)
                par[m] = n
                q.append(m)
    if target not in seen:
        return None
    chain = [target]
    while chain[-1] in par:
        chain.append(par[chain[-1]])
    return chain


def gains(mem, graph, base, total, floor=1000):
    """(bytes saved, from, to) for every single reference worth floor."""
    found = []
    for n in base:
        for m in graph[n]:
            saved = total - size(mem, reach(graph, frozenset({(n, m)})))
            if saved >= floor:
                found.append((saved, n, m))
    return sorted(found, reverse=True)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    mem, graph = load(argv[1])
    base = reach(graph)
    total = size(mem, base)
    if "--path" in argv:
        chain = path_to(graph, argv[argv.index("--path") + 1])
        print(" <- ".join(chain) if chain else "not linked")
        return 0 if chain else 1
    if "--cut" in argv:
        cuts = frozenset(tuple(c.split(":", 1)) for c in argv[argv.index("--cut") + 1:])
        left = reach(graph, cuts)
        print(f"{len(base)} members {total} B -> {len(left)} members {size(mem, left)} B")
        return 0
    print(f"{len(base)} members, {total} bytes")
    for saved, n, m in gains(mem, graph, base, total)[:15]:
        print(f"  {saved:7d}  {n} -> {m}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
