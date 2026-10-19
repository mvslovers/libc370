/*
 * tstbsrm.c - libc370 #388 on MVS: bsearch() calls compar(key, element).
 *
 * The comparator below is asymmetric, as C99 allows: its first argument is
 * a string key, its second an element of a table of structures.  It
 * checks that its second argument lies inside the table, so the order is
 * observed, not inferred from the result.
 *
 *   1 every key of the table is found, at its own element
 *   2 a key below, between and above the entries is not found
 *   3 compar() never got an element as its first argument
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstbsrm.c \
 *         -o $S/TSTBSRG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstbsrm.c \
 *         -o $S/TSTBSRR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTBSRG=TSTBSRG.iebcopy \
 *         TSTBSRR=TSTBSRR.iebcopy -o bs -xmit --dsn IBMUSER.LIBC370.BSSCR
 *
 * then jcl/recvbsrm.jcl and jcl/tstbsrm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>

typedef struct { const char *ext; int n; } ENT;

static const ENT    tab[] = {
    { "css", 1 }, { "gif", 2 }, { "htm", 3 }, { "jpg", 4 },
    { "js",  5 }, { "png", 6 }, { "txt", 7 }, { "zip", 8 },
};
#define NTAB (sizeof(tab) / sizeof(tab[0]))

static int bad, swapped;

static void want(int cond, const char *what)
{
    wtof("TSTBSRM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* C99: the key (a string) first, the element second */
static int cmp(const void *key, const void *elem)
{
    const ENT   *e = elem;

    if ((const char *)elem < (const char *)tab ||
        (const char *)elem >= (const char *)(tab + NTAB)) {
        swapped++;          /* the "element" is not in the table */
        return 0;
    }
    return strcmp((const char *)key, e->ext);
}

int main(void)
{
    static const char * const   miss[] = { "aaa", "html", "zzz" };
    const ENT                   *e;
    unsigned                    i, found = 0;

    for (i = 0; i < NTAB; i++) {
        swapped = 0;
        e = bsearch(tab[i].ext, tab, NTAB, sizeof(tab[0]), cmp);
        if (e == &tab[i] && !swapped) found++;
    }
    want(found == NTAB, "every key is found at its own element");

    for (i = 0; i < 3; i++) {
        swapped = 0;
        e = bsearch(miss[i], tab, NTAB, sizeof(tab[0]), cmp);
        want(e == NULL && !swapped, miss[i]);
    }
    return bad ? 8 : 0;
}
