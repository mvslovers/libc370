/*
 * tstidcren.c - libc370 #590 on MVS: rename() of a whole data set to a
 * name that takes " ALTER old NEWNAME(new)" past column 72.
 *
 * Built twice with a different name prefix (-DPFX): against this tree
 * (TSTIDCRN, green) and the installed library (TSTIDCRR, red: the new
 * name is cut at column 72 and the data set is catalogued under the
 * shortened name).
 *
 *   1 a 27-character name renamed to a 44-character one (87 columns)
 *   2 two 44-character names (105 columns)
 *   3 the lengths of the report: 24 -> 33 characters (74 columns).  The
 *     old library cut the name to 32 and dropped the ')' - IDCAMS
 *     accepted that and catalogued the cut name; cases 1 and 2 instead
 *     cut inside the name list and IDCAMS refused them (rc 12)
 *
 * rename() gets the names unquoted, as a batch caller writes them.
 * Each case creates the old data set with fopen(), renames it, and asks
 * which names exist afterwards: the full new name must, the cut one must
 * not.  Every name is removed at the end, whichever exists.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <stdio.h>
#include <string.h>
#include <mvs/wto.h>

#ifndef PFX
#define PFX "IBMUSER.LIBC370.RN"
#endif

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTIDCREN %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static int exists(const char *dsn)
{
    char    q[60];
    FILE    *fp;

    sprintf(q, "'%s'", dsn);
    fp = fopen(q, "r");
    if (fp) fclose(fp);
    return fp != NULL;
}

static void kill(const char *dsn)
{
    char q[60];

    sprintf(q, "'%s'", dsn);
    if (exists(dsn)) remove(q);
}

/* the new name as the old library cut it: what fitted into column 72 */
static void cutname(const char *old, const char *new, char *cut)
{
    int room = 72 - (int)strlen(" ALTER ") - (int)strlen(old)
             - (int)strlen(" NEWNAME(");

    if (room < 0) room = 0;
    if (room > (int)strlen(new)) room = (int)strlen(new);
    memcpy(cut, new, room);
    cut[room] = 0;
    while (room > 0 && cut[room - 1] == '.') cut[--room] = 0;
}

static void one(const char *old, const char *new, const char *what)
{
    char    qo[60], cut[50], msg[160];
    FILE    *fp;
    int     rc;

    cutname(old, new, cut);
    kill(old); kill(new); kill(cut);

    sprintf(qo, "'%s'", old);
    fp = fopen(qo, "w");
    if (fp) { fputs("x\n", fp); fclose(fp); }
    want(fp != NULL, "the old data set is created");

    /* unquoted, as a batch caller writes it (no TSO prefix in batch):
       quoted names put the cut inside a quoted string, which IDCAMS
       refuses outright instead of renaming */
    rc = rename(old, new);
    sprintf(msg, "%s: rename rc=%d", what, rc);
    want(rc == 0, msg);
    sprintf(msg, "%s: %s exists", what, new);
    want(exists(new), msg);
    sprintf(msg, "%s: the cut name %s does not", what, cut);
    want(!exists(cut), msg);

    kill(old); kill(new); kill(cut);
}

int main(void)
{
    /* 1: 27 -> 44 characters; the case of the report, under our prefix */
    one(PFX ".T0000154", PFX ".OFTPIN.COLL2.M38B.TO.DEV1", "1 (87 columns)");
    /* 2: two 44-character names */
    one(PFX ".AAAAAAAA.BBBBBBBB.CCCCCCC", PFX ".DDDDDDDD.EEEEEEEE.FFFFFFF",
        "2 (two 44-character names)");
    /* 3: 24 -> 33 characters, the silent case */
    one(PFX ".T0154", PFX ".COLL2.TO.DEVX1", "3 (74 columns)");

    wtof("TSTIDCREN VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
