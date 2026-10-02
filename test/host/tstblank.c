/*
 * tstblank.c - libc370 #314: isblank() is true for exactly the two
 * EBCDIC blanks, X'05' (HT) and X'40' (space), through bit 0x0800 of
 * __isbuf.
 *
 * The table is EBCDIC, so this test reads it by code point rather than
 * by host character constant: it #includes src/stdio/@@isbuf.c and walks
 * all 256 entries.  It also pins that no other bit moved -- the blanks
 * keep isspace, and the rest of the table is what it was before
 * (checked by the bit, since the entries themselves are not repeated
 * here).  test/mvs/tstblank.c checks the same with the compiler's ' '
 * and '\t' on MVS.
 *
 * BUILD / RUN (host, from the repository root):
 *
 *     cc -std=gnu99 -Wall -Wextra -Werror -o t test/host/tstblank.c && ./t
 *
 * No -I include: @@isbuf.c needs nothing but <stddef.h>, and libc370's
 * <stdio.h> clashes with the host's snprintf under -Werror.
 *
 * RED, 2026-10-02: the same test against the table before #314 (git
 * stash of @@isbuf.c) fails 5 of 12 -- no entry carries 0x0800.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include "../../src/stdio/@@isbuf.c"

static int run = 0, failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        run++;                                                            \
        if (!(cond)) { failed++; printf("  FAIL: %s\n", (msg)); }         \
    } while (0)

int main(void)
{
    int c;
    int blanks = 0;

    printf("=== tstblank: isblank on the EBCDIC table (#314), host ===\n");
    for (c = 0; c < 256; c++) {
        if (__isbuf[c] & 0x0800U) {
            blanks++;
        }
    }
    CHECK(blanks == 2, "exactly two entries carry 0x0800");
    CHECK(__isbuf[0x05] & 0x0800U, "X'05' (HT) is blank");
    CHECK(__isbuf[0x40] & 0x0800U, "X'40' (space) is blank");
    CHECK(__isbuf[0x05] & 0x0100U, "X'05' is still space");
    CHECK(__isbuf[0x40] & 0x0100U, "X'40' is still space");
    CHECK(__isbuf[0x40] & 0x0040U, "X'40' is still print");
    CHECK(__isbuf[0x05] == 0x0904U, "X'05' is cntrl|space|blank");
    CHECK(__isbuf[0x40] == 0x0940U, "X'40' is print|space|blank");
    CHECK(!(__isbuf[0x25] & 0x0800U), "X'25' (LF) is not blank");
    CHECK(!(__isbuf[0x15] & 0x0800U), "X'15' (NL) is not blank");
    CHECK(!(__isbuf[0x0B] & 0x0800U), "X'0B' (VT) is not blank");
    CHECK(__isbuf[-1] == 0, "EOF is nothing");

    printf("=== tstblank: %d/%d passed ===\n", run - failed, run);
    return failed > 0 ? 1 : 0;
}
