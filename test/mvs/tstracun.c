/*
 * tstracun.c - libc370 #197: racf_auth() from a caller that is NOT
 * APF-authorized (MVS target, batch, NOT authorized - on purpose).
 *
 * ISSUE #197: racf_auth() issued MODESET KEY=ZERO,MODE=SUP unconditionally
 * before the RACHECK.  From an unauthorized program that is an S047, although
 * SVC 130 itself answers correctly from problem state - RAKF's SVC 130 entry
 * (RAKF SRCLIB/IGC00130.hlasm) copies the plist and issues RACROUTE, with no
 * TESTAUTH anywhere on the way.  So racf_auth() was unusable for exactly the
 * programs that have no other way to ask (brexx370's RACCHECK()).
 *
 * And the MODESET back was unconditional too: KEY=NZERO,MODE=PROB.  A caller
 * that was already in supervisor state came back in problem state.
 *
 * CELLS
 *
 *   (0)  precondition: __isauth() must be 0.  A probe that runs from an APF
 *        library measures nothing here, so that is the one failure that
 *        makes the whole run void (CC 12).
 *   (1)  the reference: a raw SVC 130 from problem state, no MODESET, for
 *        seven resources.  Its answers are checked against the values
 *        measured in #197 (JOB00486/00488) - a control with a known value,
 *        so a stand whose profiles moved says so instead of passing.
 *   (2)  racf_auth(NULL, ...) unauthorized, problem state, same seven.  Must
 *        equal (1) and must not abend.  RED before the fix: S047.
 *   (3)  SVC 244 on (JSCBAUTH), racf_auth(NULL, ...) - the authorized path,
 *        which still MODESETs.  Must equal (1), and the caller must be back
 *        in problem state afterwards.
 *   (4)  supervisor-state caller: SVC 244 on, __super(), racf_auth(), and the
 *        caller must STILL be in supervisor state.  RED before the fix.
 *   (5)  a foreign ACEE from problem state - the httpd/ftpd shape that #197
 *        left unmeasured.  SVC 244 on, racf_login() the non-admin user with
 *        PASSCHK=NO, SVC 244 off, then racf_auth(acee, ...) unauthorized.
 *        FACILITY LIBC370.TSTRACMX.ALLOW answers 8 for IBMUSER (not in group
 *        USER) and 0 for MVSCE02 (in it), so the answer says whose ACEE RAKF
 *        used.  REPORTED, not gated: it answers a question for a follow-up,
 *        it does not decide this fix.
 *
 * Cells (3)-(5) need SVC 244.  Where it is missing or refused they abend
 * inside try() and report SKIPPED; (0)-(2) are the gate and need nothing.
 *
 * FIXTURE: the profiles tstracmx.c documents in SYS1.SECURE.CNTL(PROFILES),
 * plus the userid MVSCE02 in group USER - all present on mvsdev.  The names
 * are constants here because the reference values in (1) belong to them.
 *
 * WHY STATICS: try() returns only the abend code, so the callee's rc comes
 * back through a static.  This module is STEPLIBbed from a private library,
 * never fetched from the LNKLST, so its statics are writable.
 *
 * BUILD (host) - no AC=1, that is the point:
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstracun.c -o TSTRACUN \
 *           -flinker-output=iebcopy
 *     ld370 --pack TSTRACUN=TSTRACUN.iebcopy -o tstracun -xmit \
 *           --dsn IBMUSER.LIBC370.RACUNLIB
 * Upload tstracun.xmit to IBMUSER.LIBC370.RACUNXMI (FB 80) and submit
 * jcl/recvracu.jcl, then jcl/tstracun.jcl.  IBMUSER.LIBC370.RACUNLIB must
 * NOT be in IEAAPF00.
 *
 * RC:  0 every gated cell passed
 *      4 the stand's answers moved (a reference value in (1) differs)
 *      8 racf_auth() abended, disagreed with SVC 130, or left the caller in
 *        the wrong state
 *     12 the probe ran authorized - nothing was measured
 */
#include <stdio.h>
#include <string.h>
#include "mvs/racf.h"
#include "clibos.h"
#include "mvs/apf.h"
#include "mvs/recovery.h"
#include "mvs/wto.h"

#define FIXUSER "MVSCE02"

typedef struct {
    const char *cls;
    const char *res;
    int         attr;
    const char *an;
    int         ref;        /* measured for IBMUSER in #197 */
} CELL;

static const CELL cells[] = {
    { "FACILITY", "SVC244",                 RACHECK_ATTR_READ,   "READ",   0 },
    { "FACILITY", "SVC244",                 RACHECK_ATTR_UPDATE, "UPDATE", 8 },
    { "FACILITY", "BRXALLAUTH",             RACHECK_ATTR_READ,   "READ",   8 },
    { "FACILITY", "LIBC370.TSTRACMX.DENY",  RACHECK_ATTR_READ,   "READ",   8 },
    { "FACILITY", "LIBC370.TSTRACMX.ALLOW", RACHECK_ATTR_READ,   "READ",   8 },
    { "FACILITY", "NO.SUCH.PROFILE.XYZ",    RACHECK_ATTR_READ,   "READ",   4 },
    { "DATASET",  "LIBC370.RACTEST.DENY",   RACHECK_ATTR_READ,   "READ",   0 },
};
#define NCELLS  (sizeof(cells) / sizeof(cells[0]))

/* SVC 130 with the plist racf_auth() builds (LOG=NONE), from whatever state
** the caller is in - no MODESET.  This is what brexx370's RACCHECK() does. */
static int svc130(ACEE *acee, const char *cls, const char *res, int attr)
{
    RACHECK  plist;
    RACLASS  cclass;
    char     resname[80];
    unsigned addr;
    int      rc;
    size_t   len;

    memset(&plist, 0, sizeof(plist));
    memset(cclass.name, ' ', sizeof(cclass.name));
    memset(resname, ' ', sizeof(resname));

    len = strlen(cls);
    cclass.len = (char) len;
    memcpy(cclass.name, cls, len);
    memcpy(resname, res, strlen(res));

    plist.len   = sizeof(plist);
    plist.flag1 = RACHECK_FLAG1_LOG_NONE;
    addr = (unsigned) resname;
    memcpy(plist.entity, ((char *) &addr) + 1, 3);
    plist.attr  = (char) attr;
    addr = (unsigned) &cclass;
    memcpy(plist.aclass, ((char *) &addr) + 1, 3);
    plist.acee  = acee;

    __asm__("\n"
            "         LR    1,%1\n"
            "         SVC   130\n"
            "         ST    15,%0"
            : "=m"(rc) : "r"(&plist) : "0", "1", "14", "15");
    return rc;
}

static void svc244(int on)
{
    __asm__("\n"
            "         SLR   0,0\n"
            "         LR    1,%0\n"
            "         SVC   244"
            : : "r"(on) : "0", "1", "14", "15");
}

/* try() targets and their results */
static const CELL *t_cell;
static ACEE       *t_acee;
static int         t_rc;
static int         t_sup_after;
static ACEE       *t_login;
static int         t_login_rc;

static void do_auth(void)
{
    t_rc = racf_auth(t_acee, t_cell->cls, t_cell->res, t_cell->attr);
}

static void do_on(void)  { svc244(1); }
static void do_off(void) { svc244(0); }

static void do_sup_auth(void)
{
    t_sup_after = -1;
    if (__super(PSWKEYNONE, NULL)) return;
    t_rc = racf_auth(NULL, t_cell->cls, t_cell->res, t_cell->attr);
    t_sup_after = __issup();
    __prob(PSWKEYNONE, NULL);
}

static void do_login(void)
{
    t_login = racf_login(FIXUSER, NULL, NULL, &t_login_rc);
}

static void do_logout(void)
{
    racf_logout(&t_login);
}

/* one racf_auth() under try(); returns the abend code, rc in t_rc */
static unsigned auth(ACEE *acee, const CELL *c)
{
    t_cell = c;
    t_acee = acee;
    t_rc   = -99;
    return (unsigned) try(do_auth, 0);
}

int main(void)
{
    int      worst = 0;
    int      raw[NCELLS];
    unsigned ab;
    size_t   i;

    printf("TSTRACUN - libc370 #197: racf_auth() without APF\n\n");

    /* (0) */
    printf("(0) __isauth()=%d __issup()=%d\n", __isauth(), __issup());
    if (__isauth()) {
        printf("*** the probe runs APF-authorized - nothing is measured\n");
        wtof("TSTRACUN VOID: RAN AUTHORIZED");
        return 12;
    }

    /* (1) */
    printf("\n(1) raw SVC 130, problem state, no MODESET\n");
    for (i = 0; i < NCELLS; i++) {
        raw[i] = svc130(NULL, cells[i].cls, cells[i].res, cells[i].attr);
        printf("    %-8s %-24s %-6s rc=%d%s\n", cells[i].cls, cells[i].res,
               cells[i].an, raw[i],
               raw[i] == cells[i].ref ? "" : "  *** reference moved");
        if (raw[i] != cells[i].ref && worst < 4) worst = 4;
    }
    wtof("TSTRACUN C1 RAW %d %d %d %d %d %d %d", raw[0], raw[1], raw[2],
         raw[3], raw[4], raw[5], raw[6]);

    /* (2) - the gate */
    printf("\n(2) racf_auth(NULL,...), not authorized, problem state\n");
    for (i = 0; i < NCELLS; i++) {
        int bad;

        ab  = auth(NULL, &cells[i]);
        bad = ab != 0 || t_rc != raw[i];
        printf("    %-8s %-24s %-6s abend=%06X rc=%d%s\n", cells[i].cls,
               cells[i].res, cells[i].an, ab, t_rc,
               bad ? "  *** FAIL" : "");
        wtof("TSTRACUN C2 %u ABEND=%06X RC=%d", (unsigned) i, ab, t_rc);
        if (bad) worst = 8;
        if (ab) break;      /* one S047 says it all */
    }

    /* (3) - authorized path */
    printf("\n(3) SVC 244 on, racf_auth(NULL,...)\n");
    ab = (unsigned) try(do_on, 0);
    if (ab || !__isauth()) {
        printf("    SKIPPED - SVC 244 abend=%06X __isauth()=%d\n", ab,
               __isauth());
    }
    else {
        for (i = 0; i < NCELLS; i++) {
            int bad;
            int sup;

            ab  = auth(NULL, &cells[i]);
            sup = __issup();
            bad = ab != 0 || t_rc != raw[i] || sup;
            printf("    %-8s %-24s %-6s abend=%06X rc=%d sup-after=%d%s\n",
                   cells[i].cls, cells[i].res, cells[i].an, ab, t_rc, sup,
                   bad ? "  *** FAIL" : "");
            wtof("TSTRACUN C3 %u ABEND=%06X RC=%d SUP=%d", (unsigned) i, ab,
                 t_rc, sup);
            if (bad) worst = 8;
        }

        /* (4) - supervisor-state caller */
        printf("\n(4) supervisor-state caller, racf_auth(NULL, %s %s)\n",
               cells[0].cls, cells[0].res);
        t_cell = &cells[0];
        t_rc   = -99;
        ab = (unsigned) try(do_sup_auth, 0);
        printf("    abend=%06X rc=%d still-sup=%d%s\n", ab, t_rc, t_sup_after,
               ab || t_rc != raw[0] || t_sup_after != 1 ? "  *** FAIL" : "");
        wtof("TSTRACUN C4 ABEND=%06X RC=%d SUP=%d", ab, t_rc, t_sup_after);
        if (ab || t_rc != raw[0] || t_sup_after != 1) worst = 8;
        if (__issup()) {
            printf("    *** still in supervisor state after the cell\n");
            __prob(PSWKEYNONE, NULL);
            worst = 8;
        }

        /* (5) - foreign ACEE from problem state, reported */
        printf("\n(5) racf_login(\"%s\") authorized, racf_auth(acee,...) "
               "NOT authorized\n", FIXUSER);
        t_login = NULL;
        t_login_rc = -99;
        ab = (unsigned) try(do_login, 0);
        printf("    racf_login abend=%06X acee=%08X racf_rc=%d\n", ab,
               (unsigned) t_login, t_login_rc);
        try(do_off, 0);
        printf("    SVC 244 off: __isauth()=%d\n", __isauth());

        if (t_login && !__isauth()) {
            size_t pick[2];
            size_t k;

            pick[0] = 3;    /* LIBC370.TSTRACMX.DENY  */
            pick[1] = 4;    /* LIBC370.TSTRACMX.ALLOW */
            for (k = 0; k < 2; k++) {
                const CELL *c = &cells[pick[k]];
                int         r;

                r  = svc130(t_login, c->cls, c->res, c->attr);
                ab = auth(t_login, c);
                printf("    %-24s raw=%d racf_auth abend=%06X rc=%d "
                       "(own ACEE: %d)\n", c->res, r, ab, t_rc, raw[pick[k]]);
                wtof("TSTRACUN C5 %s RAW=%d ABEND=%06X RC=%d OWN=%d", c->res,
                     r, ab, t_rc, raw[pick[k]]);
                /* the library must at least agree with SVC 130 */
                if (ab || t_rc != r) worst = 8;
            }
        }
        else {
            printf("    SKIPPED - no ACEE\n");
        }

        if (t_login) {
            try(do_on, 0);
            try(do_logout, 0);
        }
    }
    try(do_off, 0);

    printf("\nTSTRACUN %s (rc=%d) __isauth()=%d __issup()=%d\n",
           worst == 0 ? "PASSED" : worst == 4 ? "REFERENCE MOVED" : "FAILED",
           worst, __isauth(), __issup());
    wtof("TSTRACUN RC=%d", worst);
    return worst;
}
