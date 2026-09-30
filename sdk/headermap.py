#!/usr/bin/env python3
"""headermap -- the 1.x -> 2.0 header mapping (#245, #256), in one place.

sdk/headermap.tsv holds one row per header libc370 1.x shipped: its 1.x name,
its 2.0 target, how many consumers include it, and a note.  The target is a
path under include/, or one of

    internal   leaves the sysroot in phase 2 (src/internal/)
    split      divided across several headers; the note says how
    removed    gone in 2.0 (dead, or moved to another project)

The table in doc/design-2.0.md is rendered from this file, and the consumer
migration script will read it, so a mapping is written down exactly once.

Usage:  python3 sdk/headermap.py render   # rewrite the tables in the design doc
        python3 sdk/headermap.py check    # doc in sync, every include/*.h mapped,
                                          # and where each moved header stands
"""
import os, sys, json, glob, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TSV = os.path.join(ROOT, "sdk", "headermap.tsv")
DOC = os.path.join(ROOT, "doc", "design-2.0.md")
INC = os.path.join(ROOT, "include")

KEYWORDS = ("internal", "split", "removed")
BEGIN, END = "<!-- headermap:begin -->", "<!-- headermap:end -->"


def load():
    rows = []
    with open(TSV, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            cells = line.split("\t")
            if len(cells) != 4:
                sys.exit(f"{TSV}:{n}: {len(cells)} fields, want 4")
            today, target, users, note = cells
            if "/" in today or not today.endswith(".h"):
                sys.exit(f"{TSV}:{n}: '{today}' is not a 1.x header name")
            if target not in KEYWORDS and not target.endswith(".h"):
                sys.exit(f"{TSV}:{n}: target '{target}' is neither a header nor one of {KEYWORDS}")
            rows.append(dict(today=today, target=target, users=users, note=note))
    seen = collections.Counter(r["today"] for r in rows)
    dup = [h for h, c in seen.items() if c > 1]
    if dup:
        sys.exit(f"{TSV}: listed twice: {', '.join(sorted(dup))}")
    return rows


def category(target):
    if target == "removed":
        return "removed or moved out"
    if target in ("internal", "split"):
        return {"internal": "internal", "split": "split across several"}[target]
    if "/" not in target:
        return "ISO C (name unchanged)"
    parts = target.split("/")
    top = parts[:2] if parts[0] == "ibm" else parts[:1]     # ibm/<component>/
    return "`" + "/".join(top) + "/`"


def render(rows):
    order = ["ISO C (name unchanged)", "`mvs/`", "`ibm/mvs/`", "`ibm/jes2/`",
             "`libc370/`", "`s370/`", "internal", "split across several",
             "removed or moved out"]
    count = collections.Counter(category(r["target"]) for r in rows)
    unknown = set(count) - set(order)
    if unknown:
        sys.exit(f"no summary line for: {', '.join(sorted(unknown))}")
    out = ["Summary:", "", "| target | headers |", "|---|---|"]
    out += [f"| {c} | {count[c]} |" for c in order if count[c]]
    out += [f"| **total** | **{len(rows)}** |", "",
            "| today | 2.0 | users | note |", "|---|---|---|---|"]
    for r in rows:
        t = r["target"]
        shown = "—" if t == "removed" else (t if t in KEYWORDS else f"`{t}`")
        out.append(f"| `{r['today']}` | {shown} | {r['users']} | {r['note']} |")
    return "\n".join(out)


def doc_parts():
    s = open(DOC, encoding="utf-8").read()
    if s.count(BEGIN) != 1 or s.count(END) != 1:
        sys.exit(f"{DOC}: want exactly one {BEGIN} ... {END}")
    a = s.index(BEGIN) + len(BEGIN)
    b = s.index(END)
    return s[:a], s[a:b], s[b:]


def cmd_render():
    head, _, tail = doc_parts()
    table = render(load())          # before the doc is opened for writing
    with open(DOC, "w", encoding="utf-8") as f:
        f.write(head + "\n" + table + "\n" + tail)
    print(f"[headermap] {DOC} rendered")
    return 0


def cmd_check():
    rows = load()
    bad = 0
    _, body, _ = doc_parts()
    if body.strip() != render(rows).strip():
        print(f"[headermap] {os.path.relpath(DOC, ROOT)} is out of date: "
              f"run 'python3 sdk/headermap.py render'")
        bad += 1

    # every header in include/, at any depth, is either a 1.x name still in
    # place or the 2.0 target of exactly one row
    present = set()
    for d, _, files in os.walk(INC):
        for f in files:
            if f.endswith(".h"):
                present.add(os.path.relpath(os.path.join(d, f), INC))
    today = {r["today"] for r in rows}
    targets = collections.defaultdict(list)
    for r in rows:
        if r["target"] not in KEYWORDS:
            targets[r["target"]].append(r["today"])
    # a split row's destinations are in its spec, sdk/splits/<header>.json
    for spec in glob.glob(os.path.join(ROOT, "sdk", "splits", "*.json")):
        sp = json.load(open(spec))
        for t in sp["targets"]:
            if not t.startswith("src/"):
                targets[t].append(sp["source"])
    stray = sorted(p for p in present if p not in today and p not in targets)
    if stray:
        print(f"[headermap] in include/, but in no row: {', '.join(stray)}")
        bad += 1

    # where each row stands.  A moved header must not exist under both names;
    # a merge target is done once all of its sources are gone.
    state = collections.Counter()
    for r in rows:
        old = r["today"] in present
        t = r["target"]
        if t in ("internal", "split"):
            state[f"{t}, pending" if old else f"{t}, done"] += 1
        elif t == "removed":
            state["removed, pending" if old else "removed, done"] += 1
        elif t == r["today"]:
            state["ISO C, unchanged"] += 1
        else:
            new = t in present
            if old and new and len(targets[t]) > 1:
                state["merge, pending"] += 1    # target made from another row
                continue
            if old and new:
                print(f"[headermap] {r['today']} and {t} both exist")
                bad += 1
            state["moved, done" if new and not old else
                  "moved, pending" if old else "moved, MISSING"] += 1
            if not old and not new:
                print(f"[headermap] {r['today']} is gone and {t} does not exist")
                bad += 1
    for k in sorted(state):
        print(f"[headermap] {k:22} {state[k]:4}")
    print(f"[headermap] {len(rows)} rows, {len(present)} headers in include/"
          + (f", {bad} problem(s)" if bad else ", consistent"))
    return 1 if bad else 0


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    fn = {"render": cmd_render, "check": cmd_check}.get(cmd)
    if not fn:
        sys.exit(__doc__)
    sys.exit(fn())
