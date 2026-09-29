/*
 * tstwchar.c - libc370 #195 / #213 on MVS: wchar_t is cc370's type, the
 * multibyte conversions work with it, and ptrdiff_t prints through %td.
 *
 * test/host/tstwchar.c checks the header wiring and the five functions'
 * semantics with the host compiler.  What only cc370 on MVS can answer:
 *
 *   (1) cc370's own types: wchar_t is 4 bytes and the type of L"x"[0],
 *       wint_t is 4 bytes, ptrdiff_t is the type of p - q
 *   (2) a wide literal round-trips through wcstombs() to the same EBCDIC
 *       bytes as the narrow literal, and back through mbstowcs()
 *   (3) mbtowc / mblen / wctomb on the S/370 code
 *   (4) %td of a ptrdiff_t prints (#211's parser, #213's type)
 *
 * Printed only, not checked: L'a' against 'a'.  cc370 builds a wide
 * CHARACTER constant in the host charset (97) while the narrow one and the
 * elements of a wide STRING are EBCDIC (129) - a compiler defect, not
 * libc370's; see the cc370 issue referenced in the PR.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstwchar.c -o TSTWCH -flinker-output=iebcopy
 *          cc370 -O1 -Iinclude test/mvs/tstwchar.c \
 *                -o TSTWCR -flinker-output=iebcopy      (red: sysroot libc)
 *          ld370 --pack TSTWCH=TSTWCH.iebcopy TSTWCR=TSTWCR.iebcopy \
 *                -o tstwchar -xmit --dsn IBMUSER.LIBC370.WCHSCR
 * Install: jcl/recvwch.jcl (its own staging data set, not the mbt one).
 * Run:     jcl/tstwchar.jcl.
 *
 * GREEN: mvsdev JOB00649 step GREEN, CC 0000, 18/18, 2026-09-29.
 * RED, same job, step RED: the same source and headers linked against the
 * sysroot libc from 8928b2a (before #195), CC 0001, 11 of 18 failed - all
 * of (2) and all of (3) but wctomb of a byte value.  The types in (1) and
 * %td in (4) pass there too: they are header and parser, not library.
 * Printed by both: L'a' = 97, 'a' = 129, L"a"[0] = 129.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define CHECK_EQ(got, want, msg)                                           \
    do {                                                                   \
        long g_ = (long)(got), w_ = (long)(want);                          \
        mbt_run++;                                                         \
        if (g_ == w_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }     \
        else { mbt_failed++;                                               \
               printf("  FAIL: %s (got %ld, want %ld)\n", (msg), g_, w_); } \
    } while (0)

#if WCHAR_MAX != INT_MAX || WCHAR_MIN != INT_MIN \
    || WINT_MAX != UINT_MAX || WINT_MIN != 0 || PTRDIFF_MAX != LONG_MAX
#error "limits do not match cc370's types"
#endif

int main(void)
{
    const wchar_t *lit = L"Hello";
    char     a[8];
    char     b[16];
    wchar_t  w[16];
    wchar_t  wc;
    size_t   r;
    ptrdiff_t d;

    printf("=== tstwchar: wchar_t, wint_t, ptrdiff_t on MVS (#195, #213) ===\n\n");

    printf("(1) cc370's types:\n");
    CHECK_EQ(sizeof(wchar_t), 4, "(1) sizeof(wchar_t)");
    CHECK_EQ(sizeof(L"x"[0]), 4, "(1) sizeof(L\"x\"[0])");
    CHECK(__builtin_types_compatible_p(wchar_t, __typeof__(L"x"[0])),
          "(1) wchar_t is the type of a wide literal's element");
    CHECK_EQ(sizeof(wint_t), 4, "(1) sizeof(wint_t)");
    CHECK(__builtin_types_compatible_p(ptrdiff_t, __typeof__(&a[1] - &a[0])),
          "(1) ptrdiff_t is the type of p - q");

    printf("\n(2) wide literal <-> EBCDIC bytes:\n");
    memset(b, 'x', sizeof(b));
    r = wcstombs(b, lit, sizeof(b));
    CHECK_EQ(r, 5, "(2) wcstombs(L\"Hello\") stores five bytes");
    CHECK(strcmp(b, "Hello") == 0, "(2) ... the same bytes as \"Hello\"");
    CHECK_EQ((unsigned char)b[0], 0xC8, "(2) 'H' is EBCDIC X'C8'");
    r = mbstowcs(w, "Hello", 16);
    CHECK(r == 5 && memcmp(w, lit, 6 * sizeof(wchar_t)) == 0,
          "(2) mbstowcs(\"Hello\") equals L\"Hello\", terminator included");
    CHECK(wcstombs(b, L"\x100", sizeof(b)) == (size_t)-1,
          "(2) a value above a byte is refused");

    printf("\n(3) mbtowc, mblen, wctomb:\n");
    CHECK_EQ(mbtowc(&wc, "abc", 3), 1, "(3) mbtowc with n = 3 converts one");
    CHECK_EQ(wc, 0x81, "(3) ... 'a' is X'81'");
    CHECK_EQ(mbtowc(&wc, "", 1), 0, "(3) mbtowc of the null character is 0");
    CHECK_EQ(mblen("abc", 3), 1, "(3) mblen with n = 3 is 1");
    CHECK_EQ(mblen("", 1), 0, "(3) mblen of the null character is 0");
    CHECK_EQ(wctomb(b, (wchar_t)0x100), -1, "(3) wctomb of a value above a byte");
    CHECK(wctomb(b, lit[0]) == 1 && b[0] == 'H', "(3) wctomb(L\"H\"[0]) is 'H'");

    printf("\n(4) %%td:\n");
    d = &a[2] - &a[7];
    snprintf(b, sizeof(b), "%td|%d", d, 7);
    CHECK(strcmp(b, "-5|7") == 0, "(4) %td of p - q, then %d");

    printf("\nprinted only - cc370's wide character constant:\n");
    printf("  L'a' = %d, 'a' = %d, L\"a\"[0] = %d\n",
           (int)L'a', (int)'a', (int)L"a"[0]);

    printf("\n=== tstwchar: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
