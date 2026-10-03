/*
 * tstinttyp.c - libc370 #314 on MVS: <inttypes.h>.
 *
 * Two halves:
 *
 *   - every PRI and SCN macro the header defines is used once with its
 *     own type, in format_check().  Compiled with -Wall -Werror, cc370's
 *     -Wformat checks each against the <stdint.h> type, so a macro that
 *     does not match its type fails the build - that is the check, and it
 *     runs at compile time.  The macro list was generated from the header
 *     (84 PRI, 70 SCN); no runtime result is asserted there.
 *   - at run time: a value per width through snprintf and sscanf, and
 *     imaxabs, imaxdiv, strtoimax, strtoumax.
 *
 * SCN*8, SCN*64 and SCN*MAX came with #336, once #318 had given scanf hh,
 * ll and j; the test pins that they exist.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstinttyp.c -o TSTINTT -flinker-output=iebcopy
 *          ld370 --pack TSTINTT=TSTINTT.iebcopy -o tstinttyp -xmit \
 *                --dsn IBMUSER.LIBC370.INTTSCR
 * Install: jcl/recvintt.jcl.   Run: jcl/tstinttyp.jcl.
 *
 * mvsdev JOB01167, 2026-10-02 (RECEIVE JOB01166): GREEN CC 0000, 19/19.
 * Before #321 was fixed, JOB01163: 16/19, PRId64 and PRIdMAX printed
 * every negative value unsigned - which is how #321 was found.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static void format_check(void)
{
    char b[80];

    snprintf(b, sizeof(b), "%" PRId8 "%" PRIi8, (int8_t)1, (int8_t)2);
    snprintf(b, sizeof(b), "%" PRIo8 "%" PRIu8 "%" PRIx8 "%" PRIX8, (uint8_t)1, (uint8_t)2, (uint8_t)3, (uint8_t)4);
    snprintf(b, sizeof(b), "%" PRIdLEAST8 "%" PRIiLEAST8, (int_least8_t)1, (int_least8_t)2);
    snprintf(b, sizeof(b), "%" PRIoLEAST8 "%" PRIuLEAST8 "%" PRIxLEAST8 "%" PRIXLEAST8, (uint_least8_t)1, (uint_least8_t)2, (uint_least8_t)3, (uint_least8_t)4);
    snprintf(b, sizeof(b), "%" PRIdFAST8 "%" PRIiFAST8, (int_fast8_t)1, (int_fast8_t)2);
    snprintf(b, sizeof(b), "%" PRIoFAST8 "%" PRIuFAST8 "%" PRIxFAST8 "%" PRIXFAST8, (uint_fast8_t)1, (uint_fast8_t)2, (uint_fast8_t)3, (uint_fast8_t)4);
    snprintf(b, sizeof(b), "%" PRId16 "%" PRIi16, (int16_t)1, (int16_t)2);
    snprintf(b, sizeof(b), "%" PRIo16 "%" PRIu16 "%" PRIx16 "%" PRIX16, (uint16_t)1, (uint16_t)2, (uint16_t)3, (uint16_t)4);
    snprintf(b, sizeof(b), "%" PRIdLEAST16 "%" PRIiLEAST16, (int_least16_t)1, (int_least16_t)2);
    snprintf(b, sizeof(b), "%" PRIoLEAST16 "%" PRIuLEAST16 "%" PRIxLEAST16 "%" PRIXLEAST16, (uint_least16_t)1, (uint_least16_t)2, (uint_least16_t)3, (uint_least16_t)4);
    snprintf(b, sizeof(b), "%" PRIdFAST16 "%" PRIiFAST16, (int_fast16_t)1, (int_fast16_t)2);
    snprintf(b, sizeof(b), "%" PRIoFAST16 "%" PRIuFAST16 "%" PRIxFAST16 "%" PRIXFAST16, (uint_fast16_t)1, (uint_fast16_t)2, (uint_fast16_t)3, (uint_fast16_t)4);
    snprintf(b, sizeof(b), "%" PRId32 "%" PRIi32, (int32_t)1, (int32_t)2);
    snprintf(b, sizeof(b), "%" PRIo32 "%" PRIu32 "%" PRIx32 "%" PRIX32, (uint32_t)1, (uint32_t)2, (uint32_t)3, (uint32_t)4);
    snprintf(b, sizeof(b), "%" PRIdLEAST32 "%" PRIiLEAST32, (int_least32_t)1, (int_least32_t)2);
    snprintf(b, sizeof(b), "%" PRIoLEAST32 "%" PRIuLEAST32 "%" PRIxLEAST32 "%" PRIXLEAST32, (uint_least32_t)1, (uint_least32_t)2, (uint_least32_t)3, (uint_least32_t)4);
    snprintf(b, sizeof(b), "%" PRIdFAST32 "%" PRIiFAST32, (int_fast32_t)1, (int_fast32_t)2);
    snprintf(b, sizeof(b), "%" PRIoFAST32 "%" PRIuFAST32 "%" PRIxFAST32 "%" PRIXFAST32, (uint_fast32_t)1, (uint_fast32_t)2, (uint_fast32_t)3, (uint_fast32_t)4);
    snprintf(b, sizeof(b), "%" PRId64 "%" PRIi64, (int64_t)1, (int64_t)2);
    snprintf(b, sizeof(b), "%" PRIo64 "%" PRIu64 "%" PRIx64 "%" PRIX64, (uint64_t)1, (uint64_t)2, (uint64_t)3, (uint64_t)4);
    snprintf(b, sizeof(b), "%" PRIdLEAST64 "%" PRIiLEAST64, (int_least64_t)1, (int_least64_t)2);
    snprintf(b, sizeof(b), "%" PRIoLEAST64 "%" PRIuLEAST64 "%" PRIxLEAST64 "%" PRIXLEAST64, (uint_least64_t)1, (uint_least64_t)2, (uint_least64_t)3, (uint_least64_t)4);
    snprintf(b, sizeof(b), "%" PRIdFAST64 "%" PRIiFAST64, (int_fast64_t)1, (int_fast64_t)2);
    snprintf(b, sizeof(b), "%" PRIoFAST64 "%" PRIuFAST64 "%" PRIxFAST64 "%" PRIXFAST64, (uint_fast64_t)1, (uint_fast64_t)2, (uint_fast64_t)3, (uint_fast64_t)4);
    snprintf(b, sizeof(b), "%" PRIdMAX "%" PRIiMAX, (intmax_t)1, (intmax_t)2);
    snprintf(b, sizeof(b), "%" PRIoMAX "%" PRIuMAX "%" PRIxMAX "%" PRIXMAX, (uintmax_t)1, (uintmax_t)2, (uintmax_t)3, (uintmax_t)4);
    snprintf(b, sizeof(b), "%" PRIdPTR "%" PRIiPTR, (intptr_t)1, (intptr_t)2);
    snprintf(b, sizeof(b), "%" PRIoPTR "%" PRIuPTR "%" PRIxPTR "%" PRIXPTR, (uintptr_t)1, (uintptr_t)2, (uintptr_t)3, (uintptr_t)4);
    { int16_t a, c; uint16_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNd16 " %" SCNi16 " %" SCNo16 " %" SCNu16 " %" SCNx16, &a, &c, &d, &e, &f); }
    { int_least16_t a, c; uint_least16_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdLEAST16 " %" SCNiLEAST16 " %" SCNoLEAST16 " %" SCNuLEAST16 " %" SCNxLEAST16, &a, &c, &d, &e, &f); }
    { int_fast16_t a, c; uint_fast16_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdFAST16 " %" SCNiFAST16 " %" SCNoFAST16 " %" SCNuFAST16 " %" SCNxFAST16, &a, &c, &d, &e, &f); }
    { int32_t a, c; uint32_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNd32 " %" SCNi32 " %" SCNo32 " %" SCNu32 " %" SCNx32, &a, &c, &d, &e, &f); }
    { int_least32_t a, c; uint_least32_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdLEAST32 " %" SCNiLEAST32 " %" SCNoLEAST32 " %" SCNuLEAST32 " %" SCNxLEAST32, &a, &c, &d, &e, &f); }
    { int_fast32_t a, c; uint_fast32_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdFAST32 " %" SCNiFAST32 " %" SCNoFAST32 " %" SCNuFAST32 " %" SCNxFAST32, &a, &c, &d, &e, &f); }
    { int8_t a, c; uint8_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNd8 " %" SCNi8 " %" SCNo8 " %" SCNu8 " %" SCNx8, &a, &c, &d, &e, &f); }
    { int_least8_t a, c; uint_least8_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdLEAST8 " %" SCNiLEAST8 " %" SCNoLEAST8 " %" SCNuLEAST8 " %" SCNxLEAST8, &a, &c, &d, &e, &f); }
    { int_fast8_t a, c; uint_fast8_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdFAST8 " %" SCNiFAST8 " %" SCNoFAST8 " %" SCNuFAST8 " %" SCNxFAST8, &a, &c, &d, &e, &f); }
    { int64_t a, c; uint64_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNd64 " %" SCNi64 " %" SCNo64 " %" SCNu64 " %" SCNx64, &a, &c, &d, &e, &f); }
    { int_least64_t a, c; uint_least64_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdLEAST64 " %" SCNiLEAST64 " %" SCNoLEAST64 " %" SCNuLEAST64 " %" SCNxLEAST64, &a, &c, &d, &e, &f); }
    { int_fast64_t a, c; uint_fast64_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdFAST64 " %" SCNiFAST64 " %" SCNoFAST64 " %" SCNuFAST64 " %" SCNxFAST64, &a, &c, &d, &e, &f); }
    { intmax_t a, c; uintmax_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdMAX " %" SCNiMAX " %" SCNoMAX " %" SCNuMAX " %" SCNxMAX, &a, &c, &d, &e, &f); }
    { intptr_t a, c; uintptr_t d, e, f;
      sscanf("1 2 3 4 5", "%" SCNdPTR " %" SCNiPTR " %" SCNoPTR " %" SCNuPTR " %" SCNxPTR, &a, &c, &d, &e, &f); }
}

static int fmt(const char *want, const char *f, ...)
{
    char b[48];
    va_list ap;

    va_start(ap, f);
    vsnprintf(b, sizeof(b), f, ap);
    va_end(ap);
    if (strcmp(b, want) != 0) {
        printf("        got \"%s\", want \"%s\"\n", b, want);
        return (0);
    }
    return (1);
}

int main(void)
{
    char *end;
    int16_t h;
    uint32_t u;
    intptr_t p;
    imaxdiv_t q;
    const char *s;

    printf("=== tstinttyp: <inttypes.h> on MVS (#314) ===\n\n");
    format_check();

    printf("(1) PRI at run time:\n");
    CHECK(fmt("-128", "%" PRId8, (int8_t)-128), "(1) PRId8 -128");
    CHECK(fmt("ff", "%" PRIx8, (uint8_t)255), "(1) PRIx8 255");
    CHECK(fmt("-32768", "%" PRId16, (int16_t)-32768), "(1) PRId16");
    CHECK(fmt("4294967295", "%" PRIu32, (uint32_t)4294967295UL),
          "(1) PRIu32 UINT32_MAX");
    CHECK(fmt("-5", "%" PRId64, (int64_t)-5), "(1) PRId64 -5");
    CHECK(fmt("-4294967296", "%" PRIdMAX, (intmax_t)-4294967296LL),
          "(1) PRIdMAX -2**32");
    CHECK(fmt("-9223372036854775808", "%" PRId64, INT64_MIN),
          "(1) PRId64 INT64_MIN");
    CHECK(fmt("FFFFFFFFFFFFFFFF", "%" PRIX64, UINT64_MAX),
          "(1) PRIX64 UINT64_MAX");
    CHECK(fmt("1234567890123", "%" PRIdMAX, (intmax_t)1234567890123LL),
          "(1) PRIdMAX");
    CHECK(fmt("-7|8", "%" PRIdPTR "|%" PRIuPTR, (intptr_t)-7, (uintptr_t)8),
          "(1) PRIdPTR, PRIuPTR");

    printf("\n(2) SCN at run time:\n");
    h = 0;
    CHECK(sscanf("-1234", "%" SCNd16, &h) == 1 && h == -1234, "(2) SCNd16");
    u = 0;
    CHECK(sscanf("fffffffe", "%" SCNx32, &u) == 1 && u == 0xFFFFFFFEUL,
          "(2) SCNx32");
    p = 0;
    CHECK(sscanf("-42", "%" SCNdPTR, &p) == 1 && p == -42, "(2) SCNdPTR");
#if defined(SCNd8) && defined(SCNd64) && defined(SCNdMAX) && defined(SCNuLEAST64)
    CHECK(1, "(2) SCN*8/64/MAX exist since #336");
#else
    CHECK(0, "(2) SCN*8/64/MAX exist since #336");
#endif

    printf("\n(3) functions:\n");
    s = "  -9223372036854775808x";
    CHECK(strtoimax(s, &end, 10) == INTMAX_MIN && *end == 'x',
          "(3) strtoimax INTMAX_MIN, endptr");
    CHECK(strtoumax("ffffffffffffffff", NULL, 16) == UINTMAX_MAX,
          "(3) strtoumax UINTMAX_MAX");
    CHECK(strtoimax("JR", NULL, 36) == 711, "(3) strtoimax base 36");
    CHECK(imaxabs((intmax_t)-4294967296LL) == 4294967296LL, "(3) imaxabs");
    q = imaxdiv((intmax_t)-10000000000LL, 3);
    CHECK(q.quot == -3333333333LL && q.rem == -1, "(3) imaxdiv");

    printf("\n=== tstinttyp: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
