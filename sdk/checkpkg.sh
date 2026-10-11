#!/bin/sh
# checkpkg.sh DIST VERSION - check the artifacts sdk/package.py made (#326).
#
# Every expected value is derived again from sdk/cc370.json and
# sdk/maclib370.json through
# sdk/package.py requires, not copied: the packages are checked against the
# one place each range is written.  Needs dpkg-deb and rpm (CI: Ubuntu).
#
# RC: 0 when every check passed, 1 otherwise; each check prints one line.

D=$1
V=$2
[ -n "$D" ] && [ -n "$V" ] || { echo "usage: checkpkg.sh DIST VERSION"; exit 2; }
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SYS=usr/lib/cc370/cc370
DEB="$D/libc370-dev_${V}_all.deb"
RPM="$D/libc370-devel-${V}.noarch.rpm"
TGZ="$D/libc370-${V}-sysroot.tar.gz"
META="$D/libc370-${V}-metadata.json"
fail=0

check() {   # check "what" "got" "want"
    if [ "$2" = "$3" ]; then
        echo "ok    $1"
    else
        echo "FAIL  $1"
        echo "      got:  $2"
        echo "      want: $3"
        fail=1
    fi
}

want_deb=$(python3 "$ROOT/sdk/package.py" requires deb | paste -sd, - | sed 's/,/, /g')
want_rpm=$(python3 "$ROOT/sdk/package.py" requires rpm | sort | paste -sd'|' -)
want_range=$(python3 "$ROOT/sdk/package.py" requires range)

( cd "$D" && sha256sum -c --quiet SHA256SUMS ) && echo "ok    SHA256SUMS" \
    || { echo "FAIL  SHA256SUMS"; fail=1; }

check "deb Package"      "$(dpkg-deb -f "$DEB" Package)"      "libc370-dev"
check "deb Architecture" "$(dpkg-deb -f "$DEB" Architecture)" "all"
check "deb Version"      "$(dpkg-deb -f "$DEB" Version)"      "$V"
check "deb Depends"      "$(dpkg-deb -f "$DEB" Depends)"      "$want_deb"

check "rpm Name"    "$(rpm -qp --qf '%{NAME}' "$RPM" 2>/dev/null)" "libc370-devel"
check "rpm Arch"    "$(rpm -qp --qf '%{ARCH}' "$RPM" 2>/dev/null)" "noarch"
check "rpm Requires" \
    "$(rpm -qpR "$RPM" 2>/dev/null | grep -E '^(cc370|maclib370) ' | sort | paste -sd'|' -)" \
    "$want_rpm"

check "metadata requires" \
    "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["requires"]["cc370"])' "$META")" \
    "$want_range"
check "metadata requires maclib370" \
    "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["requires"]["maclib370"])' "$META")" \
    "$(python3 "$ROOT/sdk/package.py" requires maclib370-range)"
check "metadata version" \
    "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$META")" \
    "$V"

# the same files in all three, under the sysroot in the packages
t=$(tar tzf "$TGZ" | grep -v '/$' | sort)
d=$(dpkg-deb -c "$DEB" | awk '{print $NF}' | grep -v '/$' \
    | sed "s|^\./$SYS/||" | sort)
# rpm -qlp lists directories without a trailing slash: keep regular files
r=$(rpm -qp --qf '[%{FILEMODES:perms} %{FILENAMES}\n]' "$RPM" 2>/dev/null \
    | awk '/^-/ {print $2}' | grep "^/$SYS/" | sed "s|^/$SYS/||" | sort)
same() {    # same "what" "list" "reference list"
    if [ "$2" = "$3" ]; then
        echo "ok    $1"
    else
        echo "FAIL  $1"
        printf '%s\n' "$2" > /tmp/checkpkg.a
        printf '%s\n' "$3" > /tmp/checkpkg.b
        diff /tmp/checkpkg.a /tmp/checkpkg.b | head -20 | sed 's/^/      /'
        fail=1
    fi
}
same "deb files = tarball files" "$d" "$t"
same "rpm files = tarball files" "$r" "$t"
check "files outside the sysroot (deb)" \
    "$(dpkg-deb -c "$DEB" | awk '{print $NF}' | grep -v '/$' \
       | grep -vc "^\./$SYS/")" "0"
for f in lib/libc.a lib/crtm.o include/stdio.h \
         include/mvs/crt.h 'macros/$pso.macro' macros/clibregs.macro \
         share/libc370/sysmac-removed.txt; do
    check "tarball has $f" "$(echo "$t" | grep -Fcx "$f")" "1"
done
# the IBM macros are maclib370's since #494, never this package's
grep -v '^#' "$ROOT/sdk/sysmac-removed.txt" | sed 's|^|macros/|' > "$D/.ibm"
check "tarball has no IBM macro (sdk/sysmac-removed.txt)" \
    "$(echo "$t" | grep -Fxcf "$D/.ibm")" "0"
rm -f "$D/.ibm"
echo "tarball: $(echo "$t" | wc -l) files"

exit $fail
