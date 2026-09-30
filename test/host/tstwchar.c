/*
 * tstwchar.c - libc370 #195 / #213: wchar_t, wint_t and ptrdiff_t are the
 * compiler's types, and the multibyte conversions work with them.
 *
 * ISSUE #195: <stddef.h> and <stdlib.h> said `typedef char wchar_t`, while
 * cc370 builds L"..." from int-wide elements, so `wchar_t *p = L"abc"` was
 * an incompatible-pointer error under -Werror, and wint_t, WEOF, WCHAR_MIN/
 * MAX and WINT_MIN/MAX did not exist.  mbstowcs() and wcstombs() were a
 * strncpy through a cast, correct only while wchar_t was one byte, and
 * mblen()/mbtowc() answered -1 for any n > 1 and 1 for the null character.
 *
 * ISSUE #213: ptrdiff_t was int where the compiler's is long, so %td of it
 * warned under -Wall.
 *
 * What this pins:
 *
 *   1. the types: wchar_t is the type of L"x"[0], ptrdiff_t the type of
 *      p - q, and wint_t holds every wchar_t plus WEOF
 *   2. the limits are usable in #if and match the types
 *   3. mbstowcs: widens, stops at the terminator, honours n, and widens a
 *      byte above X'7F' to a positive value
 *   4. wcstombs: narrows, honours n, refuses a value that does not fit a
 *      byte, and measures with a NULL destination
 *   5. mbtowc / mblen / wctomb: C99 7.20.7 for n > 1, the null character,
 *      a NULL string and n == 0
 *
 * The host compiler supplies __WCHAR_TYPE__, __WINT_TYPE__ and
 * __PTRDIFF_TYPE__ just as cc370 does, so (1) checks the headers' wiring,
 * not cc370's values - test/mvs/tstwchar.c checks those on MVS.
 *
 * BUILD / RUN (host, from the repository root):
 *
 *     cc -std=gnu99 -Wall -Wextra -Werror=incompatible-pointer-types \
 *        -fsanitize=address -U__LP64__ -D'__asm__(...)=' \
 *        -D__volatile__= -D__32BIT__ -I include -o t test/host/tstwchar.c \
 *        && ./t
 *
 * Only the incompatible-pointer warning is an error: plain -Werror also
 * stops on libc370's own snprintf/vsnprintf declarations, which the host
 * compiler knows with a different signature.
 *
 * RED, 2026-09-29, against main 62c222b:
 *   - the old headers plus this wchar.h do not compile: "incompatible
 *     pointer types initializing 'const wchar_t *' (aka 'const char *')
 *     with an expression of type 'int[4]'" - the #195 line itself, and
 *     every L"..." handed to wcstombs().
 *   - the new headers with the old five functions (strncpy declared for
 *     them): 13 of 31 fail - mbstowcs/wcstombs copy bytes into and out of
 *     int elements, mblen/mbtowc answer -1 for n = 3 and 1 for "", and
 *     wctomb stores 0x100 as a byte.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
/* string.h is suppressed and the three string functions used here are
** declared instead: its memset() is inline S/370 assembler that the host
** cannot build (see test/host/tstfprls.c, note a). */
#define STRING_H
#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned long size_t;
#endif
void   *memset(void *, int, size_t);
size_t  strlen(const char *);
int     strcmp(const char *, const char *);

#include <stdio.h>
#include <limits.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>

#include "../../src/clib/mblen.c"
#include "../../src/clib/mbtowc.c"
#include "../../src/clib/wctomb.c"
#include "../../src/clib/mbstowcs.c"
#include "../../src/clib/wcstombs.c"

/* ---- harness ---------------------------------------------------------- */
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

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* (2): the limits must be preprocessor constants */
#if !defined(WCHAR_MAX) || !defined(WCHAR_MIN) \
    || !defined(WINT_MAX) || !defined(WINT_MIN) || !defined(WEOF)
#error "WCHAR_MIN/MAX, WINT_MIN/MAX or WEOF missing"
#endif
#if WCHAR_MAX != 2147483647 || WCHAR_MIN >= 0 || WINT_MIN != 0
#error "WCHAR_MAX/WCHAR_MIN/WINT_MIN wrong in #if"
#endif

int main(void)
{
    const wchar_t *lit = L"abc";        /* the #195 line: -Werror */
    char     a[4];
    wchar_t  w[8];
    char     b[8];
    wchar_t  wc;
    size_t   r;

    printf("=== tstwchar: wchar_t, wint_t, ptrdiff_t and the multibyte "
           "conversions (#195, #213) ===\n\n");

    printf("(1) the types are the compiler's:\n");
    CHECK(__builtin_types_compatible_p(wchar_t, __typeof__(L"x"[0])),
          "(1) wchar_t is the type of a wide literal's element");
    CHECK(__builtin_types_compatible_p(ptrdiff_t, __typeof__(&a[1] - &a[0])),
          "(1) ptrdiff_t is the type of p - q (#213)");
    CHECK(sizeof(wint_t) >= sizeof(wchar_t), "(1) wint_t is as wide as wchar_t");
    CHECK(lit[1] == L'b', "(1) L\"abc\"[1] reads through a wchar_t pointer");

    printf("\n(2) the limits match the types:\n");
    CHECK_EQ(WCHAR_MAX, INT_MAX, "(2) WCHAR_MAX");
    CHECK((wint_t)WEOF == (wint_t)-1, "(2) WEOF is (wint_t)-1");
    /* C99 7.24.1: WEOF is no member of the character set.  With the
    ** single-byte encoding that is every byte mbtowc() can produce.  Not
    ** "WEOF > WCHAR_MAX": on a host whose wint_t is int, WEOF is -1. */
    for (r = 0; r <= UCHAR_MAX; r++) {
        if ((wint_t)WEOF == (wint_t)r) break;
    }
    CHECK(r > UCHAR_MAX, "(2) WEOF is no character mbtowc can produce");

    printf("\n(3) mbstowcs:\n");
    for (r = 0; r < 8; r++) w[r] = (wchar_t)0x7777;
    r = mbstowcs(w, "AB", 8);
    CHECK_EQ(r, 2, "(3) returns the count without the terminator");
    CHECK(w[0] == L'A' && w[1] == L'B' && w[2] == 0, "(3) widened and terminated");
    CHECK_EQ(w[3], 0x7777, "(3) nothing written past the terminator");
    for (r = 0; r < 8; r++) w[r] = (wchar_t)0x7777;
    r = mbstowcs(w, "ABC", 2);
    CHECK_EQ(r, 2, "(3) n = 2 of three stores two");
    CHECK_EQ(w[2], 0x7777, "(3) n is counted in wide characters");
    r = mbstowcs(w, "\xE9", 8);
    CHECK_EQ(w[0], 0xE9, "(3) a byte above X'7F' widens to a positive value");
    CHECK_EQ(mbstowcs(NULL, "abcd", 0), 4, "(3) NULL destination measures");

    printf("\n(4) wcstombs:\n");
    memset(b, 'x', sizeof(b));
    r = wcstombs(b, L"AB", sizeof(b));
    CHECK_EQ(r, 2, "(4) returns the count without the terminator");
    CHECK(strcmp(b, "AB") == 0, "(4) narrowed and terminated");
    memset(b, 'x', sizeof(b));
    r = wcstombs(b, L"ABC", 2);
    CHECK(r == 2 && b[0] == 'A' && b[1] == 'B' && b[2] == 'x',
          "(4) n = 2 stores two bytes and no terminator");
    w[0] = L'A'; w[1] = 0x100; w[2] = 0;
    CHECK(wcstombs(b, w, sizeof(b)) == (size_t)-1,
          "(4) a value above a byte is refused");
    CHECK_EQ(wcstombs(NULL, L"abcd", 0), 4, "(4) NULL destination measures");

    printf("\n(5) mbtowc, mblen, wctomb:\n");
    wc = 0x7777;
    CHECK_EQ(mbtowc(&wc, "abc", 3), 1, "(5) mbtowc with n = 3 converts one");
    CHECK_EQ(wc, L'a', "(5) mbtowc stored the character");
    CHECK_EQ(mbtowc(&wc, "", 1), 0, "(5) mbtowc of the null character is 0");
    CHECK_EQ(wc, 0, "(5) ... and stores 0");
    CHECK_EQ(mbtowc(&wc, "a", 0), -1, "(5) mbtowc with n = 0 is -1");
    CHECK_EQ(mbtowc(NULL, NULL, 0), 0, "(5) mbtowc(NULL string): no state");
    CHECK_EQ(mblen("abc", 3), 1, "(5) mblen with n = 3 is 1");
    CHECK_EQ(mblen("", 1), 0, "(5) mblen of the null character is 0");
    CHECK_EQ(wctomb(b, L'Z'), 1, "(5) wctomb of a byte value");
    CHECK_EQ(b[0], 'Z', "(5) wctomb stored it");
    CHECK_EQ(wctomb(b, 0x100), -1, "(5) wctomb of a value above a byte");
    CHECK_EQ(wctomb(NULL, L'Z'), 0, "(5) wctomb(NULL): no state");

    return mbt_test_summary("tstwchar");
}
