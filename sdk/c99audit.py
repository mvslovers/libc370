#!/usr/bin/env python3
"""c99audit -- what libc370 provides of the C99 library, name by name (#325).

sdk/c99.tsv lists every function, macro and type of C99 clauses 7.2-7.24 with
its header, its kind and, for functions, the C99 prototype.  For each name
this asks the tree (include/, build/sdk/libc.a) with cc370:

  header    does include/<header> exist?
  declared  does the header declare it (function: as a function; function-
            like macro: as a macro or a function; macro: #ifdef; expression:
            usable; typedef: usable as a type; struct: complete)?
  as C99    function only: does the header's declaration agree with the C99
            prototype (no "conflicting types" when both are seen)?
  libc.a    function only: does a program referring to it link against
            build/sdk/libc.a (+ cc370's libcc370rt.a)?  One link refers to
            all of them; a name is present when its MVS symbol is not in
            ld370's unresolved list.

Every probe compiles with -fno-builtin, so the compiler's own knowledge of a
library function stands in for neither the header nor the archive, and with
-nostdinc -I include, so it reads this tree, not the installed sysroot.

cc370 maps a C name to an MVS symbol by upper-casing it, turning '_' into
'@' and cutting it to 8 characters.  Names that land on one symbol are
reported as a group: one link cannot tell them apart.

Usage:  make build && python3 sdk/c99audit.py [--out docs/c99-audit.md]
"""
import argparse, collections, datetime, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAT = os.path.join(ROOT, "sdk", "c99.tsv")
INC = os.path.join(ROOT, "include")
LIB = os.path.join(ROOT, "build", "sdk")
CFLAGS = ["-std=gnu99", "-fno-builtin", "-nostdinc", "-I", INC,
          "-Werror-implicit-function-declaration"]
# Headers libc370 leaves out on purpose (#342, maintainer, 2026-10-03), and why.
ABSENT = {
    "complex.h": "no consumer in the ecosystem; it would build on the C99 "
                 "<math.h> additions (#340), which do not exist yet",
    "fenv.h": "the IEEE model it controls does not match S/370 HFP: no "
              "selectable rounding mode, no exception flags (an exponent "
              "overflow is a program check, S0CC); a header that answers "
              "'unsupported' at run time would be worse than one that is "
              "missing at compile time",
    "tgmath.h": "type-generic macros over <math.h> and <complex.h>; only "
                "useful once every variant exists",
}
TYPEHDRS = ("stddef.h", "stdarg.h", "stdio.h", "stdint.h", "wchar.h", "time.h",
            "wctype.h", "inttypes.h", "setjmp.h", "fenv.h", "complex.h")


def load():
    rows = []
    for line in open(CAT):
        if line.startswith("#") or not line.strip():
            continue
        h, k, n, p = (line.rstrip("\n").split("\t") + [""])[:4]
        rows.append(dict(header=h, kind=k, name=n, proto=p))
    return rows


def mvs(name):
    return name.upper().replace("_", "@")[:8]


def compiles(src, tmp):
    c = os.path.join(tmp, "p.c")
    with open(c, "w") as f:
        f.write(src)
    r = subprocess.run(["cc370"] + CFLAGS + ["-S", "-o", os.devnull, c],
                       capture_output=True, text=True)
    return r.returncode == 0, r.stderr


def probe_declared(r, tmp):
    h, n, k = r["header"], r["name"], r["kind"]
    inc = f"#include <{h}>\n"
    if k == "f":
        return compiles(inc + f"void *probe = (void *)&{n};\n", tmp)[0]
    if k == "g":
        if compiles(inc + f"#ifndef {n}\n#error no macro\n#endif\n", tmp)[0]:
            return True
        return compiles(inc + f"void *probe = (void *)&{n};\n", tmp)[0]
    if k == "m":
        return compiles(inc + f"#ifndef {n}\n#error no macro\n#endif\n", tmp)[0]
    if k == "e":
        return compiles(inc + f"int probe(void) {{ return (int)sizeof({n}); }}\n", tmp)[0]
    if k == "t":
        return compiles(inc + f"{n} *probe;\n", tmp)[0]
    if k == "s":
        return compiles(inc + f"int probe = (int)sizeof(struct {n});\n", tmp)[0]
    if k == "h":
        return True
    raise SystemExit(f"unknown kind {k} for {n}")


def probe_as_c99(r, present, tmp):
    """The header's declaration agrees with C99's prototype."""
    pre = "".join(f"#include <{t}>\n" for t in TYPEHDRS
                  if t in present and t != r["header"])
    # "(getc)(...)": a function that is also a macro is declared without
    # the macro expanding, as C99 7.1.4 allows
    proto = re.sub(rf"(?<=[\s*]){r['name']}\(", f"({r['name']})(", r["proto"], count=1)
    ok, err = compiles(pre + f"#include <{r['header']}>\n{proto};\n", tmp)
    if ok:
        return True, ""
    m = re.search(r"(conflicting types[^\n]*|error:[^\n]*)", err)
    return False, (m.group(1) if m else err.strip().splitlines()[-1]).strip()


def link_unresolved(funcs, tmp):
    """One program referring to every function; ld370's unresolved list."""
    src = []
    for i, n in enumerate(funcs):
        src.append(f"extern int {n}();")
    src.append("void *probe[] = {")
    src += [f"  (void *)&{n}," for n in funcs]
    src.append("};")
    src.append("int main(void) { return probe[0] != 0; }")
    c = os.path.join(tmp, "link.c")
    with open(c, "w") as f:
        f.write("\n".join(src) + "\n")
    r = subprocess.run(["cc370", "-std=gnu99", "-fno-builtin", "-w", "-nostdinc",
                        "-I", INC, "-L", LIB, c, "-o", os.path.join(tmp, "probe")],
                       capture_output=True, text=True)
    out = r.stdout + r.stderr
    if "unresolved" not in out and r.returncode != 0:
        raise SystemExit("the link probe failed for another reason:\n" + out)
    return {l.strip() for l in out.splitlines()
            if re.fullmatch(r"\s+[A-Z0-9@$#]{1,8}", l)}


def defined_in_src(name):
    """Does some library source define a function of this C name?"""
    r = subprocess.run(["git", "-C", ROOT, "grep", "-l", "-E",
                        rf"^[A-Za-z_].*[ *]{name}[[:space:]]*\(", "--", "src"],
                       capture_output=True, text=True)
    return bool(r.stdout.strip())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "c99-audit.md"))
    a = ap.parse_args()
    if not os.path.exists(os.path.join(LIB, "libc.a")):
        sys.exit("build first: make build")
    rows = load()
    headers = list(dict.fromkeys(r["header"] for r in rows))
    present = {h for h in headers if os.path.exists(os.path.join(INC, h))}
    funcs = [r["name"] for r in rows if r["kind"] == "f"]

    with tempfile.TemporaryDirectory() as tmp:
        unres = link_unresolved(funcs, tmp)
        for r in rows:
            r["has_header"] = r["header"] in present
            r["declared"] = r["has_header"] and probe_declared(r, tmp)
            r["macro"] = r["has_header"] and r["kind"] == "f" and compiles(
                f"#include <{r['header']}>\n#ifndef {r['name']}\n#error\n#endif\n", tmp)[0]
            r["as_c99"], r["why"] = (None, "")
            if r["kind"] == "f" and r["declared"]:
                r["as_c99"], r["why"] = probe_as_c99(r, present, tmp)
            r["lib"] = (mvs(r["name"]) not in unres) if r["kind"] == "f" else None

    # names that share an MVS symbol: the link cannot tell them apart
    groups = collections.defaultdict(list)
    for n in funcs:
        groups[mvs(n)].append(n)
    shared = {s: ns for s, ns in groups.items() if len(ns) > 1}
    for r in rows:
        if r["kind"] == "f" and mvs(r["name"]) in shared:
            r["shared"] = shared[mvs(r["name"])]
            if r["lib"]:
                r["lib"] = "shared" if not defined_in_src(r["name"]) else True
        else:
            r["shared"] = None

    write(a.out, rows, headers, present, shared)
    print(f"[c99audit] {len(rows)} names, {len(funcs)} functions -> "
          f"{os.path.relpath(a.out, ROOT)}")
    return 0


def mark(v):
    return {True: "yes", False: "**no**", None: "", "shared": "symbol only"}[v]


def write(path, rows, headers, present, shared):
    cc = subprocess.run(["cc370", "--version"], capture_output=True,
                        text=True).stdout.splitlines()[0]
    rev = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
                         capture_output=True, text=True).stdout.strip()
    ver = open(os.path.join(ROOT, "VERSION")).read().strip()
    o = []
    o.append("# C99 library audit\n")
    o.append(f"Generated by `sdk/c99audit.py` on {datetime.date.today()} from "
             f"libc370 {ver} (`{rev}`) with {cc}. Do not edit by hand: "
             "change `sdk/c99.tsv` or the library and rerun it.\n")
    o.append("**What it checks.** For every function, macro and type of C99 "
             "clauses 7.2-7.24 (`sdk/c99.tsv`): whether `include/` declares it, "
             "whether a function's declaration agrees with the C99 prototype, and "
             "whether a program that refers to the function links against "
             "`build/sdk/libc.a`. All probes compile with `-fno-builtin` and "
             "`-nostdinc -I include`, so neither the compiler's built-in "
             "knowledge nor the installed sysroot answers for the tree. See the "
             "script's header for the details.\n")
    o.append("**The catalogue** is hand-written from the standard's clause list. "
             "It was checked two ways: every standard library function GCC 3.4.6 "
             "knows as a builtin (`gcc/builtins.def`, 277 names) is in it, and the "
             "host C library (macOS) knows all of its function names, so none is "
             "misspelt. Macros and types have no such independent check.\n")
    o.append("**Columns.** *declared* - the header provides the name in the form "
             "C99 requires (a function as a function; a function-like macro as a "
             "macro or function). *macro* - a function is also a macro. *as C99* "
             "- the declaration agrees with the C99 prototype. *libc.a* - a "
             "reference links. *symbol only* - the MVS symbol is defined, but by "
             "another C name that shares it (see *Shared MVS names*).\n")

    # summary
    o.append("## Summary\n")
    o.append("| header | in `include/` | names | declared | functions | as C99 | in `libc.a` |")
    o.append("|---|---|---|---|---|---|---|")
    tot = collections.Counter()
    for h in headers:
        rs = [r for r in rows if r["header"] == h]
        fs = [r for r in rs if r["kind"] == "f"]
        d = sum(1 for r in rs if r["declared"])
        c = sum(1 for r in fs if r["as_c99"])
        l = sum(1 for r in fs if r["lib"] is True)
        o.append(f"| `{h}` | {'yes' if h in present else '**no**'} | {len(rs)} | "
                 f"{d} | {len(fs)} | {c} | {l} |")
        tot.update(names=len(rs), declared=d, funcs=len(fs), c99=c, lib=l)
    o.append(f"| **all** | {len(present)}/{len(headers)} | {tot['names']} | "
             f"{tot['declared']} | {tot['funcs']} | {tot['c99']} | {tot['lib']} |\n")

    # findings
    o.append("## Findings\n")
    miss = [h for h in headers if h not in present]
    o.append("### Headers that do not exist\n")
    o.append(", ".join(f"`{h}`" for h in miss) + "\n" if miss else "None.\n")
    gone = [h for h in miss if h in ABSENT]
    if gone:
        o.append("Left out on purpose (#342), so not gaps to fill:\n")
        o += [f"- `<{h}>`: {ABSENT[h]}." for h in gone]
        o.append("")

    def section(title, sel, note=""):
        rs = [r for r in rows if sel(r)]
        o.append(f"### {title} ({len(rs)})\n")
        if note:
            o.append(note + "\n")
        if not rs:
            o.append("None.\n")
            return
        by = collections.OrderedDict()
        for r in rs:
            by.setdefault(r["header"], []).append(r)
        for h, rr in by.items():
            o.append(f"- `{h}`: " + ", ".join(f"`{r['name']}`" for r in rr))
        o.append("")

    section("In `libc.a`, but the header does not declare them",
            lambda r: r["kind"] == "f" and r["lib"] is True and r["has_header"]
            and not r["declared"],
            "A program can call them only with its own prototype. For the "
            "`<ctype.h>` family this is C99 7.1.4: each must also exist as a "
            "function, declared, beside its macro.")
    section("Declared, but not in `libc.a`",
            lambda r: r["kind"] == "f" and r["declared"] and r["lib"] is False,
            "These compile and then fail to link.")
    section("Declared differently from C99",
            lambda r: r["as_c99"] is False)
    rs = [r for r in rows if r["as_c99"] is False]
    if rs:
        o.append("| header | function | compiler says |")
        o.append("|---|---|---|")
        for r in rs:
            o.append(f"| `{r['header']}` | `{r['name']}` | {r['why'].replace('|', '/')} |")
        o.append("")
    section("Functions of existing headers that are missing entirely",
            lambda r: r["kind"] == "f" and r["has_header"] and not r["declared"]
            and r["lib"] is False)
    section("Macros, types and expressions of existing headers that are missing",
            lambda r: r["kind"] != "f" and r["has_header"] and not r["declared"])

    o.append(f"### Shared MVS names ({len(shared)} groups)\n")
    o.append("C names that cc370 maps to the same 8-character MVS symbol. libc370 "
             "cannot define more than one of each group under its plain name; the "
             "others need an `asm(\"...\")` label in their declaration.\n")
    if shared:
        o.append("| MVS symbol | C names | defined in `src/` |")
        o.append("|---|---|---|")
        for s, ns in sorted(shared.items()):
            d = [n for n in ns if defined_in_src(n)]
            o.append(f"| `{s}` | " + ", ".join(f"`{n}`" for n in ns) + " | " +
                     (", ".join(f"`{n}`" for n in d) or "none") + " |")
        o.append("")

    # the full table
    o.append("## Every name\n")
    for h in headers:
        rs = [r for r in rows if r["header"] == h]
        o.append(f"### `<{h}>`" + ("" if h in present else " - **header missing**") + "\n")
        o.append("| name | kind | declared | macro | as C99 | libc.a |")
        o.append("|---|---|---|---|---|---|")
        kinds = {"f": "function", "g": "function-like macro", "m": "macro",
                 "e": "expression", "t": "type", "s": "struct", "h": "header"}
        for r in rs:
            o.append(f"| `{r['name']}` | {kinds[r['kind']]} | {mark(r['declared'])} | "
                     f"{'yes' if r['macro'] else ''} | {mark(r['as_c99'])} | "
                     f"{mark(r['lib'])} |")
        o.append("")
    with open(path, "w") as f:
        f.write("\n".join(o))


if __name__ == "__main__":
    sys.exit(main())
