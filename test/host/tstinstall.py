#!/usr/bin/env python3
"""tstinstall.py - libc370 #636: `make install` removes libc370's headers of
the previous install, and no others.

sdk/mklibc.py install cleared the whole <sysroot>/include before it copied
the headers, so a header another toolchain had put there - pl1c370's
pl1abi.h - was gone after every libc370 install.

The test runs mklibc's cmd_install() against a throwaway sysroot: sysroot()
is replaced to point there, and BUILD at a directory with a stand-in libc.a
and crtm.o, so neither cc370 nor a build is needed.

  1 a sysroot an older install left (no manifest): the 1.x header
    clibwto.h goes, the libc370 headers are new, pl1abi.h and pl1/x.h stay,
    and the install writes share/libc370/include-files.txt
  2 the next install with that manifest: a header the manifest lists but
    this tree no longer ships goes, the foreign files stay

Usage (from test/host):  python3 tstinstall.py [path to sdk/mklibc.py]
RED, 2026-10-19, against main's mklibc.py: pl1abi.h and pl1/x.h are gone
after case 1.
RC: 0 = every check passed, 1 = at least one did not.
"""
import importlib.util
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
MK = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "sdk", "mklibc.py")
sys.path.insert(0, os.path.dirname(os.path.abspath(MK)))
spec = importlib.util.spec_from_file_location("mklibc", MK)
mk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mk)

bad = 0


def check(cond, what):
    global bad
    print(("  ok    " if cond else "  FAIL  ") + what)
    if not cond:
        bad += 1


def put(path, text="/* x */\n"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as fh:
        fh.write(text)


with tempfile.TemporaryDirectory() as tmp:
    triple = "s370-ibm-mvs"
    inc = os.path.join(tmp, "cc370", triple, "include")
    lib = os.path.join(tmp, "cc370", triple, "lib")
    build = os.path.join(tmp, "build")
    put(os.path.join(build, "libc.a"), "!<arch>\n")
    for crt, _ in mk.STARTFILES:
        put(os.path.join(build, crt), "obj")
    mk.BUILD = build
    mk.sysroot = lambda: (triple, inc, lib)

    # 1: what an install before the manifest left, plus another toolchain's
    put(os.path.join(inc, "clibwto.h"))             # libc370 1.x
    put(os.path.join(inc, "stdio.h"), "old\n")      # libc370, replaced
    put(os.path.join(inc, "pl1abi.h"))              # pl1c370
    put(os.path.join(inc, "pl1", "x.h"))            # a foreign directory
    rc = mk.cmd_install()
    check(rc == 0, "install 1 returns 0")
    check(not os.path.exists(os.path.join(inc, "clibwto.h")),
          "the 1.x header clibwto.h is gone")
    check(open(os.path.join(inc, "stdio.h")).read() != "old\n",
          "stdio.h is the new one")
    check(os.path.isfile(os.path.join(inc, "pl1abi.h")), "pl1abi.h stays")
    check(os.path.isfile(os.path.join(inc, "pl1", "x.h")), "pl1/x.h stays")
    man = os.path.join(tmp, "cc370", triple, "share", "libc370",
                       "include-files.txt")
    check(os.path.isfile(man), "share/libc370/include-files.txt written")

    # 2: a header the previous install shipped and this tree does not
    if os.path.isfile(man):
        with open(man, "a") as fh:
            fh.write("mvs/dropped.h\n")
    put(os.path.join(inc, "mvs", "dropped.h"))
    rc = mk.cmd_install()
    check(rc == 0, "install 2 returns 0")
    check(not os.path.exists(os.path.join(inc, "mvs", "dropped.h")),
          "a header the previous install listed and this one drops is gone")
    check(os.path.isfile(os.path.join(inc, "pl1abi.h")), "pl1abi.h still stays")
    check(os.path.isfile(os.path.join(inc, "mvs", "wto.h")),
          "the libc370 tree is complete again")

print(f"tstinstall: {'FAILED' if bad else 'passed'}")
sys.exit(1 if bad else 0)
