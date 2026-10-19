#!/usr/bin/env python3
"""asmlint -- the clobbers of libc370's inline assembler (#427, #125).

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

The second rule (#125): an asm inside a function whose template issues an
SVC or calls out - a system macro that expands to SVC or to a branch into
system code, SVC itself, BALR, BASR - is an extended asm and lists "0",
"1", "14" and "15" among its clobbers.  SVC linkage and the macro
expansions use those four, and an asm that did not say so let cc370 keep a
value in R15 across a STIMER (ftpd#113: a wait loop that ran once).  Which
of the four a given expansion touches is not worth working out: all four,
every time.  An exception says so on the line before it:

    /* asmlint: no-svc <reason> */

Usage:  python3 sdk/asmlint.py [path ...]     default: src include
Exit:   0 clean, 1 an asm breaks a rule
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEYWORD = re.compile(r"\b(?:__asm__|__asm|asm)\b")
VOLATILE = re.compile(r"\s*(?:__volatile__|volatile)?\s*")
EXEMPT = "asmlint: no-memory"
EXEMPT_SVC = "asmlint: no-svc"

# macros that expand to an SVC or a branch into system code, and the
# instructions that leave the asm for code that follows SVC linkage
SVCOPS = {
    "ABEND", "ATTACH", "BLDL", "CHECK", "CLOSE", "DELETE", "DEQ", "DETACH",
    "DYNALLOC", "ENDREQ", "ENQ", "ERASE", "ESTAE", "EXCP", "EXTRACT",
    "FREEMAIN", "FREEPOOL", "GET", "GETMAIN", "GETPOOL", "IDENTIFY", "LINK",
    "LOAD", "LOCATE", "LSPACE", "MODCB", "MODESET", "NOTE", "OBTAIN", "OPEN",
    "POINT", "POST", "PUT", "QEDIT", "RACHECK", "RACINIT", "RDJFCB", "READ",
    "SHOWCAT", "SHOWCB", "SNAP", "SPIE", "STAE", "STIMER", "STOW", "SVC",
    "TESTAUTH", "TGET", "TPUT", "TRKCALC", "TTIMER", "WAIT", "WRITE", "WTO",
    "WTOR", "XCTL", "BALR", "BASR",
}
SVCREGS = ('"0"', '"1"', '"14"', '"15"')


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


def exempt_svc(src, start):
    """True when the line before the asm carries the no-svc exemption."""
    line_start = src.rfind("\n", 0, start)
    prev_start = src.rfind("\n", 0, max(line_start, 0))
    return EXEMPT_SVC in src[prev_start + 1:start]


def depths(code):
    """-> brace depth at every offset of the masked code."""
    out, d = [0] * (len(code) + 1), 0
    for i, c in enumerate(code):
        if c == "{":
            d += 1
        elif c == "}":
            d -= 1
        out[i + 1] = d
    return out


def opcodes(src, code, a, b):
    """The operation of every line of the template in src[a:b]: the string
    literals there, decoded.  A line after the first that starts in column
    1 has a label first."""
    text = ""
    i = a
    while i < b:
        if code[i] == '"':
            j = skip_quoted(src, i)
            text += src[i + 1:j - 1]
            i = j
        else:
            i += 1
    text = text.replace("\\n", "\n").replace("\\t", "\t")
    ops = []
    for k, line in enumerate(text.split("\n")):
        words = line.split()
        if not words or line.lstrip().startswith("*"):
            continue
        labelled = k > 0 and line[:1] not in " \t" and len(words) > 1
        ops.append(words[1] if labelled else words[0])
    return ops


def svc_rule(src, code, depth, start, after, close):
    """True when the asm at start breaks the SVC clobber rule (#125)."""
    if depth[start] <= 0 or exempt_svc(src, start):
        return False                     # file scope: no clobbers there
    colons, d, i = [], 1, after + 1
    while i < close:
        c = code[i]
        if c in "([":
            d += 1
        elif c in ")]":
            d -= 1
        elif c == ":" and d == 1:
            colons.append(i)
        i += 1
    end = colons[0] if colons else close
    if not SVCOPS.intersection(opcodes(src, code, after + 1, end)):
        return False
    if len(colons) < 3:
        return True                      # no clobber section at all
    clobbers = src[colons[2]:close]
    return not all(r in clobbers for r in SVCREGS)


def check(path):
    """-> (line, what) of the asms in path that break a rule."""
    src = open(path, "rb").read().decode("latin-1")
    code = mask(src)
    depth = depths(code)
    bad = []
    for m in KEYWORD.finditer(code):
        after = VOLATILE.match(code, m.end()).end()
        if code[after:after + 1] != "(":
            continue                     # asm("NAME") on a declaration is
        close, colon = operands(code, after)   # found below as no-colon
        line = src.count("\n", 0, m.start()) + 1
        if colon and '"memory"' not in src[m.start():close] \
                and not exempt(src, m.start()):
            bad.append((line, 'extended asm without a "memory" clobber'))
        if svc_rule(src, code, depth, m.start(), after, close):
            bad.append((line, 'SVC or call without the clobbers '
                              '"0", "1", "14", "15"'))
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
        for line, what in check(path):
            problems += 1
            print(f"[asmlint] {os.path.relpath(path, ROOT)}:{line}: {what}")
    print(f"[asmlint] {files} files, {problems} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
