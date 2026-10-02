#!/bin/sh
# test/host/run.sh - build and run the host tests, one line of output each.
#
# Every test documents its own build recipe in its header comment; this file
# is those recipes, written out once so that CI and a developer run the same
# thing.  Each recipe is copied from the test's header comment, with two
# changes only: paths are relative to test/host, and the binary goes into a
# temporary directory instead of ./t or /tmp.  If a recipe changes in a test's
# header, change it here too.
#
# Flags are written out on every line, never through a shell variable: zsh
# does not word-split an unquoted $VAR, so a pasted line would hand the
# compiler one argument and it would usually carry on.
#
# Usage:  test/host/run.sh              run every test listed below
#         test/host/run.sh tstvsnp ...  run only those
# RC:     0 when every test that ran passed, 1 otherwise.
#
# The compiler is $CC, default cc.  Run on macOS: the libc370 headers name
# symbols with asm("@@ARCOU") labels, and on Linux those cannot link.  GNU as
# rejects the '@' under gcc; clang quotes it, but ELF then reads "@@" as the
# default-version separator, every such name becomes an empty symbol, and
# they collide ("multiple definition of `no symbol'").  Mach-O has no symbol
# versioning.  Measured in CI on 2026-09-30: 8 of 21 tests fail on Linux gcc,
# 7 on Linux clang, none on macOS.  CI runs this on a macOS runner.
# AddressSanitizer is load-bearing for the tests that ask for it.

CC=${CC:-cc}
cd "$(dirname "$0")" || exit 2
R=../..
B=$(mktemp -d) || exit 2
trap 'rm -rf "$B"' EXIT

tst75snd() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address -Wno-trigraphs \
       -D'__asm__(...)=' -D'asm(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tst75snd.c && "$B/t"
}
tstcmtt() {
    "$CC" -std=gnu99 -Wall -Wextra -o "$B/t" tstcmtt.c && "$B/t"
}
tstblank() {
    "$CC" -std=gnu99 -Wall -Wextra -Werror -o "$B/t" tstblank.c && "$B/t"
}
tstcnvdi() {
    "$CC" -std=gnu99 -Wall -Wextra -Werror -O1 -o "$B/t" tstcnvdi.c && "$B/t"
}
tstdblcv() {
    "$CC" -std=gnu99 -Wall -Wextra -Werror -O1 -o "$B/t" tstdblcv.c && "$B/t"
}
tstdblrb() {
    "$CC" -std=gnu99 -Wall -Wextra -Werror -O1 -fsanitize=address \
       -o "$B/t" tstdblrb.c && "$B/t"
}
tstdi3() {
    "$CC" -std=gnu99 -Wall -Wextra -Werror -O1 -o "$B/t" tstdi3.c && "$B/t"
}
tstdirck() {
    "$CC" -std=gnu99 -Wall \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstdirck.c && "$B/t"
}
tstemptl() {
    "$CC" -std=gnu99 -Wall \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstemptl.c && "$B/t"
}
tstenqdq() {
    "$CC" -std=gnu99 -Wall -Wextra -U__LP64__ -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstenqdq.c && "$B/t"
}
tsterrfl() {
    "$CC" -std=gnu99 -Wall \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tsterrfl.c && "$B/t"
}
tstfabnd() {
    "$CC" -std=gnu99 -Wall -Wextra \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstfabnd.c \
       "$R/src/ext/array/@@aradd.c" "$R/src/ext/array/@@arnew.c" \
       "$R/src/ext/array/@@arcou.c" "$R/src/ext/array/@@ardel.c" \
       "$R/src/ext/array/@@arfre.c" "$R/src/ext/array/@@arget.c" && "$B/t"
}
tstfcls() {
    "$CC" -std=gnu99 -Wall -Wextra \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstfcls.c \
       "$R/src/ext/array/@@aradd.c" "$R/src/ext/array/@@arnew.c" \
       "$R/src/ext/array/@@arcou.c" "$R/src/ext/array/@@ardel.c" \
       "$R/src/ext/array/@@arfre.c" "$R/src/ext/array/@@arget.c" && "$B/t"
}
tstfpapp() {
    "$CC" -std=gnu99 -Wall \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstfpapp.c && "$B/t"
}
tstfprls() {
    "$CC" -std=gnu99 -Wall -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
       -D'__asm__(...)=' -D'asm(x)=' -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstfprls.c && "$B/t"
}
tstfpunit() {
    "$CC" -std=gnu99 -Wall -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
       -D'__asm__(...)=' -D'asm(x)=' -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstfpunit.c && "$B/t"
}
tstidcam() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address -Wno-unknown-pragmas \
       -D'__asm__(...)=' -D'__asm(...)=' -D'asm(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstidcam.c && "$B/t"
}
tstinet() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address -Wno-trigraphs \
       -D'__asm__(...)=' -D'asm(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstinet.c \
       "$R/src/net/@@inaton.c" "$R/src/net/@@inaddr.c" "$R/src/net/@@inpton.c" \
       "$R/src/net/@@inntop.c" "$R/src/net/@@inntoa.c" && "$B/t"
}
tstiolk() {
    "$CC" -std=gnu99 -Wall -Wextra \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstiolk.c && "$B/t"
}
tstjesop() {
    "$CC" -std=gnu99 -U__LP64__ -D'__asm__(x)=' -D__32BIT__ \
       -Dcalloc=tst_calloc -I $R/include -I $R \
       -c "$R/src/mvs/jes2/jesopen.c" -o "$B/jesopen.o" &&
    "$CC" -std=gnu99 -U__LP64__ -D'__asm__(x)=' -D__32BIT__ \
       -Dcalloc=tst_calloc -I $R/include -I $R \
       -c "$R/src/ext/array/@@arnew.c" -o "$B/arnew.o" &&
    "$CC" -std=gnu99 -U__LP64__ -D'__asm__(x)=' -D__32BIT__ \
       -Dfree=tst_free -I $R/include -I $R \
       -c "$R/src/mvs/jes2/jesjobf1.c" -o "$B/jesjobf1.o" &&
    "$CC" -std=gnu99 -U__LP64__ -Wall -Wextra -fsanitize=address \
       -D'__asm__(x)=' -D__32BIT__ -I $R/include -I $R \
       -o "$B/t" tstjesop.c "$B/jesopen.o" "$B/arnew.o" "$B/jesjobf1.o" \
       "$R/src/mvs/jes2/jesclose.c" "$R/src/mvs/jes2/jesjobfr.c" \
       "$R/src/ext/array/@@aradd.c" "$R/src/ext/array/@@arcou.c" \
       "$R/src/ext/array/@@arget.c" "$R/src/ext/array/@@arfre.c" && "$B/t"
}
tstjesprb() {
    "$CC" -std=gnu99 -Wall -Wextra -I $R \
       -o "$B/t" tstjesprb.c $R/src/mvs/jes2/jesprb.c && "$B/t"
}
tstjestx() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstjestx.c \
       "$R/src/mvs/jes2/jesprb.c" \
       "$R/src/ext/array/@@aradd.c" "$R/src/ext/array/@@arnew.c" \
       "$R/src/ext/array/@@arcou.c" "$R/src/ext/array/@@arget.c" \
       "$R/src/ext/array/@@arfre.c" && "$B/t"
}
tstlstds() {
    "$CC" -std=gnu99 -Wall -fsanitize=address \
       -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstlstds.c "$R/src/ext/strutil/@@patmat.c" && "$B/t"
}
tstscnll() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address \
       -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstscnll.c && "$B/t"
}
tstll() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address \
       -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -o "$B/t" tstll.c && "$B/t"
}
tstlspd() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address \
       -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstlspd.c "$R/src/ext/strutil/@@patmat.c" && "$B/t"
}
tstplus() {
    "$CC" -std=gnu99 -Wall \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstplus.c && "$B/t"
}
tstrldwk() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address \
       -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstrldwk.c && "$B/t"
}
tsttm64vec() {
    "$CC" -std=gnu99 -Wall -Wextra -o "$B/t" tsttm64vec.c && "$B/t"
}
tsttxdsn() {
    "$CC" -std=gnu99 -D'__asm__(x)=' -D__32BIT__ -Dcalloc=tst_calloc \
       -I $R/include -I $R -c "$R/src/mvs/dynalloc/@@nwtx99.c" -o "$B/nwtx99.o" &&
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address \
       -D'__asm__(x)=' -D__32BIT__ -I $R/include -I $R -o "$B/t" tsttxdsn.c \
       "$B/nwtx99.o" \
       "$R/src/mvs/dynalloc/@@txdsn.c" "$R/src/ext/array/@@aradd.c" \
       "$R/src/ext/array/@@arnew.c" "$R/src/ext/array/@@arcou.c" \
       "$R/src/ext/array/@@arget.c" "$R/src/ext/array/@@arfre.c" && "$B/t"
}
tstvsnp() {
    "$CC" -std=gnu99 -Wall -Wextra -fsanitize=address \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstvsnp.c && "$B/t"
}
tstwchar() {
    "$CC" -std=gnu99 -Wall -Wextra -Werror=incompatible-pointer-types \
       -fsanitize=address -U__LP64__ -D'__asm__(...)=' \
       -D__volatile__= -D__32BIT__ -I $R/include -I $R -o "$B/t" tstwchar.c \
       && "$B/t"
}
tstwpos() {
    "$CC" -std=gnu99 -Wall \
       -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
       -I $R/include -I $R -o "$B/t" tstwpos.c && "$B/t"
}

# The tests a plain run covers: all of them.  A test that stops building is
# not dropped from here - fix it, or say which issue it waits on.
ALL="tst75snd tstblank tstcmtt tstcnvdi tstdblcv tstdblrb tstdi3 tstdirck tstemptl
     tstenqdq tsterrfl tstfabnd tstfcls tstfpapp tstfprls tstfpunit tstidcam tstinet tstiolk tstjesop
     tstjesprb tstjestx tstll tstlspd tstlstds tstplus tstrldwk tstscnll tsttm64vec tsttxdsn tstvsnp
     tstwchar tstwpos"

[ $# -gt 0 ] && ALL="$*"

pass=0
fail=0
failed=
for t in $ALL; do
    printf '%-12s ' "$t"
    if ( $t ) >"$B/$t.log" 2>&1; then
        echo ok
        pass=$((pass + 1))
    else
        echo "FAIL (rc $?)"
        sed 's/^/    | /' "$B/$t.log" | tail -30
        fail=$((fail + 1))
        failed="$failed $t"
    fi
    rm -f "$B/t" "$B"/*.o
done

echo "host tests: $pass passed, $fail failed${failed:+ -$failed}"
[ $fail -eq 0 ]
