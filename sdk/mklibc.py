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
        python3 sdk/mklibc.py clean      # remove build/sdk + the generated .s
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
# -Wuninitialized is not implied by -Wall in this gcc 3.4.6 and needs -O to run
# at all, so it has to be named here (#102).  It finds #99 -- __loadhi() calling
# fclose() on stack residue -- at the -O1 the build already uses.
CFLAGS = ["-O1", "-Wuninitialized", f'-DVERSION="{VERSION}"',
          f"-I{ROOT}/include", f"-I{ROOT}/src/thdmgr", f"-I{ROOT}/src/time64"]
ASMINC = ["-I", f"{ROOT}/maclib", "-I", f"{ROOT}/sysmac"]   # sysmac vendors SYS1.MACLIB
STARTUPS = ("@@crt0", "@@crt1", "@@crtm")                      # -> separate startfiles


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


VER_C = os.path.join(ROOT, "src", "clib", "@@ver.c")   # build-stamp TU (see compile_ver)


def gitrev():
    """Short HEAD commit for the build stamp; '-dirty' when a TRACKED source
    differs from HEAD (an uncommitted edit -- the provenance case that matters);
    untracked files (stray notes, generated .s/.o) do not count.  'unknown'
    outside a git checkout (e.g. a tarball)."""
    try:
        rev = run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"]).stdout.strip()
        if not rev:
            return "unknown"
        dirty = run(["git", "-C", ROOT, "status", "--porcelain",
                     "--untracked-files=no"]).stdout.strip()
        return rev + ("-dirty" if dirty else "")
    except Exception:
        return "unknown"


def sysroot():
    triple = run([CC370, "-dumpmachine"]).stdout.strip()
    # Derive <prefix> from the driver's own location (<prefix>/bin/cc370) rather
    # than the cc1 path -- robust to the libexec layout / version depth.
    drv = shutil.which(CC370) or os.path.join(os.path.dirname(
        run([CC370, "-print-prog-name=cc1"]).stdout.strip()), "..", "..", "..", "bin", "cc370")
    prefix = os.path.dirname(os.path.dirname(os.path.realpath(drv)))
    base = os.path.join(prefix, triple)
    return triple, os.path.join(base, "include"), os.path.join(base, "lib")


def compile_c(cfile, sfile, extra=(), warnings=None):
    """cc370 -S a .c -> .s, unconditionally.

    The .s files are build output that happens to be written next to its source.
    They used to be skipped when the .s was newer than the .c, which is not a
    staleness test: a .c mtime says nothing about the headers it includes, the
    flags it was compiled with, or the code generator that compiled it.  All
    three went wrong silently -- a fixed cc370 (mvslovers/cc370#14) and an
    edited include/*.h both left the old .s in place and put the old object code
    into libc.a, with nothing in the build output to show a skip (#8).

    Regenerating all 712 costs ~7s, which is the whole of what the skip saved.
    """
    r = run([CC370] + CFLAGS + list(extra) + ["-S", cfile, "-o", sfile])
    if not os.path.exists(sfile) or os.path.getsize(sfile) == 0:
        return "cc370 FAIL %s: %s" % (os.path.basename(cfile),
               "\n".join(l for l in r.stderr.splitlines() if "re-asserted" not in l)[:300])
    # A successful compile's stderr was discarded, so a warning cc370 did issue
    # reached nobody -- which is the other half of why #99 sat unnoticed.  Hand
    # them back so the build can report them (#102).
    if warnings is not None:
        warnings.extend(l.replace(ROOT + "/", "") for l in r.stderr.splitlines()
                        if ": warning:" in l)
    return None


def assemble(src, ofile):
    r = run([AS370] + ASMINC + ["-o", ofile, src])
    # as370 writes an object file even when it flagged statements (rc=8 for an
    # undefined operation code), so "the .o exists" is NOT success: a missing
    # macro silently drops the instruction and ships a wrong object.  That is
    # how __stow() shipped as a no-op (#32).  Trust the return code.
    if r.returncode != 0:
        if os.path.exists(ofile):
            os.remove(ofile)        # never leave a half-assembled object behind
        return "as370 rc=%d %s: %s" % (r.returncode, os.path.basename(src),
                                       (r.stderr or r.stdout).strip()[:300])
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
    # 1. compile every .c -> .s, collect them + the hand-written .asm
    rev = gitrev()      # one git call per build; @@ver.c bakes it in as the stamp
    srcs = []
    warns = []
    for d in C_DIRS:
        for c in sorted(glob.glob(f"{d}/**/*.c", recursive=True)):
            s = c[:-2] + ".s"
            extra = [f'-DLIBC370_REV="{rev}"'] if os.path.abspath(c) == os.path.abspath(VER_C) else ()
            err = compile_c(c, s, extra, warns)
            if err:
                print("  " + err); return 1
            srcs.append(s)
    asms = sorted(glob.glob(f"{ASM_DIR}/*.asm"))
    print(f"[libc] {len(srcs)} .s + {len(asms)} .asm")
    if warns:
        print(f"[libc] {len(warns)} compiler warning(s):")
        for w in warns:
            print("  " + w)

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
    # macros go in the sysroot beside include/lib; as370's real binary lives in
    # <sysroot>/bin, so its default macro path <exedir>/../macros resolves here.
    mac = os.path.join(os.path.dirname(inc), "macros")  # <sysroot>/macros = <prefix>/cc370/macros
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


def cmd_clean():
    """Remove build/sdk AND the generated .s.

    The .s sit next to their .c in src/ and are gitignored, so `rm -rf build`
    used to leave 712 files behind that look like source and are not.  Only a .s
    with a .c sibling is removed -- exactly the set cmd_build() writes, and
    therefore the set that cannot be hand-maintained, since the build overwrites
    it on every run.  A .s without a .c is left alone (there is none today, and
    the build would ignore it anyway).
    """
    n = 0
    for d in C_DIRS:
        for c in glob.glob(f"{d}/**/*.c", recursive=True):
            s = c[:-2] + ".s"
            if os.path.exists(s):
                os.remove(s); n += 1
    shutil.rmtree(BUILD, ignore_errors=True)
    print(f"[clean] {n} generated .s removed, {BUILD} gone")
    return 0


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "all"
    rc = 0
    if cmd == "clean":
        sys.exit(cmd_clean())
    if cmd in ("build", "all"):
        rc = cmd_build()
    if rc == 0 and cmd in ("install", "all"):
        rc = cmd_install()
    sys.exit(rc)
