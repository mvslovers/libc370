/*
 * tststrci.c - libc370 #183: the case-insensitive comparisons.
 *
 * strcasecmp/strncasecmp are the POSIX names; stricmp/strncmpi are the
 * MS-style aliases that predate them in this library.  All four fold
 * through tolower(), i.e. the __tolow table, which is what makes them
 * EBCDIC-correct -- an ASCII-style fold (c | 0x20) would be wrong here.
 *
 * MVS target only, and that is the whole point: on a host the fold runs
 * through the host's ASCII table and cannot observe the property under
 * test.  EBCDIC letters sit in three runs with gaps between them --
 * a-i X'81'-X'89', j-r X'91'-X'99', s-z X'A2'-X'A9' -- so case (2)
 * spans all three deliberately.  Case (1) is the control: if it fails,
 * the run is not EBCDIC and nothing below means what it says.
 *
 * Build:   cc370 -O1 -Iinclude -L build/sdk test/mvs/tststrci.c \
 *                -o TSTSTRCI -flinker-output=iebcopy
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tststrci.jcl.
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 *
 * Run:     mvsdev JOB00409, CC 0000, 37/37, 2026-09-22.
 *
 * Proven red the same day against a build with two deliberate defects --
 * an n-compare that never looks for the NUL, and an ASCII-style fold --
 * which fails 12 of the 37 (JOB00421).  Worth keeping from building that
 * control: the ASCII fold has to be written `c | 0x20`.  A `c | 0x40`
 * fold looks equally wrong and is not: it maps all 26 EBCDIC letters to
 * their uppercase exactly (X'81'|X'40' = X'C1', and so on through all
 * three runs), so a control built on it passes case (2) and measures
 * nothing (JOB00414).
 */
#include <stdio.h>
#include <string.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }         \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }         \
    } while (0)

/* the sign is what is specified, not the magnitude */
#define CHECK_SIGN(got, want, msg)                                        \
    do {                                                                  \
        int g_ = (got), w_ = (want);                                      \
        int ok_ = (w_ == 0) ? (g_ == 0)                                   \
                : (w_ <  0) ? (g_ <  0) : (g_ > 0);                       \
        mbt_run++;                                                        \
        if (ok_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }          \
        else { mbt_failed++;                                              \
               printf("  FAIL: %s (got %d, wanted sign %d)\n",            \
                      (msg), g_, w_); }                                   \
    } while (0)

int main(void)
{
    char g1[16];
    char g2[16];

    printf("=== TSTSTRCI: libc370 #183 case-insensitive compares ===\n\n");

    printf("(1) the control - this run is EBCDIC\n");
    CHECK((unsigned char)'a' == 0x81, "(1) 'a' is X'81'");
    CHECK((unsigned char)'j' == 0x91, "(1) 'j' is X'91'");
    CHECK((unsigned char)'s' == 0xA2, "(1) 's' is X'A2'");
    CHECK((unsigned char)'A' == 0xC1, "(1) 'A' is X'C1'");

    printf("(2) equality across all three EBCDIC letter runs\n");
    CHECK(strcasecmp("abcdefghi", "ABCDEFGHI") == 0,
          "(2) strcasecmp folds a-i");
    CHECK(strcasecmp("jklmnopqr", "JKLMNOPQR") == 0,
          "(2) strcasecmp folds j-r");
    CHECK(strcasecmp("stuvwxyz", "STUVWXYZ") == 0,
          "(2) strcasecmp folds s-z");
    CHECK(strcasecmp("CopyReplacing", "COPYREPLACING") == 0,
          "(2) strcasecmp folds a mixed-case word");
    CHECK(strncasecmp("abcdefghi", "ABCDEFGHI", 9) == 0,
          "(2) strncasecmp folds a-i");
    CHECK(strncasecmp("jklmnopqr", "JKLMNOPQR", 9) == 0,
          "(2) strncasecmp folds j-r");
    CHECK(strncasecmp("stuvwxyz", "STUVWXYZ", 8) == 0,
          "(2) strncasecmp folds s-z");

    printf("(3) non-letters pass through unfolded\n");
    CHECK(strcasecmp("0123456789", "0123456789") == 0,
          "(3) digits compare equal");
    CHECK(strcasecmp("a.b,c;d", "A.B,C;D") == 0,
          "(3) punctuation between letters is preserved");
    CHECK(strcasecmp("a#b", "a@b") != 0,
          "(3) different specials still differ");

    printf("(4) ordering, in EBCDIC collation\n");
    CHECK_SIGN(strcasecmp("ABC", "abd"), -1, "(4) abc < abd");
    CHECK_SIGN(strcasecmp("abd", "ABC"),  1, "(4) abd > abc");
    CHECK_SIGN(strcasecmp("abc", "ABC"),  0, "(4) equal is 0");
    /* digits sort ABOVE letters in EBCDIC (X'F1' > X'81') */
    CHECK_SIGN(strcasecmp("a", "1"), -1, "(4) 'a' sorts below '1'");

    printf("(5) the terminator ends the comparison\n");
    CHECK_SIGN(strcasecmp("abc", "abcd"), -1, "(5) a prefix is less");
    CHECK_SIGN(strcasecmp("abcd", "ABC"),  1, "(5) the longer is greater");
    CHECK(strcasecmp("", "") == 0, "(5) two empty strings are equal");
    CHECK_SIGN(strcasecmp("", "a"), -1, "(5) empty is less than non-empty");
    /* n past both terminators must stop, not run on into whatever follows */
    CHECK(strncasecmp("ab", "AB", 64) == 0,
          "(5) strncasecmp stops at the terminator, not at n");
    CHECK_SIGN(strncasecmp("ab", "abc", 64), -1,
          "(5) strncasecmp sees the shorter string end");

    printf("(6) n bounds the comparison\n");
    CHECK(strncasecmp("abcXXX", "ABCyyy", 3) == 0,
          "(6) only the first n characters are compared");
    CHECK(strncasecmp("abc", "abd", 0) == 0,
          "(6) n = 0 compares nothing and returns 0");
    CHECK(strncasecmp("abd", "abc", 2) == 0,
          "(6) the differing character past n is not reached");
    CHECK_SIGN(strncasecmp("abd", "abc", 3), 1,
          "(6) the differing character at n-1 is reached");

    printf("(7) n does not read past the terminator\n");
    /* poison the two tails DIFFERENTLY, so an implementation that runs on
       past the NUL compares 'Z' against 'Q' and cannot return 0.  Poisoning
       both the same way would pass either implementation. */
    memset(g1, 'Z', sizeof g1);
    memset(g2, 'Q', sizeof g2);
    memcpy(g1, "ab", 3);
    memcpy(g2, "AB", 3);
    CHECK(strncasecmp(g1, g2, sizeof g1) == 0,
          "(7) n past both terminators still compares equal");
    CHECK(strncmpi(g1, g2, sizeof g1) == 0,
          "(7) strncmpi likewise");
    /* the same shape where the strings differ: the answer must come from
       the letters, not from the poison */
    memcpy(g2, "AC", 3);
    CHECK_SIGN(strncasecmp(g1, g2, sizeof g1), -1,
          "(7) the verdict comes from before the terminator");

    printf("(8) the MS-style aliases agree with the POSIX names\n");
    CHECK(stricmp("Hello", "HELLO") == strcasecmp("Hello", "HELLO"),
          "(8) stricmp agrees on equality");
    CHECK(strncmpi("Hello", "HELLO", 5) == strncasecmp("Hello", "HELLO", 5),
          "(8) strncmpi agrees on equality");
    CHECK_SIGN(stricmp("abc", "ABD"), -1, "(8) stricmp orders like strcasecmp");
    CHECK_SIGN(strncmpi("abd", "ABC", 3), 1,
          "(8) strncmpi orders like strncasecmp");
    /* strncmpi had no in-tree caller and no <ctype.h>, so its tolower() was
       an implicit declaration resolving to the out-of-line function.  This
       is the first thing that has ever exercised it. */
    CHECK(strncmpi("jklmnopqr", "JKLMNOPQR", 9) == 0,
          "(8) strncmpi folds j-r");
    CHECK(strncmpi("stuvwxyz", "STUVWXYZ", 8) == 0,
          "(8) strncmpi folds s-z");

    printf("\n=== TSTSTRCI: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");

    return mbt_failed ? 1 : 0;
}
