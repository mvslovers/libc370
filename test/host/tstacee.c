/*
 * tstacee.c - libc370 #581: the ACEE's userid and group are a length byte
 * and an 8-byte name, IBM's ACEEUSRL/ACEEUSRI and ACEEGRPL/ACEEGRPN, and
 * ihaacee.h names them beside the old aceeuser[9] and aceegrp[9].
 *
 * The host's pointers are 8 bytes, so the absolute offsets (X'14', X'1D')
 * are not the target's; this checks the layout relative to aceeuser, which
 * holds on any machine since the fields are all char.  The absolute
 * offsets and sizeof(ACEE) == 144 were checked with cc370 when #581 was
 * made: unchanged.
 *
 * BUILD / RUN (host, from the repository root):
 *
 *     cc -std=gnu99 -Wall -Wextra -Werror -idirafter include -o t \
 *        test/host/tstacee.c && ./t
 *
 * -idirafter: the host's own headers come first, libc370's only for
 * ihaacee.h and <sys/_cc370.h>; libc370's <string.h> carries S/370 asm.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <ibm/mvs/ihaacee.h>

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

#define OFF(m) offsetof(ACEE, m)

int main(void)
{
    ACEE    a;

    check(OFF(aceeusrl) == OFF(aceeuser), "aceeusrl is aceeuser[0]");
    check(OFF(aceeusri) == OFF(aceeuser) + 1, "aceeusri is aceeuser + 1");
    check(OFF(aceegrp) == OFF(aceeuser) + 9, "aceegrp follows 9 bytes on");
    check(OFF(aceegrpl) == OFF(aceegrp), "aceegrpl is aceegrp[0]");
    check(OFF(aceegrpn) == OFF(aceegrp) + 1, "aceegrpn is aceegrp + 1");
    check(OFF(aceeflg1) == OFF(aceegrp) + 9, "aceeflg1 follows aceegrp");

    /* the names and the arrays are the same storage */
    memset(&a, 0, sizeof(a));
    a.aceeusrl = 5;
    memcpy(a.aceeusri, "IBMUSER ", 8);
    check(a.aceeuser[0] == 5 && memcmp(a.aceeuser + 1, "IBMUSER ", 8) == 0,
          "aceeusrl/aceeusri read back through aceeuser");

    printf("tstacee: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
