/*
 * tstidcrec.c - libc370 #590: the IDCAMS input records of a command.
 *
 * ISSUE #590: idcams() handed IDCAMS its whole command as one record,
 * and IDCAMS reads columns 2-72; rename()'s " ALTER old NEWNAME(new)"
 * went past column 72 with long names, and the data set was renamed to
 * the new name cut there.  __idcrec() now cuts the command into records
 * of at most 72 columns, at '\n' and, where a line is longer, at a blank
 * outside quotes with IDCAMS's '-' continuation.
 *
 * This #includes @@idcrec.c and checks the records: each one at most 72
 * columns, a continued one ending in '-', and the words of the command
 * coming out unchanged and in order.  The red is on MVS: test/mvs/
 * tstidcren.c renames to a 33-character name with the installed library.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -Werror -I $R -o t tstidcrec.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include "../../src/mvs/idcams/@@idcrec.c"

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

/* the records of cmds, joined back: continuations removed, one blank
   between records; returns the number of records, -2 if unsplittable */
static int records(const char *cmds, char *joined, int *maxlen, int *contok)
{
    IDCREC  c = { cmds, 0 };
    char    rec[IDCREC_MAX + 1];
    int     n, count = 0;

    joined[0] = 0;
    *maxlen = 0;
    *contok = 1;
    while ((n = __idcrec(&c, rec)) >= 0) {
        int len = (int)strlen(rec);

        if (len != n) *contok = 0;
        if (n > *maxlen) *maxlen = n;
        if (c.cont) {
            if (rec[n - 1] != '-') *contok = 0;
            rec[n - 1] = 0;
        }
        strcat(joined, rec);
        count++;
    }
    return n == -2 ? -2 : count;
}

/* the words of s, blanks collapsed */
static void words(const char *s, char *out)
{
    int k = 0, sp = 1;

    for (; *s; s++) {
        if (*s == ' ' || *s == '\n') {
            if (!sp) out[k++] = ' ';
            sp = 1;
        }
        else {
            out[k++] = *s;
            sp = 0;
        }
    }
    if (k > 0 && out[k - 1] == ' ') k--;
    out[k] = 0;
}

static void same(const char *cmds, int want, const char *what)
{
    char    joined[600], a[600], b[600];
    int     maxlen, contok;
    int     n = records(cmds, joined, &maxlen, &contok);

    if (want < 0) {             /* refused: nothing else to compare */
        check(n == want, what);
        return;
    }
    words(cmds, a);
    words(joined, b);
    check(n == want && maxlen <= 72 && contok && strcmp(a, b) == 0, what);
    if (!(n == want && maxlen <= 72 && contok && strcmp(a, b) == 0))
        printf("      records %d (want %d), longest %d, words %s\n",
               n, want, maxlen, strcmp(a, b) == 0 ? "same" : "differ");
}

int main(void)
{
    char    big[200];

    same(" DELETE IBMUSER.X", 1, "a short command is one record");
    same(" ALTER IBMUSER.OFTPTMP.T0000154 NEWNAME(IBMUSER.OFTPIN.COLL2.M38B.TO.DEV1)",
         2, "the #590 rename (74 columns) is two records");
    same(" ALTER AAAAAAAA.BBBBBBBB.CCCCCCCC.DDDDDDDD.EEE"
         " NEWNAME(FFFFFFFF.GGGGGGGG.HHHHHHHH.IIIIIIII.JJJ)",
         2, "two 44-character names");
    same(" LISTC LEVEL('IBMUSER.SOME.RATHER.LONG.LEVEL.QUALIFIER.X')"
         " OUTFILE(SYS00001) ALL", 2, "LISTC with a long level");
    same(" DELETE A.B\n DELETE C.D", 2, "two lines, two records");
    same(" REPRO INDATASET('A B C D E F G H I J K L M N O P Q R S T U V W X Y Z"
         " 0 1 2 3 4 5 6 7 8 9') OUTDATASET(X.Y)",
         -2, "blanks inside quotes are not break points: refused");

    memset(big, 'X', sizeof(big));
    big[0] = ' ';
    big[100] = 0;
    check(__idcchk(big) == -2, "a word past column 72 is refused");
    check(__idcchk(" ALTER A NEWNAME(B)") == 0, "__idcchk of a short command");

    printf("tstidcrec: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
