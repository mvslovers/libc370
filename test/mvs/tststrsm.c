/*
 * tststrsm.c - libc370 #393 on MVS: strstr() with an empty s2.
 *
 *   1 strstr(s, "") returns s (C99 7.21.5.7); it returned NULL
 *   2 strstr("", "") returns its first argument
 *   3 controls: a match in the middle, and no match, as before
 *
 * The arguments go through volatile pointers, so the compiler cannot fold
 * the calls into constants and the library function runs.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tststrsm.c \
 *         -o $S/TSTSTRSG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tststrsm.c \
 *         -o $S/TSTSTRSR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSTRSG=TSTSTRSG.iebcopy \
 *         TSTSTRSR=TSTSTRSR.iebcopy -o ss -xmit --dsn IBMUSER.LIBC370.SSSCR
 *
 * then jcl/recvstrsm.jcl and jcl/tststrsm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTSTRSM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    static char         abc[] = "abcdef";
    static char         none[] = "";
    const char * volatile s     = abc;
    const char * volatile e     = none;
    const char * volatile cd    = "cd";
    const char * volatile xy    = "xy";

    want(strstr(s, e) == s, "strstr(s, \"\") returns s");
    want(strstr(e, e) == e, "strstr(\"\", \"\") returns its first argument");
    want(strstr(s, cd) == s + 2, "strstr(\"abcdef\", \"cd\") finds offset 2");
    want(strstr(s, xy) == NULL, "strstr(\"abcdef\", \"xy\") is NULL");
    return bad ? 8 : 0;
}
