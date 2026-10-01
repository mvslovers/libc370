#!/usr/bin/env python3
"""clibstr split (#256, doc/design-2.0-splits.md sections 2 and 6).

Not a sdk/splithdr.py spec, because the shape differs: string.h is itself a
target and must not pass the other targets on -- the point is that it holds
ISO names only.  So:

  string.h            <- the ISO part of clibstr.h, and strdup
  strings.h (new)     <- strcasecmp, strncasecmp
  ext/strutil.h   <- strcpyp, memcpyp, __patmat, memclr
  unistd.h (new)      <- sleep (from time.h) and usleep (declared nowhere)
  stdlib.h            <- setenv, unsetenv, putenv (from mvs/env.h, which
                         now includes stdlib.h, so its includers keep them)
  removed             <- stricmp, strncmpi (same code as the POSIX pair;
                         libc370's own two callers switch to strcasecmp),
                         bcopy (no caller)

Every file that uses a name from strings.h, ext/strutil.h or unistd.h
gets that include at its first #include line, above any code.
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
    """remove and return the lines from the one starting with start to the
    one starting with end (inclusive), plus one trailing blank line"""
    ls = text.split("\n")
    a = next(i for i, l in enumerate(ls) if l.startswith(start))
    b = next(i for i in range(a, len(ls)) if ls[i].startswith(end))
    if b + 1 < len(ls) and not ls[b + 1].strip():
        b += 1
    return "\n".join(ls[:a] + ls[b + 1:]), "\n".join(ls[a:b + 1]).rstrip("\n")


def header(g, what, body, incs=""):
    return (f"#ifndef {g}\n#define {g}\n/* {what}\n**\n"
            f"** libc370 2.0 (#256, #250).\n*/\n\n{incs}{body}\n\n#endif /* {g} */\n")


src, _ = rd(I("clibstr.h"))
src, cmp_block = cut(src, "/* case-insensitive comparison", "int strncmpi(")
src, strutil = cut(src, "/* copy source string to target", "int __patmat(")
src, memclr = cut(src, "static __inline void *memclr", "}")

# strings.h: the two POSIX names, without the aliases that go
cmp = [l for l in cmp_block.split("\n") if not l.startswith(("int stricmp", "int strncmpi"))]
cmp = "\n".join(cmp).replace(
    "/* case-insensitive comparison; the POSIX names and the MS-style\n"
    "   aliases that predate them here.  All four fold through tolower(),\n"
    "   so they are EBCDIC-correct. */",
    "/* case-insensitive comparison.  Both fold through tolower(), so they\n"
    "   are EBCDIC-correct.  1.x also declared stricmp()/strncmpi(), the\n"
    "   same code under MS-style names; they are gone in 2.0 (#250). */")
assert "stricmp" not in cmp.split("*/")[-1]
wr(I("strings.h"), header("STRINGS_H", "strings.h - POSIX <strings.h>: case-insensitive comparison.",
                          cmp, "#include <stddef.h>\n\n"))
wr(I("ext/strutil.h"), header("EXT_STRUTIL_H",
                                  "ext/strutil.h - libc370's own string helpers: padded copies,\n"
                                  "** pattern match, a clearing memset.",
                                  strutil + "\n\n" + memclr, "#include <stddef.h>\n\n"))

# string.h: the rest of clibstr.h, under string.h's own guard
body = re.sub(r"\A\s*#ifndef CLIBSTR_H\n#define CLIBSTR_H\n", "", src)
body = body[:body.rstrip().rfind("#endif")].rstrip("\n")
sh, shcrlf = rd(I("string.h"))
assert '#include "clibstr.h"' in sh
sh = sh.replace('#include "clibstr.h"', body.strip("\n"))
wr(I("string.h"), sh, shcrlf)
git("rm", "-q", "include/clibstr.h")

# unistd.h: sleep from time.h, and usleep, which no header declared
tm, tmcrlf = rd(I("time.h"))
tm, sleep = cut(tm, "/* sleep() - wait the given number", "int sleep(")
wr(I("time.h"), tm, tmcrlf)
wr(I("unistd.h"), header("UNISTD_H", "unistd.h - the POSIX <unistd.h> functions libc370 has (D2).",
                         sleep + "\n\n/* usleep() - wait about usec microseconds (STIMER in units of\n"
                         "**            26 microseconds, at least one).  Returns 0. */\n"
                         "int usleep(unsigned usec);"))

# stdlib.h: setenv, unsetenv, putenv from mvs/env.h; bcopy goes
sl, slcrlf = rd(I("stdlib.h"))
sl, _ = cut(sl, "void bcopy (", "void bcopy (")
env, envcrlf = rd(I("mvs/env.h"))
moved = []
for fn in ("setenv", "putenv", "unsetenv"):
    env, d = cut(env, f"extern int      {fn}(", f"extern int      {fn}(")
    moved.append(d)
env = env.replace("#define CLIBENV_H\n", "#define CLIBENV_H\n\n#include <stdlib.h>\n", 1)
wr(I("mvs/env.h"), env, envcrlf)
k = sl.index("char *getenv(const char *name);") + len("char *getenv(const char *name);")
sl = sl[:k] + "\n" + "\n".join(moved) + sl[k:]
wr(I("stdlib.h"), sl, slcrlf)

# the removed functions
for f in ("src/clib/stricmp.c", "src/clib/strncmpi.c", "src/clib/bcopy.c"):
    git("rm", "-q", f)
for f in ("src/mvs/env/@@finden.c", "src/mvs/dslist/@@listds.c"):
    t, c = rd(os.path.join(ROOT, f))
    wr(os.path.join(ROOT, f), re.sub(r"\bstricmp\(", "strcasecmp(", t), c)

# includes: clibstr.h -> string.h, and the new headers where their names are used
needs = {
    "strings.h": {"strcasecmp", "strncasecmp"},
    "ext/strutil.h": {"strcpyp", "memcpyp", "__patmat", "memclr"},
    "unistd.h": {"sleep", "usleep"},
}
INC = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"]+)[>"]', re.M)
done = 0
for f in git("ls-files").splitlines():
    if not f.startswith(("src/", "include/", "test/")) or f.startswith("src/wip/") \
            or not f.endswith((".c", ".h")):
        continue
    p = os.path.join(ROOT, f)
    if not os.path.isfile(p) or f in ("include/strings.h", "include/ext/strutil.h",
                                      "include/unistd.h", "include/string.h"):
        continue
    raw = open(p, "rb").read()
    if b"\0" in raw:
        continue
    t, c = rd(p)
    t0 = t
    t = re.sub(r'^([ \t]*#[ \t]*include[ \t]*[<"])clibstr\.h([>"])', r"\1string.h\2", t, flags=re.M)
    code = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    words = set(re.findall(r"[A-Za-z_]\w*", code))
    have = {m.group(2) for m in INC.finditer(t)}
    add = [h for h, n in needs.items() if n & words and h not in have]
    # sleep/usleep defined here, or the word used as something else: skip
    if f in ("src/unistd/sleep.c", "src/unistd/usleep.c"):
        add = [h for h in add if h != "unistd.h"] + (["unistd.h"] if "unistd.h" not in have else [])
    if add:
        m = INC.search(t)
        at = m.start() if m else 0          # no #include at all: the top
        ins = "".join(f"#include <{h}>\n" for h in add)
        t = t[:at] + ins + t[at:]
    if t != t0:
        wr(p, t, c)
        done += 1
        if add:
            print(f"[clibstr] {f}: + {' '.join(add)}")
print(f"[clibstr] {done} file(s) rewritten")
