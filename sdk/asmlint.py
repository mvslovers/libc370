#!/usr/bin/env python3
"""asmlint -- every extended asm in libc370 declares "memory" (#427).

An asm statement with operands that stores into C storage without saying so
lets cc370 (GCC 3.4) keep a value it read before and use it after, or move a
store past the asm.  It happened: memset()/memclr() (#425), vsread() (#426)
and clib_identify_cthread() (#427) all returned stale values.  Telling each
case apart by reading the template is how those were missed, so the rule is
mechanical: an extended asm (one with a ':' operand section) in src/ or
include/ lists "memory" among its clobbers.  The cost is a reload here and
there; measured on the library, 19 of 752 translation units changed, all
reloads.

An asm that must not carry the clobber says so on the line before it:

    /* asmlint: no-memory <reason> */

Usage:  python3 sdk/asmlint.py [path ...]     default: src include
Exit:   0 clean, 1 an asm lacks "memory"
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM = re.compile(r"(?<![\w])(__asm__|__asm|asm)(?![\w])\s*(__volatile__|volatile)?\s*\(")
EXEMPT = "asmlint: no-memory"


def comment_spans(s):
    spans, i, n = [], 0, len(s)
    while i < n:
        if s.startswith("/*", i):
            j = s.find("*/", i + 2)
            j = n if j < 0 else j + 2
            spans.append((i, j))
            i = j
            continue
        if s.startswith("//", i):
            j = s.find("\n", i)
            j = n if j < 0 else j
            spans.append((i, j))
            i = j
            continue
        if s[i] in "\"'":
            q, j = s[i], i + 1
            while j < n and s[j] != q and s[j] != "\n":
                j += 2 if s[j] == "\\" else 1
            i = j + 1
            continue
        i += 1
    return spans


def statements(src):
    """-> (start offset, text, has operands) for every asm statement."""
    spans = comment_spans(src)
    for m in ASM.finditer(src):
        if any(a <= m.start() < b for a, b in spans):
            continue
        depth, i, n, colon = 1, m.end(), len(src), False
        while i < n and depth:
            c = src[i]
            if c == '"':
                j = i + 1
                while j < n and src[j] != '"':
                    j += 2 if src[j] == "\\" else 1
                i = j + 1
                continue
            if src.startswith("/*", i):
                i = src.find("*/", i + 2) + 2
                continue
            if c in "([":
                depth += 1
            elif c in ")]":
                depth -= 1
            elif c == ":" and depth == 1:
                colon = True
            i += 1
        yield m.start(), src[m.start():i], colon


def check(path):
    src = open(path, "rb").read().decode("latin-1")
    bad = []
    for start, text, operands in statements(src):
        if not operands or '"memory"' in text:
            continue
        before = src[max(0, src.rfind("\n", 0, max(0, src.rfind("\n", 0, start)))):start]
        if EXEMPT in before:
            continue
        bad.append(src.count("\n", 0, start) + 1)
    return bad


def main(argv):
    roots = argv or [os.path.join(ROOT, "src"), os.path.join(ROOT, "include")]
    files, problems = 0, 0
    for root in roots:
        walk = [(root, [], [os.path.basename(root)])] if os.path.isfile(root) \
            else os.walk(root)
        for d, _, names in walk:
            for name in sorted(names):
                p = name if os.path.isfile(root) else os.path.join(d, name)
                if not p.endswith((".c", ".h")):
                    continue
                files += 1
                for line in check(p):
                    problems += 1
                    print(f"[asmlint] {os.path.relpath(p, ROOT)}:{line}: "
                          f"extended asm without a \"memory\" clobber")
    print(f"[asmlint] {files} files, {problems} asm statement(s) without \"memory\"")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
