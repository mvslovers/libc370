#!/usr/bin/env python3
"""splithdr -- split one 1.x header across its 2.0 targets (#256, phase 1).

Driven by a spec in sdk/splits/<header>.json:

    {
      "source":   "clibos.h",
      "targets":  { "mvs/pds.h":  {"doc": "PDS directory: BLDL, STOW",
                                   "includes": ["<stddef.h>"]},
                    "mvs/apf.h":  {"includes": ["<ibm/mvs/ihacde.h>"]}, ...},
      "segments": [ {"from": "typedef struct bldl", "to": "int __stow(",
                     "target": "mvs/pds.h"}, ... ]
    }

A segment runs from the first line starting with "from" (after leading
blanks) to the first line at or after it starting with "to".  Every line of
the source outside its include guard and its own #include lines must fall in
exactly one segment, or nothing is written.  Segments move verbatim, comments
and #pragma lines with them.  A target that does not exist is created with a
guard and the doc line; one that exists gets the segments before its final
#endif.  "includes" are added to a target that lacks them.

Then every tracked file that includes the source is rewritten: the include
line becomes the source's own #include lines (the file saw those before) and
the targets whose names the file uses, each skipped if the file already
includes it.  The file therefore sees a subset of what it saw before -- the
parts of the source it never named are gone -- and the gate shows whether
that changed any code.  Finally the source is removed.

Usage:  python3 sdk/splithdr.py sdk/splits/clibos.json
"""
import json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INC = os.path.join(ROOT, "include")
SCOPE = ("src/", "include/", "test/")
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"]+)[>"]')


def git(*args):
    r = subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout


def read(path):
    """text with LF, and whether the file used CRLF"""
    b = open(path, "rb").read()
    return b.decode("latin-1").replace("\r\n", "\n"), b"\r\n" in b


def write(path, text, crlf):
    if crlf:
        text = text.replace("\n", "\r\n")
    open(path, "wb").write(text.encode("latin-1"))


def names(text):
    """identifiers a segment declares: functions, typedefs, tags, macros"""
    out = set()
    code = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    code = re.sub(r"//[^\n]*", " ", code)
    out |= set(re.findall(r"#[ \t]*define[ \t]+([A-Za-z_]\w*)", code))
    # the rest of the preprocessor must not reach the function pattern: a
    # "#pragma linkage(f, OS)" line would otherwise swallow the declaration
    # after it, as one long "linkage(...) ... f(...);" match
    code = re.sub(r"^[ \t]*#[^\n]*", " ", code, flags=re.M)
    out |= set(re.findall(r"\b(?:struct|union|enum)[ \t]+([A-Za-z_]\w*)", code))
    out |= set(re.findall(r"\btypedef\b[^;]*?\b([A-Za-z_]\w*)[ \t]*;", code))
    out |= set(re.findall(r"\b([A-Za-z_]\w*)[ \t]*\([^;{]*\)[^;{]*;", code))
    return out - {"if", "while", "for", "switch", "return", "sizeof", "asm",
                  "__attribute__", "defined", "pragma", "linkage"}


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    spec = json.load(open(sys.argv[1]))
    if "segments" not in spec:
        sys.exit(f"[splithdr] {sys.argv[1]} has no segments: {spec.get('applied_by', 'not a splithdr spec')}")
    src = spec["source"]
    text, crlf = read(os.path.join(INC, src))
    lines = text.split("\n")

    # the parts that stay behind: guard, own includes, blank lines
    guard = None
    m = re.match(r"\s*#ifndef[ \t]+(\w+)\s*\n#define[ \t]+\1", text)
    if m:
        guard = m.group(1)
    own = []                                    # the source's own includes
    owner = [None] * len(lines)
    for i, l in enumerate(lines):
        mi = INCLUDE.match(l)
        if mi:
            own.append((mi.group(1), mi.group(2)))
            owner[i] = "-"
        elif guard and re.match(rf"\s*#(ifndef|define)[ \t]+{guard}\b", l):
            owner[i] = "-"
        elif not l.strip():
            owner[i] = "-"
    if guard:                                   # the guard's #endif: the last one
        last = max(i for i, l in enumerate(lines) if l.strip().startswith("#endif"))
        owner[last] = "-"

    # segments
    per_target = {t: [] for t in spec["targets"]}
    for seg in spec["segments"]:
        a = next((i for i, l in enumerate(lines) if l.lstrip().startswith(seg["from"])), None)
        if a is None:
            sys.exit(f"[splithdr] no line starts with {seg['from']!r}")
        b = next((i for i in range(a, len(lines)) if lines[i].lstrip().startswith(seg["to"])), None)
        if b is None:
            sys.exit(f"[splithdr] nothing after {seg['from']!r} starts with {seg['to']!r}")
        for i in range(a, b + 1):
            if owner[i] not in (None, "-"):
                sys.exit(f"[splithdr] line {i+1} in two segments: {lines[i]!r}")
            if owner[i] is None:
                owner[i] = seg["target"]
        per_target[seg["target"]].append("\n".join(lines[a:b + 1]))
    left = [(i + 1, l) for i, l in enumerate(lines) if owner[i] is None]
    if left:
        for n, l in left[:10]:
            print(f"[splithdr] unassigned line {n}: {l}")
        sys.exit(f"[splithdr] {len(left)} line(s) in no segment; nothing written")

    # targets
    idents = {}
    for t, meta in spec["targets"].items():
        body = "\n\n".join(per_target[t])
        idents[t] = names(body)
        path = os.path.join(INC, t) if not t.startswith("src/") else os.path.join(ROOT, t)
        incs = "".join(f"#include {i}\n" for i in meta.get("includes", []))
        if os.path.exists(path):
            cur, ccrlf = read(path)
            for i in meta.get("includes", []):
                if not re.search(r'#[ \t]*include[ \t]*[<"]' + re.escape(i.strip('<>"')) + '[>"]', cur):
                    # after the guard's #define, or at the top
                    gm = re.match(r"(\s*#ifndef[ \t]+\w+\s*\n#define[ \t]+\w+[^\n]*\n)", cur)
                    cut = gm.end() if gm else 0
                    cur = cur[:cut] + f"#include {i}\n" + cur[cut:]
            k = cur.rstrip().rfind("#endif")
            cur = cur[:k] + f"/* ---- from 1.x {src} " + "-" * max(4, 58 - len(src)) + " */\n" \
                + body + "\n\n" + cur[k:]
            write(path, cur, ccrlf)
            print(f"[splithdr] {t}: +{len(per_target[t])} segment(s)")
        else:
            g = re.sub(r"\W", "_", t).upper()
            doc = meta.get("doc", "")
            out = (f"#ifndef {g}\n#define {g}\n/* {t} - {doc}\n**\n"
                   f"** libc370 2.0 splits this out of {src} (#256).\n*/\n"
                   + "\n" + (incs + "\n" if incs else "") + body + "\n\n#endif /* " + g + " */\n")
            os.makedirs(os.path.dirname(path), exist_ok=True)
            write(path, out, False)
            git("add", os.path.relpath(path, ROOT))
            print(f"[splithdr] {t}: new, {len(per_target[t])} segment(s)")

    # includers
    rx = re.compile(r'^([ \t]*#[ \t]*include[ \t]*)([<"])' + re.escape(src) + r'([>"])[^\n]*$', re.M)
    files = [f for f in git("ls-files").splitlines()
             if f.startswith(SCOPE) and f != f"include/{src}"]
    changed = 0
    for f in files:
        p = os.path.join(ROOT, f)
        if not os.path.isfile(p):
            continue
        raw = open(p, "rb").read()
        if b"\0" in raw:
            continue
        t, fcrlf = read(p)
        if not rx.search(t):
            continue
        words = set(re.findall(r"[A-Za-z_]\w*", t))
        # a header passes everything on: whoever included it may rely on any
        # part of the source, so it keeps every target, not just its own use
        is_header = f.endswith(".h")

        def repl(m):
            # "already included" counts only above this line: an include
            # further down arrives after the code that needed it (@@ascb.c
            # had mvs/wto.h at line 36 and used what it brought at line 22)
            before = t[:m.start()]
            present = {x.group(2) for x in (INCLUDE.match(l) for l in before.split("\n")) if x}
            new = []
            for delim, name in own:
                if name not in present:
                    new.append(name); present.add(name)
            for tgt in spec["targets"]:
                inc_name = tgt[len("src/internal/"):] if tgt.startswith("src/internal/") else tgt
                if (is_header or idents[tgt] & words) and inc_name not in present:
                    new.append(inc_name); present.add(inc_name)
            o, c = m.group(2), m.group(3)
            return "\n".join(m.group(1) + o + n + c for n in new)
        t2 = rx.sub(repl, t)
        t2 = re.sub(r"\n\n\n+", lambda m: m.group(0) if m.group(0) in t else "\n\n", t2)
        write(p, t2, fcrlf)
        changed += 1
    git("rm", "-q", f"include/{src}")
    print(f"[splithdr] {changed} includer(s) rewritten; include/{src} removed")
    left = [f for f in files if os.path.isfile(os.path.join(ROOT, f))
            and rx.search(read(os.path.join(ROOT, f))[0])]
    if left:
        print(f"[splithdr] still including {src}: {', '.join(left)}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
