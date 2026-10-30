/*
 * tstraclv.c - libc370: racf_login() hands out only the ACEE of the user it
 * was asked for (MVS target, batch, APF-AUTHORIZED).
 *
 * After RACINIT answers 0, racf_login() compares the name in the ACEE with
 * the userid it asked for.  The host test (test/host/tstraclv.c) drives the
 * comparison with made-up ACEEs; this runs the real RACINIT, so it covers
 * what the host cannot: the fold to upper case, the password and the
 * PASSCHK=NO path, the name RAKF really puts in the ACEE, and that a
 * rejected ACEE is given back.
 *
 * Cases come from DD:CASES, one per line, '*' starts a comment:
 *
 *     OK     USER PW     login must give an ACEE whose name is USER (upper
 *                        case, blank padded), racf_rc 0; it is logged out
 *     OKNP   USER        the same without a password (PASSCHK=NO)
 *     FAIL   RC USER PW  login must give NULL and racf_rc RC (RC = * means
 *                        any value but 0); the address space's resting
 *                        ACEE (ASXBSENV) must be the same afterwards
 *     FAILNP RC USER     the same without a password
 *     DFLT   [PW]        racf_login(NULL, ...), the default user: printed, no
 *                        verdict (it is not compared)
 *     PROBE  USER [PW]   the login the two cases below use to see where the
 *                        next ACEE lands; it must succeed, so USER is a user
 *                        whose login the system answers with that user's ACEE
 *     LEAK    N USER PW  N logins that must fail (PW - means none); the
 *                        probe must land where it did before
 *     CONTROL N USER PW  N logins that must succeed, kept open: the probe
 *                        must land elsewhere, which shows the instrument sees
 *                        ACEEs that are not given back
 *
 * ACEEs live in LSQA, outside the subpools a task can walk.  The address a
 * probe login gets is the measurement: unchanged after the LEAK logins means
 * they gave everything back, and the CONTROL case shows that it does change
 * when ACEEs are held.
 *
 * AUTHORIZATION and probe hygiene: see test/mvs/tstracfl.c.  AC=1 and no
 * writable statics in this file; clib_apf_setup() authorizes the task and its
 * STEPLIB the way the servers do, so the library need not be in the APF list.
 *
 * BUILD (host):
 *     cc370 -O1 -Iinclude -c test/mvs/tstraclv.c -o tstraclv.o
 *     ld370 --entry @@CRT0 --ac 1 -iebcopy -o TSTRACLV \
 *           tstraclv.o -Lbuild/sdk -lc
 *     ld370 --pack TSTRACLV=TSTRACLV.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mvs/apf.h>
#include "mvs/racf.h"

#define MAXTOK  8
#define MAXKEEP 600

/* Where the next ACEE lands.  ACEEs come from LSQA (subpool 255), which the
** task's own VSM chains do not show.  The address a probe login gets is the
** instrument: with every ACEE given back it is the same every time; ACEEs
** still held change it (the direction depends on the free chain, so only
** "same" and "different" count).  Returns 0 if the probe login fails. */
static unsigned probe_addr(const char *user, const char *pw)
{
    int      rc = 0;
    ACEE     *a = racf_login(user, pw, NULL, &rc);
    unsigned at = (unsigned)a;

    if (a) racf_logout(&a);
    return at;
}

/* the ACEE's name as text, trailing blanks cut */
static void aceename(const ACEE *a, char *out)
{
    int n = 8;

    memcpy(out, a->aceeusri, 8);
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = '\0';
}

static void upper(const char *in, char *out)
{
    int i;

    for (i = 0; i < 8 && in[i]; i++) out[i] = (char)toupper((unsigned char)in[i]);
    out[i] = '\0';
}

static int fail(const char *line, const char *why)
{
    printf("  *** FAIL %s\n      %s\n", why, line);
    fflush(stdout);
    return 1;
}

int main(int argc, char **argv)
{
    char    line[200], orig[200];
    char    *t[MAXTOK];
    char    *p;
    int     n, rc, want, bad = 0, cases = 0, i;
    ACEE    *acee, *senv;
    FILE    *in;
    char    probe_user[9], probe_pw[9];
    int     have_probe = 0, have_pw = 0;

    (void)argc;
    printf("TSTRACLV - racf_login() verifies the ACEE belongs to the user\n");
    printf("  clib_apf_setup rc=%d\n\n", clib_apf_setup(argv[0]));
    fflush(stdout);

    in = fopen("DD:CASES", "r");
    if (!in) {
        printf("cannot open DD:CASES\n");
        return 8;
    }

    while (fgets(line, sizeof(line), in)) {
        n = 0;
        for (p = strtok(line, " \r\n"); p && n < MAXTOK; p = strtok(NULL, " \r\n"))
            t[n++] = p;
        if (n == 0 || t[0][0] == '*') continue;
        sprintf(orig, "%s %s %s %s %s", t[0], n > 1 ? t[1] : "", n > 2 ? t[2] : "",
                n > 3 ? t[3] : "", n > 4 ? t[4] : "");
        cases++;

        if (!strcmp(t[0], "OK") || !strcmp(t[0], "OKNP")) {
            int np = !strcmp(t[0], "OKNP");
            char want_name[9], got_name[9];

            if (n < (np ? 2 : 3)) { bad += fail(orig, "bad case line"); continue; }
            rc = -99;
            acee = racf_login(t[1], np ? NULL : t[2], NULL, &rc);
            if (!acee) {
                printf("  %-8s %-10s -> NULL, racf_rc=%d\n", t[0], t[1], rc);
                bad += fail(orig, "expected an ACEE");
                continue;
            }
            upper(t[1], want_name);
            aceename(acee, got_name);
            printf("  %-8s %-10s -> ACEE for %s, racf_rc=%d\n", t[0], t[1],
                   got_name, rc);
            if (rc != 0) bad += fail(orig, "ACEE returned with racf_rc != 0");
            else if (strcmp(want_name, got_name))
                bad += fail(orig, "the ACEE belongs to another user");
            if (racf_logout(&acee) != 0) bad += fail(orig, "racf_logout failed");
        }
        else if (!strcmp(t[0], "FAIL") || !strcmp(t[0], "FAILNP")) {
            int np = !strcmp(t[0], "FAILNP");

            if (n < (np ? 3 : 4)) { bad += fail(orig, "bad case line"); continue; }
            want = (t[1][0] == '*') ? -1 : atoi(t[1]);
            rc = -99;
            senv = racf_get_acee();
            acee = racf_login(t[2], np ? NULL : t[3], NULL, &rc);
            printf("  %-8s %-10s -> %s, racf_rc=%d\n", t[0], t[2],
                   acee ? "ACEE" : "NULL", rc);
            if (acee) {
                bad += fail(orig, "expected NULL");
                racf_logout(&acee);
            }
            else if (rc == 0 || rc == -99 || (want >= 0 && rc != want))
                bad += fail(orig, "racf_rc is not the expected failure");
            if (racf_get_acee() != senv)
                bad += fail(orig, "ASXBSENV changed by the refused login");
        }
        else if (!strcmp(t[0], "DFLT")) {
            rc = -99;
            acee = racf_login(NULL, n > 1 ? t[1] : NULL, NULL, &rc);
            if (acee) {
                char got_name[9];

                aceename(acee, got_name);
                printf("  %-8s %-10s -> ACEE for %s, racf_rc=%d\n", t[0], "(NULL)",
                       got_name, rc);
                racf_logout(&acee);
            }
            else {
                printf("  %-8s %-10s -> NULL, racf_rc=%d\n", t[0], "(NULL)", rc);
            }
        }
        else if (!strcmp(t[0], "PROBE")) {
            if (n < 2 || strlen(t[1]) > 8 || (n > 2 && strlen(t[2]) > 8)) {
                bad += fail(orig, "bad case line");
                continue;
            }
            strcpy(probe_user, t[1]);
            if (n > 2) { strcpy(probe_pw, t[2]); have_pw = 1; }
            else have_pw = 0;
            have_probe = 1;
            printf("  PROBE    %s\n", t[1]);
        }
        else if (!strcmp(t[0], "ADDR")) {
            printf("  ADDR     probe ACEE at %08X\n", probe_addr(probe_user, have_pw ? probe_pw : NULL));
        }
        else if (!strcmp(t[0], "LEAK") || !strcmp(t[0], "CONTROL")) {
            ACEE    *keep[MAXKEEP];
            unsigned a0, a1;
            int     ctl   = !strcmp(t[0], "CONTROL");
            int     count = (n > 1) ? atoi(t[1]) : 0;
            const char *pw = (n > 3 && strcmp(t[3], "-")) ? t[3] : NULL;
            int     got   = 0;

            if (n < 3 || count < 1 || count > MAXKEEP || !have_probe) {
                bad += fail(orig, "bad case line or no PROBE before it");
                continue;
            }
            a0 = probe_addr(probe_user, have_pw ? probe_pw : NULL);
            if (!a0) {
                bad += fail(orig, "the probe login failed");
                continue;
            }
            for (i = 0; i < count; i++) {
                rc = 0;
                acee = racf_login(t[2], pw, NULL, &rc);
                keep[i] = ctl ? acee : NULL;
                if (acee) {
                    got++;
                    if (!ctl) racf_logout(&acee);
                }
            }
            a1 = probe_addr(probe_user, have_pw ? probe_pw : NULL);
            printf("  %-8s %d logins as %s: %d got an ACEE, probe ACEE at %08X -> %08X\n",
                   t[0], count, t[2], got, a0, a1);
            if (ctl) {
                for (i = 0; i < count; i++) {
                    if (keep[i]) racf_logout(&keep[i]);
                }
                if (got != count) bad += fail(orig, "the control logins must all succeed");
                else if (a1 == a0)
                    bad += fail(orig, "the control did not move the probe: the measurement is blind");
            }
            else {
                if (got) bad += fail(orig, "a login that must fail succeeded");
                if (!a1) bad += fail(orig, "the probe login failed afterwards");
                else if (a1 != a0)
                    bad += fail(orig, "the failed logins left ACEEs behind");
            }
        }
        else {
            bad += fail(orig, "unknown case");
        }
        fflush(stdout);
    }
    fclose(in);

    printf("\n%d cases\nTSTRACLV %s\n", cases, bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
