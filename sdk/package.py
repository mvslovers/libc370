#!/usr/bin/env python3
"""package -- libc370's release artifacts, one per channel, for every host (#326).

libc370 is MVS target code: headers, libc.a, crt*.o, macros.  It is the same
whatever the host, so there is one artifact per channel, not one per host:

    libc370-<v>-sysroot.tar.gz       include/ lib/ macros/, no top directory:
                                     unpack it INTO a sysroot (<prefix>/cc370)
    libc370-dev_<v>_all.deb          /usr/lib/cc370/cc370/{include,lib,macros}
    libc370-devel-<v>.noarch.rpm     the same tree
    libc370-<v>-metadata.json        version, commit, the cc370 range
    SHA256SUMS                       over all of the above

The cc370 this tree needs is written once, in sdk/cc370.json; every field
below is derived from it (the Debian and RPM dependencies, the metadata, the
release notes line, the __CC370__ number #315 checks).

Usage:  python3 sdk/package.py requires {deb|rpm|range|number|notes}
        python3 sdk/package.py build --version V --built-with "1.0.0 (sha)"
                                     [--out dist] [--no-nfpm]

`build` needs a built tree (sdk/mklibc.py build) and, for .deb/.rpm, nfpm on
PATH (CI installs a pinned release); --no-nfpm writes the tarball, the
metadata and the nfpm.yaml only.
"""
import argparse, gzip, json, os, re, shutil, subprocess, sys, tarfile, hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REQ = os.path.join(ROOT, "sdk", "cc370.json")
SYSROOT = "/usr/lib/cc370/cc370"
# cc370 1.1.0 installs these itself (cc370#688); a package that also ships
# them cannot be installed beside it (#313 removes them from maclib/)
MOVED_TO_CC370 = ("pdptop.copy", "pdpprlg.macro", "pdpepil.macro")

sys.path.insert(0, os.path.join(ROOT, "sdk"))


def vtuple(v):
    m = re.fullmatch(r"(\d+)(?:\.(\d+))?(?:\.(\d+))?", v)
    if not m:
        sys.exit(f"{REQ}: '{v}' is not MAJOR[.MINOR[.PATCH]]")
    return tuple(int(x or 0) for x in m.groups())


def requires():
    r = json.load(open(REQ))
    lo, hi = r["min"], r["below"]
    if vtuple(lo) >= vtuple(hi):
        sys.exit(f"{REQ}: min {lo} is not below {hi}")
    return lo, hi


def derived(kind):
    lo, hi = requires()
    a, b, c = vtuple(lo)
    return {
        "deb": [f"cc370 (>= {lo})", f"cc370 (<< {hi})"],
        "rpm": [f"cc370 >= {lo}", f"cc370 < {hi}"],
        "range": f">={lo} <{hi}",
        # the value cc370 predefines as __CC370__ (cc370#704)
        "number": a * 10000 + b * 100 + c,
        "notes": f"Requires cc370 >= {lo}, < {hi}.",
    }[kind]


def check_header():
    """#315: include/sys/_cc370.h carries sdk/cc370.json's minimum, and every
    public header includes it.  Called from sdk/headermap.py check (CI)."""
    hdr = os.path.join(ROOT, "include", "sys", "_cc370.h")
    text = open(hdr, encoding="latin-1").read()
    bad = 0
    lo, _ = requires()
    m = re.search(r"#define __LIBC370_MIN_CC370 (\d+)", text)
    if not m or int(m.group(1)) != derived("number"):
        print(f"[cc370] {hdr}: __LIBC370_MIN_CC370 is "
              f"{m.group(1) if m else 'missing'}, sdk/cc370.json says "
              f"{derived('number')}"); bad += 1
    if f'"libc370 needs cc370 {lo} or later"' not in text:
        print(f"[cc370] {hdr}: the #error text does not name cc370 {lo}"); bad += 1
    inc = os.path.join(ROOT, "include")
    missing = []
    for d, _, files in os.walk(inc):
        for f in files:
            p = os.path.join(d, f)
            if f.endswith(".h") and p != hdr and \
                    "#include <sys/_cc370.h>" not in open(p, encoding="latin-1").read():
                missing.append(os.path.relpath(p, inc))
    if missing:
        print(f"[cc370] not including <sys/_cc370.h>: {', '.join(sorted(missing))}")
        bad += 1
    print(f"[cc370] minimum {lo} = {derived('number')}"
          + (f"; {bad} problem(s)" if bad else "; every header checks it"))
    return bad


def cmd_requires(kind):
    v = derived(kind)
    print("\n".join(v) if isinstance(v, list) else v)
    return 0


def gitrev():
    try:
        return subprocess.check_output(["git", "-C", ROOT, "rev-parse", "HEAD"],
                                       text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def tarball(stage, path):
    """Reproducible: sorted, fixed owner, mtime and mode, no top directory."""
    def norm(ti):
        ti.uid = ti.gid = 0
        ti.uname = ti.gname = "root"
        ti.mtime = 0
        ti.mode = 0o755 if ti.isdir() else 0o644
        return ti
    # gzip writes a timestamp into its header: fix it too
    with open(path, "wb") as raw, \
            gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz, \
            tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as t:
        for top in ("include", "lib", "macros"):
            base = os.path.join(stage, top)
            entries = [base]
            for d, dirs, files in os.walk(base):
                dirs.sort()
                entries += [os.path.join(d, x) for x in dirs]
                entries += [os.path.join(d, x) for x in sorted(files)]
            for e in entries:
                t.add(e, arcname=os.path.relpath(e, stage), recursive=False,
                      filter=norm)


def nfpm_yaml(stage, version, name):
    deb, rpm = derived("deb"), derived("rpm")
    lines = [
        "# rendered by sdk/package.py from sdk/cc370.json - do not edit",
        f"name: {name}",
        "arch: all",
        "platform: linux",
        f"version: {version}",
        "version_schema: none",
        "maintainer: mvslovers <https://github.com/mvslovers>",
        "description: |",
        "  C runtime library for MVS 3.8j, the cc370 target library:",
        "  headers, libc.a, startup objects and assembler macros.",
        "homepage: https://github.com/mvslovers/libc370",
        "license: BSD-2-Clause",
        "section: devel",
        "contents:",
    ]
    for top in ("include", "lib", "macros"):
        lines += [f"  - src: {stage}/{top}/",
                  f"    dst: {SYSROOT}/{top}",
                  "    type: tree"]
    lines += ["overrides:", "  deb:", "    depends:"]
    lines += [f"      - {d}" for d in deb]
    lines += ["  rpm:", "    depends:"]
    lines += [f"      - {d}" for d in rpm]
    lines += ["rpm:", "  packager: mvslovers"]
    return "\n".join(lines) + "\n"


def cmd_build(a):
    import mklibc
    out = os.path.abspath(a.out)
    stage = os.path.join(out, "stage")
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)
    if mklibc.cmd_stage(stage) != 0:
        return 1

    lo, _ = requires()
    if vtuple(lo) >= (1, 1, 0):
        clash = [f for f in MOVED_TO_CC370
                 if os.path.exists(os.path.join(stage, "macros", f))]
        if clash:
            print(f"[package] cc370 >= {lo} ships {', '.join(clash)} itself "
                  f"(cc370#688); the package cannot install beside it - #313 "
                  f"removes them from maclib/")
            return 1

    v = a.version
    tgz = os.path.join(out, f"libc370-{v}-sysroot.tar.gz")
    tarball(stage, tgz)

    m = re.match(r"(\S+)(?:\s+\((\w+)\))?", a.built_with.strip())
    meta = {
        "schema": 1,
        "name": "libc370",
        "version": v,
        "commit": gitrev(),
        "built_with": {"cc370": m.group(1) if m else a.built_with},
        "requires": {"cc370": derived("range")},
    }
    if m and m.group(2):
        meta["built_with"]["cc370_commit"] = m.group(2)
    mj = os.path.join(out, f"libc370-{v}-metadata.json")
    with open(mj, "w") as f:
        json.dump(meta, f, indent=2)
        f.write("\n")

    # one config per format: the package names differ, and cc370's packages
    # depend on exactly these (cc370#523)
    files = [tgz, mj]
    for packager, name, target in (
            ("deb", "libc370-dev", f"libc370-dev_{v}_all.deb"),
            ("rpm", "libc370-devel", f"libc370-devel-{v}.noarch.rpm")):
        cfg = os.path.join(out, f"nfpm-{packager}.yaml")
        with open(cfg, "w") as f:
            f.write(nfpm_yaml(stage, v, name))
        if a.no_nfpm:
            continue
        path = os.path.join(out, target)
        subprocess.check_call(["nfpm", "package", "--config", cfg,
                               "--packager", packager, "--target", path])
        files.append(path)

    with open(os.path.join(out, "SHA256SUMS"), "w") as f:
        for p in files:
            f.write(f"{sha256(p)}  {os.path.basename(p)}\n")
    for p in files + [os.path.join(out, "SHA256SUMS")]:
        print(f"[package] {os.path.relpath(p, ROOT)}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("requires")
    r.add_argument("kind", choices=("deb", "rpm", "range", "number", "notes"))
    b = sub.add_parser("build")
    b.add_argument("--version", required=True)
    b.add_argument("--built-with", required=True,
                   help="the first line of `cc370 --version`, without 'cc370 '")
    b.add_argument("--out", default=os.path.join(ROOT, "dist"))
    b.add_argument("--no-nfpm", action="store_true")
    a = ap.parse_args()
    if a.cmd == "requires":
        return cmd_requires(a.kind)
    return cmd_build(a)


if __name__ == "__main__":
    sys.exit(main())
