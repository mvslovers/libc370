#!/usr/bin/env python3
"""hdrcheck -- every public header compiles on its own under -Wall -Werror.

Consumers build with -Wall -Werror (the ecosystem rule), and a header that
warns stops every program that includes it.  On 2026-10-04, 38 of the 130
headers did: a "/*" inside a comment, #pragma pack, a missing <stddef.h>, a
void member (#414, #415).  libc370's own build does not notice, because it
compiles without -Werror and a header's warning reaches it only through the
sources that happen to include it.  So each header is compiled alone:

    #include <that/header.h>

with  cc370 -S -Wall -Werror -std=gnu99 -I include.

Usage:  python3 sdk/hdrcheck.py           (cc370 from PATH)
Exit:   0 every header passes, 1 one does not, 2 cc370 is missing
"""
import concurrent.futures as cf
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INC = os.path.join(ROOT, "include")
FLAGS = ["-S", "-Wall", "-Werror", "-std=gnu99", "-I", INC, "-o", os.devnull]


def headers():
    for d, _, names in os.walk(INC):
        for name in sorted(names):
            if name.endswith(".h"):
                yield os.path.relpath(os.path.join(d, name), INC)


def compile_one(tmp, header):
    """-> (header, None) if it compiles cleanly, else (header, first line)."""
    src = os.path.join(tmp, header.replace("/", "_") + ".c")
    with open(src, "w") as f:
        f.write(f"#include <{header}>\nint hdrcheck_tu_is_not_empty;\n")
    r = subprocess.run(["cc370"] + FLAGS + [src], capture_output=True, text=True)
    if r.returncode == 0:
        return header, None
    lines = [l for l in r.stderr.splitlines() if "warning" in l or "error" in l]
    return header, (lines[0] if lines else r.stderr.strip()[:200])


def main():
    if not shutil.which("cc370"):
        print("[hdrcheck] cc370 not on PATH")
        return 2
    hs = list(headers())
    with tempfile.TemporaryDirectory() as tmp, cf.ThreadPoolExecutor(8) as ex:
        results = list(ex.map(lambda h: compile_one(tmp, h), hs))
    bad = [(h, e) for h, e in results if e]
    for h, e in bad:
        print(f"[hdrcheck] {h}: {e}")
    print(f"[hdrcheck] {len(hs)} headers, {len(hs) - len(bad)} clean under "
          f"-Wall -Werror, {len(bad)} not")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
