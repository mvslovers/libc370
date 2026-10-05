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
KEYWORD = re.compile(r"\b(?:__asm__|__asm|asm)\b")
VOLATILE = re.compile(r"\s*(?:__volatile__|volatile)?\s*")
EXEMPT = "asmlint: no-memory"


def skip_quoted(s, i):
    """Index just past the string or character literal that starts at i."""
    q, n = s[i], len(s)
    i += 1
    while i < n and s[i] != q and s[i] != "\n":
        i += 2 if s[i] == "\\" else 1
    return min(i + 1, n)


def blank(out, a, b):
    """Overwrite out[a:b] with blanks, keeping newlines."""
    for k in range(a, b):
        if out[k] != "\n":
            out[k] = " "


def mask(s):
    """s with comments and literal contents blanked, newlines kept, so that
    offsets, line numbers and parentheses of the code survive unchanged."""
    out, i, n = list(s), 0, len(s)
    while i < n:
        if s.startswith("/*", i):
            j = s.find("*/", i + 2)
            j = n if j < 0 else j + 2
            blank(out, i, j)
        elif s.startswith("//", i):
            j = s.find("\n", i)
            j = n if j < 0 else j
            blank(out, i, j)
        elif s[i] in "\"'":
            j = skip_quoted(s, i)
            blank(out, i + 1, j - 1)     # keep the quotes themselves
        else:
            j = i + 1
        i = j
    return "".join(out)


def operands(code, open_paren):
    """-> (closing paren offset, True if a ':' sits at the asm's top level)."""
    depth, i, colon = 1, open_paren + 1, False
    while i < len(code) and depth:
        c = code[i]
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif c == ":" and depth == 1:
            colon = True
        i += 1
    return i - 1, colon


def exempt(src, start):
    """True when the line before the asm carries the exemption comment."""
    line_start = src.rfind("\n", 0, start)
    prev_start = src.rfind("\n", 0, max(line_start, 0))
    return EXEMPT in src[prev_start + 1:start]


def check(path):
    """-> line numbers of the extended asms in path that lack "memory"."""
    src = open(path, "rb").read().decode("latin-1")
    code = mask(src)
    bad = []
    for m in KEYWORD.finditer(code):
        after = VOLATILE.match(code, m.end()).end()
        if code[after:after + 1] != "(":
            continue                     # asm("NAME") on a declaration is
        close, colon = operands(code, after)   # found below as no-colon
        if colon and '"memory"' not in src[m.start():close] \
                and not exempt(src, m.start()):
            bad.append(src.count("\n", 0, m.start()) + 1)
    return bad


def sources(roots):
    for root in roots:
        if os.path.isfile(root):
            yield root
            continue
        for d, _, names in os.walk(root):
            for name in sorted(names):
                if name.endswith((".c", ".h")):
                    yield os.path.join(d, name)


def main(argv):
    roots = argv or [os.path.join(ROOT, "src"), os.path.join(ROOT, "include")]
    files = problems = 0
    for path in sources(roots):
        files += 1
        for line in check(path):
            problems += 1
            print(f"[asmlint] {os.path.relpath(path, ROOT)}:{line}: "
                  f"extended asm without a \"memory\" clobber")
    print(f"[asmlint] {files} files, {problems} asm statement(s) without \"memory\"")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
