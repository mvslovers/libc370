/*
 * tstracmx.c - libc370 #63: what the RACHECK flag1 bit actually does, across
 * every constellation that matters (MVS target, batch, APF-AUTHORIZED).
 *
 * ISSUE #63: racf_auth() sets flag1 = 0x10 under the name
 * RACHECK_FLAG1_LOG_NONE.  0x10 is DSTYPE=V - "the entity is a VSAM data set"
 * (sysmac/racheck.macro:562).  LOG=NONE is 0x02.  So the audit suppression the
 * library asks for never happens, and every CLASS=DATASET check it issues
 * tells RACF something untrue about the resource.
 *
 * Setting the correct bit is not a one-character fix: 0x00 and 0x02 differ
 * only in the LOG bit, and a FACILITY resource with no profile answers rc 0
 * for the first and rc 4 for the second.  The consumers have been taught to
 * accept both (ftpd#82, httpd#135, both closed).  What is left is the
 * question this probe exists to answer:
 *
 *     DOES SUPPRESSING THE AUDIT ALSO SOFTEN A DENIAL?
 *
 * If a user who is genuinely not permitted gets anything other than 8 with
 * LOG=NONE set, the change is off - that is a security defect, not a logging
 * preference.  Cells (3) and (6) below are that question, and they are the
 * gate on #63.  Everything else here is behaviour we can adapt to.
 *
 * ====================================================================
 * WHAT DECIDES WHAT
 * --------------------------------------------------------------------
 * COND CODE 8  a denial stopped being a denial, or the RACHECK did not run.
 *              #63 does not proceed.
 * COND CODE 4  a permitted resource stopped answering 0.  Not a security
 *              defect, but it moves the consumer contract a second time and
 *              wants a decision before the bit is flipped.
 * COND CODE 0  every cell that could run behaved.
 *
 * THE AUDIT COUNT IS NOT IN THE COND CODE.  A program cannot read the console
 * it writes to, so each measurement is bracketed by two WTOs of its own and
 * the RAKF messages are counted between them in JESMSGLG - the technique from
 * #43 and #59:
 *
 *     zowe jobs submit lf jcl/tstracmx.jcl --wait-for-output --directory out
 *     grep -c RAKF out/<jobid>/JES2/JESMSGLG.txt
 *     awk '/TSTRACMX: c3 f02 open/,/TSTRACMX: c3 f02 close/' \
 *         out/<jobid>/JES2/JESMSGLG.txt
 *
 * An APF-authorized program's WTO appears WITHOUT the '+' prefix, which is
 * also how to tell the AC took.
 * ====================================================================
 *
 * FIXTURE - most cells cannot run without one, and the probe cannot create it
 * ---------------------------------------------------------------------------
 * RAKF profiles and a second userid are system setup, the way tststow.c needs
 * a PDS that already exists.  The probe reads them from DD FIXTURE, one
 * "KEYWORD value" per line, and every cell whose keywords are missing reports
 * SKIPPED instead of failing.  Values must be UPPERCASE; * starts a comment.
 *
 *   FACOK  <profile>   FACILITY profile the fixture user IS permitted to READ
 *   FACNO  <profile>   FACILITY profile the fixture user is NOT permitted to
 *   DSNOK  <dsn>       DATASET profile, fixture user permitted READ but NOT
 *                      UPDATE (cell 7 needs the UPDATE half)
 *   DSNNO  <dsn>       DATASET profile the fixture user is NOT permitted to
 *   USER   <userid>    a userid that is NOT an administrator.  IBMUSER is no
 *                      use here: it is permitted to everything, so cells (3)
 *                      and (6) could never deny it and the gate would pass
 *                      without having been tested.
 *   PASS   <password>  for USER
 *   GROUP  <group>     optional, passed to racf_login()
 *   FACNP  <profile>   OPTIONAL: a FACILITY name with no profile.  Defaults
 *   DSNNP  <dsn>       OPTIONAL: a DATASET name with no profile.   below.
 *
 * PASS lands in the job log if the DD is instream - JESJCL carries the SYSIN.
 * Point FIXTURE at a protected data set instead if that matters on your
 * system; the JCL ships with a placeholder either way.
 *
 * On the reference system the fixture is six lines added to RAKF's
 * SYS1.SECURE.CNTL(PROFILES) - class in columns 1-8, resource in 9-52, group
 * or userid in 53-60, access from 61 - plus MVSCE02, which was already there
 * in group USER and is the only non-administrator on the system (IBMUSER, MVP
 * and MVSCE01 are all ADMIN and RAKFADM, so none of them can be denied
 * anything and all four gate cells would pass untested against them):
 *
 *     DATASET LIBC370.RACTEST.ALLOW                               NONE
 *     DATASET LIBC370.RACTEST.ALLOW                       USER    READ
 *     DATASET LIBC370.RACTEST.DENY                                NONE
 *     FACILITYLIBC370.TSTRACMX.ALLOW                              NONE
 *     FACILITYLIBC370.TSTRACMX.ALLOW                      USER    READ
 *     FACILITYLIBC370.TSTRACMX.DENY                               NONE
 *
 * RAKF re-reads the member when it is restarted (S RAKF).  The unqualified
 * line grants the default and the group line grants the exception, which is
 * the pattern the file already uses for FACILITY BRXCONSAUTH and friends.
 * Granting only READ on the ALLOW data set profile is deliberate: cell (7)
 * needs a resource the user may read and may not update.
 *
 * The five flag1 values are swept for every cell: 0x00 (nothing), 0x10 (what
 * the library sets today, DSTYPE=V), 0x02 (LOG=NONE, what #63 would set),
 * 0x04 (LOG=NOFAIL), and 0x12 (both, to separate the two bits' effects).
 *
 * AUTHORIZATION: this probe issues MODESET KEY=ZERO,MODE=SUP itself, so the
 * module must be linked AC=1 and run from an APF-authorized library or it
 * S047s with an EMPTY SYSPRINT.  cc370 silently drops -Wl,--ac,1 and ld370
 * --pack loses it again, so the AC goes to ld370 TWICE (mvslovers/cc370#37):
 *
 *     cc370 -O1 -Iinclude -c test/mvs/tstracmx.c -o tstracmx.o
 *     ld370 --entry @@CRT0 --ac 1 -o TSTRACMX build/sdk/crt0.o tstracmx.o \
 *           -Lbuild/sdk -lc
 *     ld370 --pack TSTRACMX=TSTRACMX --ac 1 -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.PROBE.LINKLIB
 *
 * Install: RECEIVE into a scratch library, IEBCOPY the member into the APF
 * library, delete it again afterwards.  Never RECEIVE over the APF library
 * itself - RECEIVE does not merge and wants the target deleted first.
 *
 * A module fetched from the LNKLST cannot STORE into its own writable
 * statics (doc/consumer-notes.md), so this file has none: counters live in
 * main()'s frame and every routine returns its verdict.
 */
#include <stdio.h>
#include <string.h>
#include "racf.h"
#include "clibwto.h"

#define FIXLEN  45          /* longest fixture value: a 44-byte DSN + NUL   */
#define NFLAGS  5

/* Cells that need no fixture: a name nothing can plausibly have a profile
** for.  Overridable, because "plausibly" is a guess about someone else's
** system and generic profiles exist. */
#define DEF_FACNP   "LIBC370.TSTRACMX.NOSUCHRES"
#define DEF_DSNNP   "NOSUCH.TSTRACMX.DATASET"

typedef struct {
    char facok[FIXLEN];
    char facno[FIXLEN];
    char dsnok[FIXLEN];
    char dsnno[FIXLEN];
    char facnp[FIXLEN];
    char dsnnp[FIXLEN];
    char user[9];
    char pass[9];
    char group[9];
} FIXTURE;

/* What the cell asserts about its rc.  Only DENY is a gate. */
#define EXP_REPORT  0       /* measure and print; no verdict               */
#define EXP_PERMIT  1       /* must answer 0 -- warning (CC 4) if it moves */
#define EXP_DENY    2       /* must answer 8 -- GATE (CC 8) if it moves    */

typedef struct {
    int         no;
    const char *classname;
    int         which;      /* index of the resource field, see resource() */
    int         attr;
    int         acee_mode;  /* 0 = runner, 1 = fixture user, 2 = ASXBSENV  */
    int         expect;
    const char *what;
} CELL;

#define R_FACNP 0
#define R_FACOK 1
#define R_FACNO 2
#define R_DSNNP 3
#define R_DSNOK 4
#define R_DSNNO 5

static const char *resource(const FIXTURE *f, int which)
{
    switch (which) {
    case R_FACNP: return f->facnp;
    case R_FACOK: return f->facok;
    case R_FACNO: return f->facno;
    case R_DSNNP: return f->dsnnp;
    case R_DSNOK: return f->dsnok;
    case R_DSNNO: return f->dsnno;
    }
    return "";
}

/* ---------------------------------------------------------------------------
 * The RACHECK, with flag1 under the caller's control.  This is racf_auth()
 * (src/racf/racauth.c) with two differences and no others: flag1 is a
 * parameter instead of a constant, and there is no attr defaulting - a probe
 * that quietly substituted READ for what it was asked to check would measure
 * the wrong cell.  Keep it in step with racauth.c by hand; a divergence here
 * measures something the library does not do.
 * ------------------------------------------------------------------------ */
static int racheck_raw(ACEE *acee, const char *classname, const char *res,
                       int attr, int flag1)
{
    int     rc  = 0;
    int     len;
    RACLASS cclass;
    char    resname[80];
    RACHECK plist;

    memset(cclass.name, ' ', sizeof(cclass.name));
    memset(resname, ' ', sizeof(resname));
    memset(&plist, 0, sizeof(plist));

    len = strlen(classname);
    if (len > sizeof(cclass.name)) len = sizeof(cclass.name);
    cclass.len = len;
    memcpy(cclass.name, classname, len);

    len = strlen(res);
    if (len > sizeof(resname)) len = sizeof(resname);
    memcpy(resname, res, len);

    plist.flag1 = (char)flag1;
    plist.len   = sizeof(plist);
    plist.acee  = acee;

    __asm__("\n"
            "*\n"
            "* enter supervisor state\n"
            "*\n"
            "         MODESET KEY=ZERO,MODE=SUP\n"
            : : : "1", "14", "15");

    __asm__("\n"
            "*\n"
            "* check access to resource\n"
            "*\n"
            "         RACHECK ENTITY=((%1)),CLASS=(%2),ATTR=(%3),MF=(E,%4)\n"
            "         ST    15,%0"
            : "=m"(rc)
            : "r"(resname), "r"(&cclass), "r"(attr), "m"(plist)
            : "1", "14", "15");

    __asm__("\n"
            "*\n"
            "* return to problem state\n"
            "*\n"
            "         MODESET KEY=NZERO,MODE=PROB\n"
            : : : "1", "14", "15");

    return rc;
}

/* --------------------------------------------------------------------------
 * Fixture file.  "KEYWORD value", one per line; blank lines and lines
 * starting with '*' ignored.  Unknown keywords are reported rather than
 * ignored - a typo in a fixture that then silently SKIPs a gate cell is the
 * failure mode worth spending four lines on.
 * ------------------------------------------------------------------------ */
static void copyval(char *dst, unsigned max, const char *src)
{
    unsigned i = 0;

    while (*src == ' ') src++;
    while (i < max - 1 && src[i] > ' ') { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int load_fixture(FIXTURE *f)
{
    FILE *fp;
    char  buf[120];
    int   n = 0;

    memset(f, 0, sizeof(*f));
    copyval(f->facnp, FIXLEN, DEF_FACNP);
    copyval(f->dsnnp, FIXLEN, DEF_DSNNP);

    fp = fopen("dd:FIXTURE", "r");
    if (!fp) {
        printf("  DD FIXTURE not allocated - only the no-profile cells run\n");
        return 0;
    }

    while (fgets(buf, sizeof(buf), fp)) {
        char *v = buf;

        while (*v == ' ') v++;
        if (*v == 0 || *v == '\n' || *v == '*') continue;

        /* split off the keyword */
        while (*v > ' ') v++;
        if (*v) *v++ = 0;

        if      (!strcmp(buf, "FACOK")) copyval(f->facok, FIXLEN, v);
        else if (!strcmp(buf, "FACNO")) copyval(f->facno, FIXLEN, v);
        else if (!strcmp(buf, "DSNOK")) copyval(f->dsnok, FIXLEN, v);
        else if (!strcmp(buf, "DSNNO")) copyval(f->dsnno, FIXLEN, v);
        else if (!strcmp(buf, "FACNP")) copyval(f->facnp, FIXLEN, v);
        else if (!strcmp(buf, "DSNNP")) copyval(f->dsnnp, FIXLEN, v);
        else if (!strcmp(buf, "USER"))  copyval(f->user,  9, v);
        else if (!strcmp(buf, "PASS"))  copyval(f->pass,  9, v);
        else if (!strcmp(buf, "GROUP")) copyval(f->group, 9, v);
        else { printf("  FIXTURE: unknown keyword \"%s\" ignored\n", buf);
               continue; }
        n++;
    }

    fclose(fp);
    return n;
}

/* --------------------------------------------------------------------------
 * One cell: sweep the five flag1 values, bracket each with markers, report.
 * Returns 8 if a gate moved, 4 if a permit moved, 0 otherwise.
 * ------------------------------------------------------------------------ */
static int run_cell(const CELL *c, const FIXTURE *f, ACEE *acee)
{
    int flags[NFLAGS];
    int i;
    int worst = 0;

    flags[0] = 0x00; flags[1] = 0x10; flags[2] = 0x02;
    flags[3] = 0x04; flags[4] = 0x12;

    printf("\n  (%d) %s\n      class=%s resource=%s attr=%02X\n",
           c->no, c->what, c->classname, resource(f, c->which), c->attr);
    fflush(stdout);

    for (i = 0; i < NFLAGS; i++) {
        int rc;
        int verdict = 0;

        wtof("TSTRACMX: c%d f%02X open", c->no, flags[i]);
        rc = racheck_raw(acee, c->classname, resource(f, c->which),
                         c->attr, flags[i]);
        wtof("TSTRACMX: c%d f%02X close rc=%d", c->no, flags[i], rc);

        if      (c->expect == EXP_DENY   && rc != 8) verdict = 8;
        else if (c->expect == EXP_PERMIT && rc != 0) verdict = 4;
        if (verdict > worst) worst = verdict;

        printf("      flag1=%02X  rc=%-3d %s\n", flags[i], rc,
               verdict == 8 ? "*** GATE: a denial stopped denying" :
               verdict == 4 ? "*** permitted resource no longer answers 0" :
                              "");
        fflush(stdout);
    }

    return worst;
}

int main(void)
{
    FIXTURE  f;
    ACEE    *user   = NULL;
    ACEE    *saved  = NULL;
    int      login_rc = 0;
    int      worst  = 0;
    int      verdict;
    int      i;
    int      ncells;

    /* The matrix.  which/acee_mode are resolved per cell below. */
    CELL cells[8];

    printf("TSTRACMX - libc370 #63: RACHECK flag1 across the matrix\n\n");
    fflush(stdout);

    load_fixture(&f);

    /* Cells 1 and 4 need nothing; the rest need the fixture user. */
    cells[0].no = 1; cells[0].classname = "FACILITY"; cells[0].which = R_FACNP;
    cells[0].attr = RACHECK_ATTR_READ; cells[0].acee_mode = 0;
    cells[0].expect = EXP_REPORT;
    cells[0].what = "FACILITY, no profile, READ, runner's identity";

    cells[1].no = 2; cells[1].classname = "FACILITY"; cells[1].which = R_FACOK;
    cells[1].attr = RACHECK_ATTR_READ; cells[1].acee_mode = 1;
    cells[1].expect = EXP_PERMIT;
    cells[1].what = "FACILITY, defined, user permitted, READ";

    cells[2].no = 3; cells[2].classname = "FACILITY"; cells[2].which = R_FACNO;
    cells[2].attr = RACHECK_ATTR_READ; cells[2].acee_mode = 1;
    cells[2].expect = EXP_DENY;
    cells[2].what = "FACILITY, defined, user NOT permitted, READ  <- GATE";

    cells[3].no = 4; cells[3].classname = "DATASET"; cells[3].which = R_DSNNP;
    cells[3].attr = RACHECK_ATTR_READ; cells[3].acee_mode = 0;
    cells[3].expect = EXP_REPORT;
    cells[3].what = "DATASET, no profile, READ (ftpd's data path)";

    cells[4].no = 5; cells[4].classname = "DATASET"; cells[4].which = R_DSNOK;
    cells[4].attr = RACHECK_ATTR_READ; cells[4].acee_mode = 1;
    cells[4].expect = EXP_PERMIT;
    cells[4].what = "DATASET, defined, user permitted, READ";

    cells[5].no = 6; cells[5].classname = "DATASET"; cells[5].which = R_DSNNO;
    cells[5].attr = RACHECK_ATTR_READ; cells[5].acee_mode = 1;
    cells[5].expect = EXP_DENY;
    cells[5].what = "DATASET, defined, user NOT permitted, READ   <- GATE";

    cells[6].no = 7; cells[6].classname = "DATASET"; cells[6].which = R_DSNOK;
    cells[6].attr = RACHECK_ATTR_UPDATE; cells[6].acee_mode = 1;
    cells[6].expect = EXP_DENY;
    cells[6].what = "DATASET, permitted READ only, asked for UPDATE <- GATE";

    /* (8) the ASXBSENV fallback: same denial as (3), but the ACEE arrives
           through the field racf_auth() used to poke instead of the plist. */
    cells[7].no = 8; cells[7].classname = "FACILITY"; cells[7].which = R_FACNO;
    cells[7].attr = RACHECK_ATTR_READ; cells[7].acee_mode = 2;
    cells[7].expect = EXP_DENY;
    cells[7].what = "FACILITY, NOT permitted, ACEE via ASXBSENV    <- GATE";

    ncells = 8;

    if (f.user[0] && f.pass[0]) {
        user = racf_login(f.user, f.pass, f.group[0] ? f.group : NULL,
                          &login_rc);
        printf("  racf_login(\"%s\") acee=%08X racf_rc=%d\n",
               f.user, (unsigned)user, login_rc);
    }
    else {
        printf("  no USER/PASS in the fixture - every cell needing an\n"
               "  unprivileged identity is SKIPPED\n");
    }
    fflush(stdout);

    for (i = 0; i < ncells; i++) {
        const CELL *c   = &cells[i];
        const char *res = resource(&f, c->which);
        ACEE       *use = NULL;

        if (!res[0]) {
            printf("\n  (%d) %s\n      SKIPPED - fixture keyword missing\n",
                   c->no, c->what);
            continue;
        }
        if (c->acee_mode != 0 && !user) {
            printf("\n  (%d) %s\n      SKIPPED - no fixture user\n",
                   c->no, c->what);
            continue;
        }

        if (c->acee_mode == 1) {
            use = user;
        }
        else if (c->acee_mode == 2) {
            /* park the fixture ACEE where RACHECK falls back to it, and pass
               NULL in the plist so it has to */
            saved = racf_set_acee(user);
            use   = NULL;
        }

        verdict = run_cell(c, &f, use);
        if (verdict > worst) worst = verdict;

        if (c->acee_mode == 2) racf_set_acee(saved);
    }

    /* ----------------------------------------------------------------------
     * The library's own entry point.  Everything above measures a RACHECK
     * this file issues; these two measure the one racf_auth() issues, which
     * is what consumers actually meet.  They are the before/after of the flag
     * flip: with 0x10 an undefined FACILITY resource answers 0, with 0x02 it
     * answers 4 -- and a denial answers 8 either way, which is the only part
     * that was ever allowed to stay put.
     * ------------------------------------------------------------------- */
    if (user) {
        int rc;

        printf("\n  (9) racf_auth(), FACILITY with no profile\n");
        wtof("TSTRACMX: c9 auth open");
        rc = racf_auth(user, "FACILITY", f.facnp, RACHECK_ATTR_READ);
        wtof("TSTRACMX: c9 auth close rc=%d", rc);
        printf("      rc=%-3d %s\n", rc,
               rc == 0 ? "(0x10 in the library: 'permitted')" :
               rc == 4 ? "(0x02 in the library: 'not protected')" :
                         "*** neither 0 nor 4");
        if (rc != 0 && rc != 4) worst = 8;

        if (f.facno[0]) {
            printf("  (10) racf_auth(), FACILITY the user may not use  <- GATE\n");
            wtof("TSTRACMX: c10 auth open");
            rc = racf_auth(user, "FACILITY", f.facno, RACHECK_ATTR_READ);
            wtof("TSTRACMX: c10 auth close rc=%d", rc);
            printf("      rc=%-3d %s\n", rc,
                   rc == 8 ? "ok" : "*** GATE: the library stopped denying");
            if (rc != 8) worst = 8;
        }
        fflush(stdout);
    }

    if (user) racf_logout(&user);

    printf("\nTSTRACMX %s\n",
           worst == 8 ? "GATE FAILED - #63 does not proceed" :
           worst == 4 ? "permitted-resource contract moved - decide first" :
                        "clean");
    printf("  The audit counts are not here.  Count RAKF lines between the\n"
           "  TSTRACMX markers in JESMSGLG, per cell and flag value.\n");

    return worst;
}
