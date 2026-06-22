#!/usr/bin/env python3
"""Build libc370 -- the cc370 target libc + install it into the cc370 sysroot.

No mbt, no MVS: pure host toolchain (cc370 -S -> as370 -> ar370).  Produces the
three artifacts a cross-libc needs and drops them where cc370 already looks:

  headers   -> <sysroot>/include   (cc370 finds <stdio.h> etc. with no -I)
  libc.a    -> <sysroot>/lib       (the libc370 runtime; -lc pulls it)
  crt0/1/m.o-> <sysroot>/lib       (startup variants, SEPARATE startfiles --
                                    like glibc crt1.o, NOT inside libc.a, so the
                                    linker picks exactly one @@CRT0 and there is
                                    no startup-variant collision)

The sysroot is derived from the driver itself (cc370 -dumpmachine /
-print-prog-name=cc1), so renaming the target triple later needs no edit here.

Usage:  python3 sdk/mklibc.py build      # compile/assemble/archive into build/sdk
        python3 sdk/mklibc.py install    # copy artifacts into the cc370 sysroot
        python3 sdk/mklibc.py all
"""
import os, sys, glob, subprocess, shutil, concurrent.futures as cf

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # libc370 repo root
CC370 = "cc370"                    # the toolchain driver, on PATH
AS370 = "as370"                    # the installed tools, by name -- no repo path baked in
AR370 = "ar370"
BUILD = f"{ROOT}/build/sdk"
VERSION = open(f"{ROOT}/VERSION").read().strip() if os.path.exists(f"{ROOT}/VERSION") else "1.0.11-dev"

# library sources (the c_dirs -- NOT src/wip) + hand-written asm
C_DIRS = [f"{ROOT}/src/{d}" for d in
          ("clib", "cmtt", "crypto", "dyn75", "jes", "os", "racf", "smf", "thdmgr", "time64")]
ASM_DIR = f"{ROOT}/asm"
CFLAGS = ["-O1", f'-DVERSION="{VERSION}"',
          f"-I{ROOT}/include", f"-I{ROOT}/src/thdmgr", f"-I{ROOT}/src/time64"]
ASMINC = ["-I", f"{ROOT}/maclib", "-I", f"{ROOT}/sysmac"]   # sysmac vendors SYS1.MACLIB
STARTUPS = ("@@crt0", "@@crt1", "@@crtm")                      # -> separate startfiles


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def sysroot():
    triple = run([CC370, "-dumpmachine"]).stdout.strip()
    # Derive <prefix> from the driver's own location (<prefix>/bin/cc370) rather
    # than the cc1 path -- robust to the libexec layout / version depth.
    drv = shutil.which(CC370) or os.path.join(os.path.dirname(
        run([CC370, "-print-prog-name=cc1"]).stdout.strip()), "..", "..", "..", "bin", "cc370")
    prefix = os.path.dirname(os.path.dirname(os.path.realpath(drv)))
    base = os.path.join(prefix, triple)
    return triple, os.path.join(base, "include"), os.path.join(base, "lib")


def compile_c(cfile, sfile):
    """cc370 -S a .c -> .s when the .s is missing or older (crent ships most .s;
    71 .c -- @@renmem, @@stow, time64 -- have none and must be compiled)."""
    if os.path.exists(sfile) and os.path.getmtime(cfile) <= os.path.getmtime(sfile):
        return None
    r = run([CC370] + CFLAGS + ["-S", cfile, "-o", sfile])
    if not os.path.exists(sfile) or os.path.getsize(sfile) == 0:
        return "cc370 FAIL %s: %s" % (os.path.basename(cfile),
               "\n".join(l for l in r.stderr.splitlines() if "re-asserted" not in l)[:300])
    return None


def assemble(src, ofile):
    r = run([AS370] + ASMINC + ["-o", ofile, src])
    if not os.path.exists(ofile) or os.path.getsize(ofile) == 0:
        return "as370 FAIL %s: %s" % (os.path.basename(src), (r.stderr or r.stdout)[:300])
    return None


def defines_main(ofile):
    """does this object define @@MAIN? (a stray main must not land in libc.a)"""
    d = open(ofile, "rb").read()
    for i in range(0, len(d) - 79, 80):
        c = d[i:i+80]
        if c[:4] == b"\x02\xc5\xe2\xc4":
            cnt = (c[10] << 8) | c[11]
            for k in range(cnt // 16):
                e = c[16+k*16:32+k*16]
                if e[:8].decode("cp037", "replace").rstrip() == "@@MAIN":
                    return True
    return False


def cmd_build():
    os.makedirs(BUILD, exist_ok=True)
    odir = f"{BUILD}/obj"; os.makedirs(odir, exist_ok=True)
    # 1. gather sources: regenerate missing/stale .s, collect .s + .asm
    srcs = []
    for d in C_DIRS:
        for c in sorted(glob.glob(f"{d}/**/*.c", recursive=True)):
            s = c[:-2] + ".s"
            err = compile_c(c, s)
            if err:
                print("  " + err); return 1
            if os.path.exists(s):
                srcs.append(s)
    asms = sorted(glob.glob(f"{ASM_DIR}/*.asm"))
    print(f"[libc] {len(srcs)} .s + {len(asms)} .asm")

    # 2. assemble everything (parallel)
    def do(src):
        o = f"{odir}/{os.path.basename(src).rsplit('.',1)[0]}.o"
        return (src, o, assemble(src, o))
    objs, crtobjs, fails = [], {}, []
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        for src, o, err in ex.map(do, srcs + asms):
            if err:
                fails.append(err); continue
            stem = os.path.basename(o)[:-2]
            if stem in STARTUPS:                      # crt startfile -> separate
                crtobjs[stem] = o
            else:
                objs.append(o)
    if fails:
        print(f"[libc] {len(fails)} failure(s):")
        for f in fails[:15]:
            print("  " + f)
        return 1

    # 3. safety: no @@MAIN may leak into libc.a
    mains = [o for o in objs if defines_main(o)]
    if mains:
        print("[libc] WARNING excluding @@MAIN-definers from libc.a:",
              [os.path.basename(m) for m in mains])
        objs = [o for o in objs if o not in mains]

    # 4. archive the runtime -> libc.a ; copy crt startfiles
    libc = f"{BUILD}/libc.a"
    r = run([AR370, "rc", libc] + sorted(objs))
    if r.returncode != 0:
        print("[libc] ar370 failed:", r.stderr); return 1
    for stem in STARTUPS:
        if stem not in crtobjs:
            print(f"[libc] MISSING startup {stem}"); return 1
        shutil.copy(crtobjs[stem], f"{BUILD}/{stem.lstrip('@')}.o")  # @@crt0 -> crt0.o
    nmem = sum(1 for l in run([AR370, "t", libc]).stdout.splitlines() if l.strip().endswith("bytes"))
    print(f"[libc] OK -> {libc} ({nmem} members, {os.path.getsize(libc)} bytes)")
    print(f"[libc] startfiles -> crt0.o crt1.o crtm.o")
    return 0


def cmd_install():
    triple, inc, lib = sysroot()
    # as370's real binary lives in <prefix>/bin, so its built-in default macro
    # path (<exedir>/../macros) resolves to <prefix>/macros -- install there.
    mac = os.path.join(os.path.dirname(os.path.dirname(inc)), "macros")  # <prefix>/macros
    libc = f"{BUILD}/libc.a"
    if not os.path.exists(libc):
        print("[install] build first (no", libc + ")"); return 1
    for d in (inc, lib, mac):
        os.makedirs(d, exist_ok=True)
    # headers
    n = 0
    for h in glob.glob(f"{ROOT}/include/*.h"):
        shutil.copy(h, inc); n += 1
    # libc.a + startfiles
    shutil.copy(libc, f"{lib}/libc.a")
    for crt in ("crt0.o", "crt1.o", "crtm.o"):
        shutil.copy(f"{BUILD}/{crt}", f"{lib}/{crt}")
    # assembler macros: sysmac (vendored SYS1.MACLIB) THEN maclib (crent's
    # PDPTOP/PDPPRLG/... override any collision) -> one dir as370 finds by
    # default (<exedir>/../macros); needed for hand-asm + the cc370 one-shot.
    m = 0
    for srcdir in (f"{ROOT}/sysmac", f"{ROOT}/maclib"):
        for f in glob.glob(f"{srcdir}/*"):
            if os.path.isfile(f):
                shutil.copy(f, mac); m += 1
    print(f"[install] target {triple}")
    print(f"[install] {n} headers -> {inc}")
    print(f"[install] libc.a + crt0/1/m.o -> {lib}")
    print(f"[install] {m} macro files -> {mac}")
    print(f"[install] => an as370 installed in {os.path.dirname(mac)}/bin finds these by default;")
    print(f"[install]    otherwise set AS370_MACLIB={mac}")
    return 0


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "all"
    rc = 0
    if cmd in ("build", "all"):
        rc = cmd_build()
    if rc == 0 and cmd in ("install", "all"):
        rc = cmd_install()
    sys.exit(rc)
