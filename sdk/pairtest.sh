#!/bin/sh
# pairtest.sh deb|rpm DIR - install cc370 and libc370 as packages, together,
# under the real package manager, and build a program with the pair (#326).
#
# Runs as root inside a clean container: debian:bookworm for deb,
# fedora for rpm.  DIR holds cc370's package for this machine and
# libc370's (libc370-dev_*_all.deb / libc370-devel-*.noarch.rpm).
#
# Checks, in order:
#   1. control: libc370 alone is refused - it needs cc370
#   2. control: cc370 beside a libc370 2.0.0 (a stand-in package that owns
#      the prologue macro files, as 2.0.x did) is refused
#   3. the pair installs; the macro files belong to the right package
#   4. cc370 predefines __CC370__ as its own version's number
#   5. a printf("%lld")/strtoll program compiles and links, and the load
#      map shows the 64-bit helpers from libcc370rt.a, strtoll from libc.a
#
# RC: 0 when every check passed, 1 otherwise; each check prints one line.

MODE=$1
DIR=$2
[ -n "$MODE" ] && [ -d "$DIR" ] || { echo "usage: pairtest.sh deb|rpm DIR"; exit 2; }
W=$(mktemp -d)
fail=0
ok()  { echo "ok    $1"; }
bad() { echo "FAIL  $1"; fail=1; }

case $MODE in
deb)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq >/dev/null
    CC=$(ls "$DIR"/cc370_*.deb)
    LC=$(ls "$DIR"/libc370-dev_*_all.deb)
    install()   { apt-get install -y -q --no-install-recommends "$@"; }
    remove()    { apt-get remove -y -q "$@" >/dev/null 2>&1; }
    owner()     { dpkg -S "$1" 2>/dev/null | cut -d: -f1; }
    # a libc370-dev 2.0.0 stand-in: owns pdpprlg.macro as 2.0.x did
    mkdir -p "$W/old/DEBIAN" "$W/old/usr/lib/cc370/cc370/macros"
    printf 'Package: libc370-dev\nVersion: 2.0.0\nArchitecture: all\nMaintainer: test\nDescription: stand-in for libc370 2.0.0\n' \
        > "$W/old/DEBIAN/control"
    echo '* stand-in' > "$W/old/usr/lib/cc370/cc370/macros/pdpprlg.macro"
    dpkg-deb --build "$W/old" "$W/libc370-dev_2.0.0_all.deb" >/dev/null
    OLD=$W/libc370-dev_2.0.0_all.deb
    ;;
rpm)
    CC=$(ls "$DIR"/cc370-*.rpm | grep -v noarch)
    LC=$(ls "$DIR"/libc370-devel-*.noarch.rpm)
    install()   { dnf install -y -q "$@"; }
    remove()    { dnf remove -y -q "$@" >/dev/null 2>&1; }
    owner()     { rpm -qf --qf '%{NAME}\n' "$1" 2>/dev/null; }
    dnf install -y -q rpm-build >/dev/null
    mkdir -p "$W/rpm/SPECS"
    cat > "$W/rpm/SPECS/old.spec" <<'SPEC'
Name: libc370-devel
Version: 2.0.0
Release: 1
Summary: stand-in for libc370 2.0.0
License: BSD-2-Clause
BuildArch: noarch
%description
stand-in
%install
mkdir -p %{buildroot}/usr/lib/cc370/cc370/macros
echo '* stand-in' > %{buildroot}/usr/lib/cc370/cc370/macros/pdpprlg.macro
%files
/usr/lib/cc370/cc370/macros/pdpprlg.macro
SPEC
    rpmbuild -bb --define "_topdir $W/rpm" "$W/rpm/SPECS/old.spec" >/dev/null 2>&1
    OLD=$(ls "$W"/rpm/RPMS/noarch/libc370-devel-2.0.0-*.rpm)
    ;;
*) echo "unknown mode $MODE"; exit 2 ;;
esac
echo "cc370:   $(basename "$CC")"
echo "libc370: $(basename "$LC")"

# 1. libc370 alone
if install "$LC" > "$W/log1" 2>&1; then
    bad "libc370 alone was accepted"; remove libc370-dev libc370-devel
else
    grep -q cc370 "$W/log1" && ok "libc370 alone is refused (needs cc370)" \
        || { bad "libc370 alone failed, but not over cc370"; tail -5 "$W/log1"; }
fi

# 2. cc370 with a libc370 2.0.0
if [ -n "${OLD:-}" ] && [ -f "$OLD" ]; then
    if install "$CC" "$OLD" > "$W/log2" 2>&1; then
        bad "cc370 beside libc370 2.0.0 was accepted"; remove cc370 libc370-dev libc370-devel
    else
        ok "cc370 beside libc370 2.0.0 is refused"
    fi
else
    bad "no libc370 2.0.0 stand-in was built"
fi

# 3. the pair
if install "$CC" "$LC" > "$W/log3" 2>&1; then
    ok "the pair installs"
else
    bad "the pair does not install"; tail -20 "$W/log3"; exit 1
fi
M=/usr/lib/cc370/cc370/macros
[ "$(owner $M/pdpprlg.macro)" = cc370 ] && ok "pdpprlg.macro belongs to cc370" \
    || bad "pdpprlg.macro belongs to '$(owner $M/pdpprlg.macro)'"
o=$(owner $M/getmain.macro)
case $o in libc370-dev|libc370-devel) ok "getmain.macro belongs to $o" ;;
    *) bad "getmain.macro belongs to '$o'" ;; esac

# 4. __CC370__
v=$(cc370 --version | head -1 | sed 's/^cc370 \([0-9.]*\).*/\1/')
want=$(echo "$v" | awk -F. '{ print $1 * 10000 + $2 * 100 + $3 }')
got=$(echo | cc370 -dM -E - | sed -n 's/^#define __CC370__ //p')
[ "$got" = "$want" ] && ok "__CC370__ is $got (cc370 $v)" \
    || bad "__CC370__ is '$got', cc370 $v means $want"

# 5. a program
cat > "$W/hello.c" <<'C'
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    long long v = strtoll("-123456789012", NULL, 10);
    printf("%lld\n", v * 3);
    return 0;
}
C
if (cd "$W" && cc370 -O1 -Wall -Werror hello.c -o HELLO -Wl,--map,hello.map) \
        > "$W/log5" 2>&1; then
    ok "printf/strtoll program compiles and links"
else
    bad "printf/strtoll program does not build"; cat "$W/log5"
fi
from() {    # from NAME - the source of the section that defines NAME
    awk -v n="$1" '$1 == "PC" || $1 == "SD" { src = $4 }
                   $1 == n { print src; exit }' "$W/hello.map"
}
for h in @@MULDI3 STRTOLL; do
    s=$(from $h)
    case $h:$s in
        @@MULDI3:*libcc370rt.a*) ok "$h from $(basename "$s")" ;;
        STRTOLL:*libc.a*)        ok "$h from $(basename "$s")" ;;
        *) bad "$h from '$s'" ;;
    esac
done

exit $fail
