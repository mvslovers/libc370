#!/usr/bin/env python3
"""Build libc370 -- the cc370 target libc + install it into the cc370 sysroot.

No mbt, no MVS: pure host toolchain (cc370 -S -> as370 -> ar370).  Produces the
three artifacts a cross-libc needs and drops them where cc370 already looks:

  headers   -> <sysroot>/include   (cc370 finds <stdio.h> etc. with no -I)
  libc.a    -> <sysroot>/lib       (the libc370 runtime; -lc pulls it)
  crtm.o    -> <sysroot>/lib       (the nested-startup startfile; it defines
                                    @@CRT0 too, so it stays outside libc.a.
                                    The C startup @@CRT0 is a member of libc.a
                                    (#159); crt0.o and crt1.o, its copies for
                                    cc370 before 1.4.0, are gone since 2.4.0)

The sysroot is derived from the driver itself (cc370 -dumpmachine /
-print-prog-name=cc1), so renaming the target triple later needs no edit here.

Usage:  python3 sdk/mklibc.py build      # compile/assemble/archive into build/sdk
        python3 sdk/mklibc.py install    # copy artifacts into the cc370 sysroot
        python3 sdk/mklibc.py stage DIR  # the same tree into DIR/{include,lib,macros}
        python3 sdk/mklibc.py clean      # remove build/sdk + the generated .s
        python3 sdk/mklibc.py all
"""
import os, re, sys, glob, subprocess, shutil, collections, concurrent.futures as cf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dupscan                    # duplicate-external gate, see #151

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # libc370 repo root
CC370 = "cc370"                    # the toolchain driver, on PATH
AS370 = "as370"                    # the installed tools, by name -- no repo path baked in
AR370 = "ar370"
BUILD = f"{ROOT}/build/sdk"
VERSION = open(f"{ROOT}/VERSION").read().strip() if os.path.exists(f"{ROOT}/VERSION") else "1.0.11-dev"

# library sources: every .c and .asm under src/, where src/ mirrors include/
# (#278) and assembler sits beside its C.  Code kept but never built lives in
# attic/, outside src/.  All objects share one directory and the archive
# member is <stem>.o, so a stem may exist only once -- sources() enforces it.
SRC_DIR = f"{ROOT}/src"
# -Os since #344: 2.9% less text than -O1 (284,430 -> 276,313 bytes), and the
# MVS test series ran identically with either (libc370 55/55, rexx370 and
# brexx370 suites against it too).  LIBC370_OPT=-O1 builds the old way, should
# a problem need the comparison.
OPT = os.environ.get("LIBC370_OPT", "-Os")
if not re.fullmatch(r"-O[0123s]", OPT):
    sys.exit(f"LIBC370_OPT={OPT!r}: want -O0, -O1, -O2, -O3 or -Os")
# -Wuninitialized is not implied by -Wall in this gcc 3.4.6 and needs -O to run
# at all, so it has to be named here (#102).  It finds #99 -- __loadhi() calling
# fclose() on stack residue -- at the optimisation level the build uses.
# -std=gnu99: libc370 is C99 (gnu99), like every project in the ecosystem.
# Without it GCC 3.4 defaults to gnu89, and a C99 construct such as a
# declaration in a for loop's first clause does not compile.  -trigraphs
# after it: the cc370 driver enables trigraphs by default and -std=gnu99
# turns them off again, but @@estae.c spells || as ??!??! and headers do too.
CFLAGS = [OPT, "-std=gnu99", "-trigraphs", "-Wuninitialized", f'-DVERSION="{VERSION}"',
          f"-I{ROOT}/include", f"-I{ROOT}"]   # private headers: "src/internal/x.h" (D10)
# assembler macros, in search order: libc370's own (maclib/), the five JES2
# mappings from HASPSRC (hasp/), then SYS1.MACLIB and SYS1.AMODGEN from
# maclib370 (#494; maclib370_dirs()).  Up to 2.7.x libc370 vendored the IBM
# members it uses in sysmac/.
OWN_MACLIBS = (f"{ROOT}/maclib", f"{ROOT}/hasp")
# the IBM members that sysmac/ installed into <sysroot>/macros up to 2.7.x;
# install removes them there, where a stale copy would win over maclib370
SYSMAC_REMOVED = f"{ROOT}/sdk/sysmac-removed.txt"
STARTUPS = ("@@crtm",)              # -> separate startfile, never archived
# the startfiles installed beside libc.a.  crt0.o and crt1.o were copies of
# the archived @@CRT0 for cc370 before 1.4.0 (#159); 2.4.0 needs 1.4.0
STARTFILES = (("crtm.o", "@@crtm"),)
STALE_STARTFILES = ("crt0.o", "crt1.o")   # an earlier install left them


def run(cmd, cwd=None):
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)


def maclib370_dirs():
    """The directories holding maclib370's members (#494).

    MACLIB370 names a maclib370 tree: an installed or unpacked one (with
    macros/), or a checkout (with maclib/ and amodgen/).  Without it, the one
    an installed as370 searches by itself: <exedir>/../maclib370/macros.

    Passed with -I in every case, so it comes before the as370 defaults: a
    <sysroot>/macros an older libc370 left behind still holds IBM copies, and
    those must not stand in for maclib370 unnoticed.  Fails when none is
    found, rather than leaving as370 to find whatever its defaults hold.
    """
    env = os.environ.get("MACLIB370")
    if env:
        if os.path.isdir(os.path.join(env, "macros")):
            return [os.path.join(env, "macros")]
        if os.path.isdir(os.path.join(env, "maclib")) and \
                os.path.isdir(os.path.join(env, "amodgen")):
            return [os.path.join(env, "maclib"), os.path.join(env, "amodgen")]
        if glob.glob(os.path.join(env, "*.macro")):
            return [env]
        sys.exit(f"MACLIB370={env}: neither macros/, nor maclib/ + amodgen/, "
                 f"nor *.macro there")
    exe = shutil.which(AS370)
    if exe:
        d = os.path.join(os.path.dirname(os.path.realpath(exe)), "..",
                         "maclib370", "macros")
        if os.path.isdir(d):
            return [os.path.normpath(d)]
    sys.exit("maclib370 not found: install it beside cc370 "
             "(<sysroot>/maclib370/macros) or set MACLIB370 to a maclib370 "
             "tree or checkout (#494)")


_asminc = None


def asminc():
    global _asminc
    if _asminc is None:
        _asminc = [a for d in OWN_MACLIBS + tuple(maclib370_dirs())
                   for a in ("-I", d)]
    return _asminc


VER_C = "@@ver.c"       # build-stamp TU, found by name wherever it sits (see gitrev)


def sources(ext):
    """every library source with this extension, sorted by path"""
    return sorted(glob.glob(f"{SRC_DIR}/**/*{ext}", recursive=True))


def stem_clashes():
    """stems that more than one source uses: one object would overwrite the other"""
    seen = collections.defaultdict(list)
    for p in sources(".c") + sources(".asm"):
        seen[os.path.basename(p).rsplit(".", 1)[0].lower()].append(os.path.relpath(p, ROOT))
    return {k: v for k, v in seen.items() if len(v) > 1}


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

    The source is named relative to the repository root, from the root: it is
    what __FILE__ expands to, and assert() puts __FILE__ into the object.  An
    absolute path wrote the builder's directory into libc.a (tm64syr.c), so two
    checkouts never built the same library and sdk/gate.py could not compare
    them (#256).
    """
    r = run([CC370] + CFLAGS + list(extra) +
            ["-S", os.path.relpath(cfile, ROOT), "-o", os.path.relpath(sfile, ROOT)],
            cwd=ROOT)
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
    r = run([AS370] + asminc() + ["-o", ofile, src])
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
    clash = stem_clashes()
    if clash:
        for k, v in sorted(clash.items()):
            print(f"[libc] stem {k} used by {', '.join(v)}: one object would replace the other")
        return 1
    # parallel, as the assembly below (#37): the compile was 6.7 s of an
    # 8.2 s build.  map() keeps the source order, so the warnings and the
    # list of .s come out as a serial build gives them; every failure is
    # reported, not only the first.
    def comp(c):
        s = c[:-2] + ".s"
        extra = [f'-DLIBC370_REV="{rev}"'] if os.path.basename(c) == VER_C else ()
        w = []
        return (s, compile_c(c, s, extra, w), w)
    srcs, warns, cfails = [], [], []
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        for s, err, w in ex.map(comp, sources(".c")):
            warns.extend(w)
            if err:
                cfails.append(err)
            else:
                srcs.append(s)
    if cfails:
        print(f"[libc] {len(cfails)} compile failure(s):")
        for f in cfails[:15]:
            print("  " + f)
        return 1
    asms = sources(".asm")
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

    # 3b. safety: no external name may be exported by two archived modules.
    # The archive namespace is flat and 8 characters wide, and ld370 resolves an
    # autocall against the first object offering the name -- so a duplicate lets
    # link order decide which code runs. #151: @@ERRNO was a function in one
    # object and a DC F'0' in another, with errno.h making every errno a call to
    # that name. Fail closed; there is no safe automatic pick.
    dups, nscan, nskip = dupscan.scan(srcs + asms, startups=STARTUPS)
    if dups:
        dupscan.report(dups, nscan, nskip, tag="libc")
        return 1
    # say so on success too -- a silent gate reads the same as an absent one
    print(f"[libc] no duplicate externals ({nscan} archived, {nskip} skipped)")

    # 4. archive the runtime -> libc.a ; copy crt startfiles
    libc = f"{BUILD}/libc.a"
    r = run([AR370, "rc", libc] + sorted(objs))
    if r.returncode != 0:
        print("[libc] ar370 failed:", r.stderr); return 1
    for name, stem in STARTFILES:
        if stem not in crtobjs:
            print(f"[libc] MISSING startup {stem}"); return 1
        shutil.copy(crtobjs[stem], f"{BUILD}/{name}")
    nmem = sum(1 for l in run([AR370, "t", libc]).stdout.splitlines() if l.strip().endswith("bytes"))
    print(f"[libc] OK -> {libc} ({nmem} members, {os.path.getsize(libc)} bytes, {OPT})")
    print("[libc] startfile -> crtm.o (@@CRT0 is a libc.a member)")
    return 0


def copy_headers(inc):
    """Copy include/ as a tree into inc; -> the number of headers."""
    n = 0
    src_inc = f"{ROOT}/include"
    for d, _, files in os.walk(src_inc):
        for f in sorted(files):
            if not f.endswith(".h"):
                continue
            rel = os.path.relpath(os.path.join(d, f), src_inc)
            os.makedirs(os.path.join(inc, os.path.dirname(rel)), exist_ok=True)
            shutil.copy(os.path.join(d, f), os.path.join(inc, rel)); n += 1
    return n


def stage(inc, lib, mac):
    """Copy headers, libc.a + startfiles and macros into three directories.
    Copies only, never deletes: shared by install (into the sysroot) and by
    sdk/package.py (into a staging tree, #326)."""
    libc = f"{BUILD}/libc.a"
    if not os.path.exists(libc):
        print("[stage] build first (no", libc + ")"); return None
    for d in (inc, lib, mac):
        os.makedirs(d, exist_ok=True)
    n = copy_headers(inc)
    # libc.a + startfiles
    shutil.copy(libc, f"{lib}/libc.a")
    for crt, _ in STARTFILES:
        shutil.copy(f"{BUILD}/{crt}", f"{lib}/{crt}")
    # assembler macros: hasp/ (the JES2 mappings) and maclib/ (libc370's own)
    # -> one dir as370 finds by default (<exedir>/../macros).  The IBM macros
    # are maclib370's, in <sysroot>/maclib370/macros (#494).
    m = 0
    for srcdir in (f"{ROOT}/hasp", f"{ROOT}/maclib"):
        for f in sorted(glob.glob(f"{srcdir}/*")):
            if os.path.isfile(f):
                shutil.copy(f, mac); m += 1
    # the IBM macros earlier releases put into <sysroot>/macros (#494), as
    # <sysroot>/share/libc370/sysmac-removed.txt: whatever installs a
    # sysroot without `make install` (cc370's install.sh, an unpacked
    # tarball) can delete them from there (#498)
    share = os.path.join(os.path.dirname(mac), "share", "libc370")
    os.makedirs(share, exist_ok=True)
    shutil.copy(SYSMAC_REMOVED, share)
    return n, m


def cmd_stage(prefix):
    """Stage <prefix>/{include,lib,macros} - the sysroot's layout, anywhere."""
    if os.path.exists(prefix) and os.listdir(prefix):
        print(f"[stage] {prefix} is not empty"); return 1
    r = stage(f"{prefix}/include", f"{prefix}/lib", f"{prefix}/macros")
    if r is None:
        return 1
    print(f"[stage] {r[0]} headers, libc.a + crtm.o, {r[1]} macro files -> {prefix}")
    return 0


def cmd_install():
    triple, inc, lib = sysroot()
    # macros go in the sysroot beside include/lib; as370's real binary lives in
    # <sysroot>/bin, so its default macro path <exedir>/../macros resolves here.
    mac = os.path.join(os.path.dirname(inc), "macros")  # <sysroot>/macros = <prefix>/cc370/macros
    if not os.path.exists(f"{BUILD}/libc.a"):
        print("[install] build first (no", f"{BUILD}/libc.a" + ")"); return 1
    # headers.  2.0 puts them in subdirectories (mvs/, ibm/mvs/, ...), so the
    # whole tree is copied, not include/*.h (#256).  The sysroot's include/ is
    # cleared first: install used to copy and never delete, so a sysroot that
    # had 1.x installed would keep every flat 1.x header beside the new tree,
    # and <clibwto.h> would still resolve -- from a stale copy, silently.
    # libc370 owns that directory outright: cc370 searches no other include
    # directory, and every file a 1.x install left there came from libc370.
    # The path check keeps a mis-derived sysroot from costing anything else.
    if os.path.basename(inc) != "include" or \
            os.path.basename(os.path.dirname(inc)) != triple:
        print(f"[install] refusing to clear {inc}: not <prefix>/{triple}/include")
        return 1
    stale = 0
    if os.path.isdir(inc):
        stale = sum(len(f) for _, _, f in os.walk(inc))
        shutil.rmtree(inc)
    n, m = stage(inc, lib, mac)
    # crt0.o/crt1.o from an install before 2.4.0: nothing reads them any
    # more (cc370 1.4 names no startfile, mbt finds the sysroot by libc.a),
    # and a stale copy of an old @@CRT0 is only a trap (#159).  Install only:
    # lib comes from the driver's own sysroot, and a staging tree is new.
    for crt in STALE_STARTFILES:
        old = os.path.join(lib, crt)
        if os.path.isfile(old):
            os.remove(old)
            print(f"[install] removed the old startfile {old}")
    # the IBM macros sysmac/ installed up to 2.7.x (#494): as370 searches
    # <sysroot>/macros before <sysroot>/maclib370/macros, so a copy left here
    # would stand in for maclib370's member.  Only these names, only here.
    gone = 0
    for line in open(SYSMAC_REMOVED, encoding="utf-8"):
        name = line.strip()
        if not name or name.startswith("#"):
            continue
        old = os.path.join(mac, name)
        if os.path.isfile(old):
            os.remove(old); gone += 1
    if gone:
        print(f"[install] removed {gone} IBM macro(s) an earlier libc370 left in {mac}")
    print(f"[install] target {triple}")
    print(f"[install] {n} headers -> {inc} ({stale} files there before, cleared)")
    print(f"[install] libc.a + crtm.o -> {lib}")
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
    for c in sources(".c"):
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
    if cmd == "stage":
        if len(sys.argv) != 3:
            sys.exit("usage: python3 sdk/mklibc.py stage DIR")
        sys.exit(cmd_stage(sys.argv[2]))
    if cmd in ("build", "all"):
        rc = cmd_build()
    if rc == 0 and cmd in ("install", "all"):
        rc = cmd_install()
    sys.exit(rc)
