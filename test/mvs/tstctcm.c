/*
 * tstctcm.c - libc370 #416: the ctype macros take a plain char.
 *
 * Compile time: with the corrected <ctype.h> this file compiles under
 * -Wall -Werror; with the earlier one every macro below gave "subscript
 * has type `char'" and the build failed.  That is the red case.
 *
 * Run time on MVS:
 *   1 a char argument classifies and converts as an int one does
 *   2 EOF still reads the entry before the table: no class, and
 *     tolower(EOF)/toupper(EOF) are EOF
 *   3 a char above X'7F' (cc370's char is unsigned) is not taken as
 *     negative: isdigit((char)'9') with '9' = X'F9'
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstctcm.c \
 *         -o $S/TSTCTCG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstctcm.c -o $S/TSTCTCR \
 *         -flinker-output=iebcopy      (red: fails to compile)
 *     cd $S && ld370 --pack TSTCTCG=TSTCTCG.iebcopy -o cc -xmit \
 *         --dsn IBMUSER.LIBC370.CCSCR
 *
 * then jcl/recvctcm.jcl and jcl/tstctcm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <ctype.h>
#include <stdio.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTCTCM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    static const char   text[] = "aZ9 ";
    const char          *p     = text;
    volatile int        eof    = EOF;

    want(isalpha(p[0]) && islower(p[0]) && !isupper(p[0]), "isalpha/islower('a')");
    want(isupper(p[1]) && isalnum(p[1]), "isupper/isalnum('Z')");
    want(isdigit(p[2]) && isxdigit(p[2]) && isgraph(p[2]), "isdigit('9'), X'F9'");
    want(isspace(p[3]) && isblank(p[3]) && isprint(p[3]), "isspace(' ')");
    want(toupper(p[0]) == 'A' && tolower(p[1]) == 'z', "toupper/tolower");
    want(!iscntrl(p[0]) && !ispunct(p[0]), "'a' not cntrl, not punct");
    want(!isalpha(eof) && !isprint(eof) && !iscntrl(eof), "EOF has no class");
    want(tolower(eof) == EOF && toupper(eof) == EOF, "tolower/toupper(EOF) is EOF");
    return bad ? 8 : 0;
}
