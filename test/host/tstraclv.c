/*
 * tstraclv.c - libc370: racf_login() hands out only the ACEE of the user it
 * was asked for.
 *
 * After RACINIT answers 0, racf_login() compares the name in the ACEE with
 * the userid RACINIT was given (blank padded to 8, upper case).  A different
 * name: the ACEE is deleted through racf_logout(), the result is NULL and the
 * return code 8.  The comparison and the delete are verify_acee() in
 * raclogin.c; this #includes the real file with the inline assembler compiled
 * out and drives that function with made-up ACEEs, racf_logout() being a
 * recorder and racf_get_acee() answering with whatever the test puts in
 * asxbsenv.  The RACINIT call itself, the fold to upper case and the
 * password and PASSCHK=NO paths are asm and are measured on MVS
 * (test/mvs/tstraclv.c).
 *
 * No ACEE that ASXBSENV points at is handed back or deleted: ASXBSENV is
 * read before RACINIT (the `own` argument) and again after it; an ACEE equal
 * to either value gives NULL and rc 8 without a racf_logout(), whatever its
 * name and also when no name is compared (user NULL).  The cases at the end
 * cover that decision by the pointer: the ACEE equal to the value before,
 * equal to the value after, equal to neither, and ASXBSENV empty.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstraclv.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include "../../src/mvs/racf/raclogin.c"

static int  logouts = 0;
static ACEE *logged_out = NULL;
static ACEE *asxbsenv = NULL;   /* what racf_get_acee() answers */

int racf_logout(ACEE **acee)
{
    logouts++;
    logged_out = *acee;
    *acee = NULL;
    return 0;
}

ACEE *racf_get_acee(void)
{
    return asxbsenv;
}

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

/* the userid as racf_login() builds it: length byte, 8 blank padded */
static void mkuser(char userid[9], const char *name)
{
    int i, n = 0;

    while (name[n]) n++;
    if (n > 8) n = 8;
    userid[0] = (char)n;
    for (i = 0; i < 8; i++) userid[1 + i] = i < n ? name[i] : ' ';
}

static void mkacee(ACEE *a, const char *padded8)
{
    int i;

    for (i = 0; i < (int)sizeof(*a); i++) ((char *)a)[i] = 0;
    for (i = 0; i < 8; i++) a->aceeusri[i] = padded8[i];
    a->aceeusrl = 8;
}

static ACEE *own = NULL;        /* the `own` argument: ASXBSENV before RACINIT */

/* one call: returns the rc, leaves *got, logouts and logged_out for checks.
** want NULL: the name is not compared (user NULL). */
static int verify(const char *want, const char *aceename, int rc_in,
                  ACEE *acee, ACEE **got)
{
    char userid[9];

    if (want) mkuser(userid, want);
    mkacee(acee, aceename);
    *got = acee;
    logouts = 0;
    logged_out = NULL;
    return verify_acee(got, want ? userid : NULL, rc_in, own);
}

int main(void)
{
    ACEE    a, *got;
    int     rc;

    rc = verify("IBMUSER", "IBMUSER ", 0, &a, &got);
    check(rc == 0 && got == &a && logouts == 0,
          "same name: ACEE kept, rc 0, no delete");

    rc = verify("IBMUSER", "STC     ", 0, &a, &got);
    check(rc == 8 && got == NULL && logouts == 1 && logged_out == &a,
          "other name: rc 8, NULL, that ACEE deleted once");

    rc = verify("ABC", "ABC     ", 0, &a, &got);
    check(rc == 0 && got == &a && logouts == 0,
          "3 chars, padded: same name");

    rc = verify("ABC", "ABCD    ", 0, &a, &got);
    check(rc == 8 && got == NULL && logouts == 1,
          "request is a prefix of the ACEE name: rejected");

    rc = verify("ABCD", "ABC     ", 0, &a, &got);
    check(rc == 8 && got == NULL && logouts == 1,
          "ACEE name is a prefix of the request: rejected");

    rc = verify("ABCDEFGH", "ABCDEFGH", 0, &a, &got);
    check(rc == 0 && got == &a && logouts == 0, "8 chars: same name");

    rc = verify("ABCDEFGH", "ABCDEFGX", 0, &a, &got);
    check(rc == 8 && got == NULL && logouts == 1,
          "8 chars, 8th differs: rejected");

    rc = verify("ABCDEFGHIJ", "ABCDEFGH", 0, &a, &got);
    check(rc == 0 && got == &a && logouts == 0,
          "9+ chars: RACINIT got the first 8, so those are compared");

    rc = verify("ibmuser", "IBMUSER ", 0, &a, &got);
    check(rc == 8 && got == NULL && logouts == 1,
          "lower case request is not folded here: racf_login() folds first");

    rc = verify("", "        ", 0, &a, &got);
    check(rc == 8 && got == NULL && logouts == 1,
          "empty user: rejected");

    rc = verify("IBMUSER", "STC     ", 8, &a, &got);
    check(rc == 8 && got == &a && logouts == 0,
          "RACINIT said 8: rc kept, nothing deleted, nothing compared");

    rc = verify("IBMUSER", "STC     ", 4, &a, &got);
    check(rc == 4 && got == &a && logouts == 0,
          "RACINIT said 4: rc kept, nothing deleted");

    got = NULL;
    logouts = 0;
    {
        char userid[9];
        mkuser(userid, "IBMUSER");
        rc = verify_acee(&got, userid, 0, NULL);
    }
    check(rc == 8 && got == NULL && logouts == 0,
          "rc 0 and no ACEE: refused with rc 8, nothing to delete");

    {
        char userid[9];
        mkuser(userid, "IBMUSER");
        got = NULL;
        logouts = 0;
        rc = verify_acee(&got, userid, 4, NULL);
        check(rc == 4 && got == NULL && logouts == 0,
              "RACINIT said 4 and no ACEE: rc kept");
    }

    /* no name compared (user NULL): the ACEE is kept as it is */
    rc = verify(NULL, "STC     ", 0, &a, &got);
    check(rc == 0 && got == &a && logouts == 0,
          "user NULL: any name kept, rc 0, no delete");

    got = NULL;
    logouts = 0;
    rc = verify_acee(&got, NULL, 0, NULL);
    check(rc == 0 && got == NULL && logouts == 0,
          "user NULL, rc 0 and no ACEE: returned as it is");

    /* an ACEE that ASXBSENV points at, before or after RACINIT, is neither
    ** handed back nor deleted: NULL, rc 8, no racf_logout() */
    {
        ACEE    b;

        /* ASXBSENV after RACINIT is the ACEE */
        own = NULL; asxbsenv = &a;
        rc = verify("IBMUSER", "IBMUSER ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 0,
              "same name, ACEE is ASXBSENV's after: NULL, rc 8, no delete");

        own = NULL; asxbsenv = &a;
        rc = verify("IBMUSER", "STC     ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 0,
              "other name, ACEE is ASXBSENV's after: NULL, rc 8, no delete");

        own = NULL; asxbsenv = &a;
        rc = verify("", "        ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 0,
              "empty user, ACEE is ASXBSENV's after: NULL, rc 8, no delete");

        own = NULL; asxbsenv = &a;
        rc = verify(NULL, "STC     ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 0,
              "user NULL, ACEE is ASXBSENV's after: NULL, rc 8, no delete");

        /* ASXBSENV before RACINIT was the ACEE, after it is something else */
        own = &a; asxbsenv = &b;
        rc = verify("IBMUSER", "IBMUSER ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 0,
              "same name, ACEE is ASXBSENV's before: NULL, rc 8, no delete");

        own = &a; asxbsenv = NULL;
        rc = verify("IBMUSER", "STC     ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 0,
              "other name, ACEE is ASXBSENV's before: NULL, rc 8, no delete");

        own = &a; asxbsenv = &b;
        rc = verify(NULL, "STC     ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 0,
              "user NULL, ACEE is ASXBSENV's before: NULL, rc 8, no delete");

        /* ASXBSENV points elsewhere or at nothing: as before */
        own = &b; asxbsenv = &b;
        rc = verify("IBMUSER", "IBMUSER ", 0, &a, &got);
        check(rc == 0 && got == &a && logouts == 0,
              "same name, ASXBSENV elsewhere: kept");

        own = &b; asxbsenv = &b;
        rc = verify("IBMUSER", "STC     ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 1 && logged_out == &a,
              "other name, ASXBSENV elsewhere: that ACEE deleted");

        own = NULL; asxbsenv = NULL;
        rc = verify("IBMUSER", "STC     ", 0, &a, &got);
        check(rc == 8 && got == NULL && logouts == 1 && logged_out == &a,
              "other name, ASXBSENV empty: that ACEE deleted");

        own = &b; asxbsenv = NULL;
        rc = verify(NULL, "STC     ", 0, &a, &got);
        check(rc == 0 && got == &a && logouts == 0,
              "user NULL, ASXBSENV elsewhere before, empty after: kept");

        /* rc != 0: returned as it is, nothing compared, nothing deleted */
        own = &a; asxbsenv = &a;
        rc = verify("IBMUSER", "STC     ", 8, &a, &got);
        check(rc == 8 && got == &a && logouts == 0,
              "RACINIT said 8, ACEE is ASXBSENV's: rc kept, nothing deleted");

        own = NULL; asxbsenv = NULL;
    }

    printf("tstraclv: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
