/*
 * tstctpm.c - libc370 #399 on MVS: the ctype class of [ ] ^, which cc370
 * encodes as the CP037 code points X'BA', X'BB' and X'B0'.
 *
 *   1 ispunct(), isgraph() and isprint() are true for each, as macro
 *     and as function; all three were 0
 *   2 the compiler's '[' ']' '^' are those code points
 *   3 controls: isalnum() stays 0 for them, '!' (X'5A') stays punct
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstctpm.c \
 *         -o $S/TSTCTPG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstctpm.c \
 *         -o $S/TSTCTPR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTCTPG=TSTCTPG.iebcopy \
 *         TSTCTPR=TSTCTPR.iebcopy -o cp -xmit --dsn IBMUSER.LIBC370.CPSCR
 *
 * then jcl/recvctpm.jcl and jcl/tstctpm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <ctype.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what, int c)
{
    wtof("TSTCTPM %s: %s X'%02X'", cond ? "PASS" : "FAIL", what, c);
    if (!cond) bad++;
}

int main(void)
{
    static const int    code[3] = { 0xBA, 0xBB, 0xB0 };
    static const char   src[3]  = { '[', ']', '^' };
    int                 i;

    for (i = 0; i < 3; i++) {
        int c = code[i];

        want(src[i] == c, "the compiler's character is", (unsigned char)src[i]);
        want(ispunct(c) != 0, "ispunct() macro", c);
        want(isgraph(c) != 0, "isgraph() macro", c);
        want(isprint(c) != 0, "isprint() macro", c);
        want((ispunct)(c) != 0, "ispunct() function", c);
        want((isgraph)(c) != 0, "isgraph() function", c);
        want((isprint)(c) != 0, "isprint() function", c);
        want(isalnum(c) == 0 && iscntrl(c) == 0, "not alnum, not cntrl", c);
    }
    want(ispunct('!') != 0, "control: ispunct('!')", '!');
    return bad ? 8 : 0;
}
