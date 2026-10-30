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
 * recorder.  The RACINIT call itself, the fold to upper case and the
 * password and PASSCHK=NO paths are asm and are measured on MVS
 * (test/mvs/tstraclv.c).
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

int racf_logout(ACEE **acee)
{
    logouts++;
    logged_out = *acee;
    *acee = NULL;
    return 0;
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

/* one call: returns the rc, leaves *got, logouts and logged_out for checks */
static int verify(const char *want, const char *aceename, int rc_in,
                  ACEE *acee, ACEE **got)
{
    char userid[9];

    mkuser(userid, want);
    mkacee(acee, aceename);
    *got = acee;
    logouts = 0;
    logged_out = NULL;
    return verify_acee(got, userid, rc_in);
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
        rc = verify_acee(&got, userid, 0);
    }
    check(rc == 0 && got == NULL && logouts == 0,
          "rc 0 and no ACEE: unchanged");

    printf("tstraclv: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
