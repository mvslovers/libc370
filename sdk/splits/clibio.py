#!/usr/bin/env python3
"""clibio split (#256, internals/design-2.0-splits.md section 1).

stdio.h included clibio.h and nothing else, so clibio.h's content moves
into stdio.h, less what is not ISO C:

  src/internal/fileio.h  __caller, the functions called under the FILE lock
                         (__fflush ... __fwrite), __fpterm/__fpfree/__fptmp,
                         vvprintf/vvscanf, __ddbusy: libc370's stdio internals
  mvs/dynalloc.h         __dsalc, __dsalcf, __dsfree
  mvs/pds.h              __renmem, __delmem
  mvs/file.h (new)       __fabandon

stdio.h keeps what a standard macro expands to: the FILE struct and its
_FILE_* flags (feof/ferror read them) and __gtin/__gtout/__gterr (stdin,
stdout, stderr call them).

Every file that uses a name from one of the other targets gets that header
at its first #include line; direct includes of clibio.h become stdio.h.
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
I = lambda p: os.path.join(ROOT, "include", p)


def git(*a):
    r = subprocess.run(["git", "-C", ROOT] + list(a), capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"git {' '.join(a)}: {r.stderr}")
    return r.stdout


def rd(p):
    b = open(p, "rb").read()
    return b.decode("latin-1").replace("\r\n", "\n"), b"\r\n" in b


def wr(p, t, crlf=False):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "wb").write((t.replace("\n", "\r\n") if crlf else t).encode("latin-1"))


def cut(text, start, end):
    ls = text.split("\n")
    a = next(i for i, l in enumerate(ls) if l.startswith(start))
    b = next(i for i in range(a, len(ls)) if ls[i].startswith(end))
    while b + 1 < len(ls) and not ls[b + 1].strip():
        b += 1
    return "\n".join(ls[:a] + ls[b + 1:]), "\n".join(ls[a:b + 1]).rstrip("\n")


def append(path, marker, body, includes=()):
    t, c = rd(path)
    for inc in includes:
        if not re.search(r'#[ \t]*include[ \t]*[<"]' + re.escape(inc) + '[>"]', t):
            gm = re.match(r"(\s*#ifndef[ \t]+\w+\s*\n#define[ \t]+\w+[^\n]*\n)", t)
            k = gm.end() if gm else 0
            t = t[:k] + f"#include <{inc}>\n" + t[k:]
    k = t.rstrip().rfind("#endif")
    t = t[:k] + f"/* ---- from 1.x {marker} " + "-" * 50 + " */\n" + body + "\n\n" + t[k:]
    wr(path, t, c)


def new(path, guard, what, body, includes=()):
    incs = "".join(f"#include <{i}>\n" for i in includes)
    wr(path, f"#ifndef {guard}\n#define {guard}\n/* {what}\n**\n"
             f"** libc370 2.0 splits this out of clibio.h (#256).\n*/\n\n"
             f"{incs}\n{body}\n\n#endif /* {guard} */\n")


src, _ = rd(I("clibio.h"))
src, caller = cut(src, "/* return name of function that called", "extern char *   __caller(")
src, locked = cut(src, "/* the following are functions that should only", "extern size_t   __fwrite(")
src, term = cut(src, "/* __fpterm() - the teardown tail", "extern int      __fptmp(")
src, engines = cut(src, "/* the formatting engines", "extern int      vvscanf(")
src, dsalc = cut(src, "/* __dsalc() allocate dataset", "extern int __dsfree(")
src, members = cut(src, "/* __renmem() rename PDS member", "extern int __delmem(")
src, abandon = cut(src, "/* __fabandon() close a FILE", "extern int __fabandon(")
src, ddbusy = cut(src, "/* __ddbusy() - would this OPEN", "extern int __ddbusy(")
for gone in ("__caller(", "__fflush(", "__fpterm(", "vvprintf(", "__dsalc(", "__renmem(",
             "__fabandon(", "__ddbusy("):
    assert gone not in src, gone

# stdio.h: the rest of clibio.h, under stdio.h's own guard
body = re.sub(r"\A\s*#ifndef CLIBIO_H\n#define CLIBIO_H\n", "", src)
body = body[:body.rstrip().rfind("#endif")].rstrip("\n")
sh, shc = rd(I("stdio.h"))
assert '#include "clibio.h"' in sh
wr(I("stdio.h"), sh.replace('#include "clibio.h"', body.strip("\n")), shc)
git("rm", "-q", "include/clibio.h")

new(os.path.join(ROOT, "src/internal/fileio.h"), "SRC_INTERNAL_FILEIO_H",
    "fileio.h - internal: libc370's stdio below the ISO functions.",
    "\n\n".join((caller, locked, term, engines, ddbusy)), ("stdarg.h", "stddef.h", "stdio.h"))
new(I("mvs/file.h"), "MVS_FILE_H", "mvs/file.h - MVS extensions to a stdio FILE.",
    abandon, ("stdio.h",))
append(I("mvs/dynalloc.h"), "clibio.h", dsalc)
append(I("mvs/pds.h"), "clibio.h", members)
git("add", "src/internal/fileio.h", "include/mvs/file.h")

names = {
    "fileio.h": {"__caller", "__fflush", "__fflnl", "__fpswt", "__fpupc", "__fgetc", "__fgets",
                 "__fputc", "__fputs", "__fread", "__reopen", "__fseek", "__fwrite", "__fpterm",
                 "__fpfree", "__fptmp", "vvprintf", "vvscanf", "__ddbusy"},
    "mvs/dynalloc.h": {"__dsalc", "__dsalcf", "__dsfree"},
    "mvs/pds.h": {"__renmem", "__delmem"},
    "mvs/file.h": {"__fabandon"},
}
INC = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"]+)[>"]', re.M)
skip = {"include/stdio.h", "src/internal/fileio.h", "include/mvs/file.h",
        "include/mvs/dynalloc.h", "include/mvs/pds.h"}
done = 0
for f in git("ls-files").splitlines():
    if not f.startswith(("src/", "include/", "test/")) or not f.endswith((".c", ".h")) or f in skip:
        continue
    p = os.path.join(ROOT, f)
    if not os.path.isfile(p):
        continue
    raw = open(p, "rb").read()
    if b"\0" in raw:
        continue
    t, c = rd(p)
    t0 = t
    t = re.sub(r'^([ \t]*#[ \t]*include[ \t]*[<"])clibio\.h([>"])', r"\1stdio.h\2", t, flags=re.M)
    code = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    code = re.sub(r"//[^\n]*", " ", code)
    words = set(re.findall(r"[A-Za-z_]\w*", code))
    have = {m.group(2) for m in INC.finditer(t)}
    add = [h for h, n in names.items() if n & words and h not in have]
    if add:
        m = INC.search(t)
        at = m.start() if m else 0
        t = t[:at] + "".join(f"#include <{h}>\n" for h in add) + t[at:]
    if t != t0:
        wr(p, t, c)
        done += 1
print(f"[clibio] {done} file(s) rewritten")
