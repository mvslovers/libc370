/*
 * tstvolad.c - libc370 #143 on MVS: delete and rename a data set, and
 * the members of a PDS, by name AND volume.
 *
 * remove() and rename() go through IDCAMS, i.e. the catalog, and
 * __delmem()/__renmem() allocate through the catalog too.  An uncataloged
 * data set, or a second data set of the same name on another volume, was
 * out of reach.  __scratch() and __renamev() (SCRATCH, RENAME: SVC 29/30)
 * and __delmemv()/__renmemv() (allocation by DALVLSER) reach it.
 *
 * jcl/setvolad.jcl creates the data sets, in a job of its own: a job
 * holds the SYSDSN ENQ of every name in its JCL for its whole run, and
 * SCRATCH and RENAME take that ENQ themselves.  Volumes (MVS/CE):
 *
 *   IBMUSER.TSTVOLAD.PDS  WORK00 (3350) cataloged,   members AAA BBB
 *   IBMUSER.TSTVOLAD.PDS  WORK01 (3350) uncataloged, members AAA CCC
 *   IBMUSER.TSTVOLAD.UNC  PUB001 (3390) uncataloged
 *   IBMUSER.TSTVOLAD.EXP  WORK01 uncataloged, RETPD=30 (not expired)
 *   IBMUSER.TSTVOLAD.EXP2 WORK01 the same
 *   IBMUSER.TSTVOLAD.DUP  WORK01 uncataloged
 *
 * RED (built with -DTSTVOLAD_RED, against any library: it uses only the
 * old functions) shows that the old API cannot reach the uncataloged
 * copies, and changes nothing.  GREEN does the work with the new
 * functions, checks every step with OBTAIN (__dscbdv()), and removes what
 * setvolad.jcl created, so the next run starts clean.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstvolad.c \
 *         -o $S/TSTVOLG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DTSTVOLAD_RED test/mvs/tstvolad.c \
 *         -o $S/TSTVOLR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTVOLG=TSTVOLG.iebcopy \
 *         TSTVOLR=TSTVOLR.iebcopy -o vo -xmit --dsn IBMUSER.LIBC370.VOSCR
 *
 * then jcl/recvvola.jcl, jcl/setvolad.jcl and jcl/tstvolad.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <mvs/dynalloc.h>
#include <mvs/dscb.h>
#include <mvs/pds.h>
#include <mvs/wto.h>

#define PDS  "IBMUSER.TSTVOLAD.PDS"
#define UNC  "IBMUSER.TSTVOLAD.UNC"
#define UNCR "IBMUSER.TSTVOLAD.UNCR"
#define UNCX "IBMUSER.TSTVOLAD.UNCX"
#define EXP  "IBMUSER.TSTVOLAD.EXP"
#define EXP2 "IBMUSER.TSTVOLAD.EXP2"
#define DUP  "IBMUSER.TSTVOLAD.DUP"

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTVOLAD %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* 1 when dsn is in the VTOC of vol */
static int onvol(const char *dsn, const char *vol)
{
    DSCB dscb;

    return __dscbdv(dsn, vol, &dscb) == 0;
}

/* 1 when the PDS dsn - on vol, or the cataloged one for vol NULL - has
   the member mem */
static int hasmem(const char *dsn, const char *vol, const char *mem)
{
    char    dd[9] = {0};
    char    fn[24];
    FILE    *fp;
    int     rc;

    if (vol) rc = __dsalcf(dd, "DSN=%s;VOLSER=%s;DISP=SHR", dsn, vol);
    else     rc = __dsalcf(dd, "DSN=%s;DISP=SHR", dsn);
    if (rc) return 0;
    snprintf(fn, sizeof(fn), "DD:%s(%s)", dd, mem);
    fp = fopen(fn, "r");
    if (fp) fclose(fp);
    __dsfree(dd);
    return fp != NULL;
}

#ifdef TSTVOLAD_RED

int main(void)
{
    int rc;

    want(onvol(UNC, "PUB001"), "control: " UNC " is on PUB001");
    want(hasmem(PDS, "WORK01", "CCC"), "control: CCC is in the WORK01 copy");

    rc = remove(UNC);
    wtof("TSTVOLAD   remove(UNC) = %d", rc);
    want(rc == 0, "remove() deletes the uncataloged " UNC);

    rc = rename(UNC, UNCR);
    wtof("TSTVOLAD   rename(UNC, UNCR) = %d", rc);
    want(rc == 0, "rename() renames the uncataloged " UNC);

    rc = __delmem(PDS, "CCC");
    wtof("TSTVOLAD   __delmem(PDS, CCC) = %d", rc);
    want(rc == 0, "__delmem() reaches CCC in the uncataloged copy");

    want(onvol(UNC, "PUB001") && hasmem(PDS, "WORK01", "CCC"),
         "control: nothing was changed");
    return bad ? 8 : 0;
}

#else /* GREEN */

int main(void)
{
    char    big[50];
    int     rc;

    want(onvol(UNC, "PUB001"), "control: " UNC " is on PUB001");
    want(onvol(PDS, "WORK00") && onvol(PDS, "WORK01"),
         "control: " PDS " is on WORK00 and WORK01");

    /* RENAME */
    rc = __renamev(UNC, "PUB001", UNCR);
    wtof("TSTVOLAD   __renamev(UNC, PUB001, UNCR) = %d", rc);
    want(rc == 0 && !onvol(UNC, "PUB001") && onvol(UNCR, "PUB001"),
         "__renamev() renames the uncataloged data set");
    errno = 0;
    rc = __renamev(UNC, "PUB001", UNCX);
    wtof("TSTVOLAD   __renamev(UNC, PUB001, UNCX) = %d errno %d", rc, errno);
    want(rc == 8 && errno == ENOENT, "a name not on the volume: 8, ENOENT");
    errno = 0;
    rc = __renamev(UNCR, "PUB001", UNCR);
    wtof("TSTVOLAD   __renamev(UNCR, PUB001, UNCR) = %d errno %d", rc, errno);
    want(rc == 8 && errno == EEXIST, "to its own name: 8, EEXIST");
    errno = 0;
    rc = __renamev(PDS, "WORK01", DUP);
    wtof("TSTVOLAD   __renamev(PDS, WORK01, DUP) = %d errno %d", rc, errno);
    want(rc == 8 && errno == EEXIST && onvol(PDS, "WORK01"),
         "to a name on the volume already: 8, EEXIST, nothing renamed");

    /* the members of the uncataloged copy */
    rc = __delmemv(PDS, "WORK01", "ccc");
    wtof("TSTVOLAD   __delmemv(PDS, WORK01, ccc) = %d", rc);
    want(rc == 0 && !hasmem(PDS, "WORK01", "CCC"),
         "__delmemv() deletes CCC in the WORK01 copy");
    want(__delmemv(PDS, "WORK01", "CCC") == 8, "a second time: 8");
    rc = __renmemv(PDS, "WORK01", "AAA", "ZZZ");
    wtof("TSTVOLAD   __renmemv(PDS, WORK01, AAA, ZZZ) = %d", rc);
    want(rc == 0 && hasmem(PDS, "WORK01", "ZZZ")
         && !hasmem(PDS, "WORK01", "AAA"),
         "__renmemv() renames AAA to ZZZ in the WORK01 copy");
    want(hasmem(PDS, NULL, "AAA") && hasmem(PDS, NULL, "BBB")
         && !hasmem(PDS, NULL, "ZZZ"),
         "the cataloged copy on WORK00 is untouched");
    rc = __delmemv(PDS, "PUB001", "ZZZ");
    wtof("TSTVOLAD   __delmemv(PDS, PUB001, ZZZ) = %d", rc);
    want(rc == -2, "a PDS that is not on the volume: -2");

    /* SCRATCH */
    rc = __scratch(EXP, "WORK01", __SCRATCH_OVRD);
    wtof("TSTVOLAD   __scratch(EXP, WORK01, OVRD) = %d", rc);
    want(rc == 0 && !onvol(EXP, "WORK01"),
         "__SCRATCH_OVRD deletes a data set before its expiration date");

    /* Without the override, SCRATCH compares the expiration date of the
       DSCB with CVTDATE.  On MVS 3.8j after 1999 the DSCB holds the year
       as 26 (X'1A') and CVTDATE as 126, so every date looks past and the
       data set goes: measured, not asserted.  8 with EPERM is the answer
       where the two agree. */
    errno = 0;
    rc = __scratch(EXP2, "WORK01", 0);
    wtof("TSTVOLAD   __scratch(EXP2, WORK01, 0) = %d errno %d", rc, errno);
    want(rc == 0 || (rc == 8 && errno == EPERM),
         "before its expiration date, no override: 0 on 3.8j, or 8 EPERM");
    if (rc == 8) __scratch(EXP2, "WORK01", __SCRATCH_OVRD);
    want(!onvol(EXP2, "WORK01"), "cleanup: " EXP2);

    errno = 0;
    rc = __scratch(UNCR, "WORK01", 0);
    wtof("TSTVOLAD   __scratch(UNCR, WORK01, 0) = %d errno %d", rc, errno);
    want(rc == 8 && errno == ENOENT, "the wrong volume: 8, ENOENT");
    rc = __scratch(UNCR, "PUB001", 0);
    wtof("TSTVOLAD   __scratch(UNCR, PUB001, 0) = %d", rc);
    want(rc == 0 && !onvol(UNCR, "PUB001"),
         "__scratch() deletes the uncataloged data set");

    rc = __scratch(PDS, "WORK01", 0);
    wtof("TSTVOLAD   __scratch(PDS, WORK01, 0) = %d", rc);
    want(rc == 0 && !onvol(PDS, "WORK01") && onvol(PDS, "WORK00"),
         "deletes the WORK01 copy, the cataloged one stays");

    errno = 0;
    rc = __scratch(DUP, "NOSUCH", 0);
    wtof("TSTVOLAD   __scratch(DUP, NOSUCH, 0) = %d errno %d", rc, errno);
    want(rc == -1 && errno == ENODEV, "a volume not online: -1, ENODEV");
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = 0;
    errno = 0;
    want(__scratch(big, "WORK01", 0) == -1 && errno == EINVAL,
         "a name longer than 44: -1, EINVAL");
    errno = 0;
    want(__renamev(DUP, "WORK01", NULL) == -1 && errno == EINVAL,
         "no new name: -1, EINVAL");

    /* the catalog forms, now wrappers of the volume forms */
    want(__renmem(PDS, "AAA", "YYY") == 0 && hasmem(PDS, NULL, "YYY"),
         "__renmem() still renames in the cataloged copy");
    want(__delmem(PDS, "BBB") == 0 && !hasmem(PDS, NULL, "BBB"),
         "__delmem() still deletes in the cataloged copy");
    want(__delmem(PDS, "BBB") == 8, "a second time: 8");

    /* clean up */
    want(__scratch(DUP, "WORK01", 0) == 0, "cleanup: " DUP);
    want(remove(PDS) == 0, "cleanup: the cataloged " PDS);
    return bad ? 8 : 0;
}

#endif
