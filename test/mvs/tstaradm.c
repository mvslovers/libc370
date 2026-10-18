/*
 * tstaradm.c - libc370 #398 on MVS: arrayaddf() of a string longer than
 * 1023 characters.  It went through a 1024-byte buffer on the stack.
 *
 *   1 a 2000-character string formats, is added whole, and the stack
 *     around the call is intact (a guard area in the caller's frame)
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstaradm.c \
 *         -o $S/TSTARADG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstaradm.c \
 *         -o $S/TSTARADR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTARADG=TSTARADG.iebcopy \
 *         TSTARADR=TSTARADR.iebcopy -o ar -xmit --dsn IBMUSER.LIBC370.ARSCR
 *
 * then jcl/recvaradm.jcl and jcl/tstaradm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <ext/array.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTARADM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static char long_s[2001];

/* the call in a frame of its own, with guard bytes the old 1024-byte
   buffer of arrayaddf() would reach from below */
static int add(char ***arr)
{
    volatile char   guard[64];
    int             i, rc, ok = 1;

    for (i = 0; i < 64; i++) guard[i] = 'G';
    rc = arrayaddf(arr, "%s-%d", long_s, 7);
    for (i = 0; i < 64; i++) if (guard[i] != 'G') ok = 0;
    want(ok, "1 the caller's stack is intact");
    return rc;
}

int main(void)
{
    char    **arr = NULL;
    char    *s;
    int     i;

    for (i = 0; i < 2000; i++) long_s[i] = 'L';
    long_s[2000] = 0;

    want(add(&arr) == 0, "1 arrayaddf() of 2002 characters returns 0");
    s = arr ? arrayget(&arr, 1) : NULL;
    want(s && strlen(s) == 2002 && strcmp(s + 2000, "-7") == 0,
         "1 the whole string is in the array");
    wtof("TSTARADM VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
