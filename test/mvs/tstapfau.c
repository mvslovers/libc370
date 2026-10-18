/*
 * tstapfau.c - libc370 #402 on MVS, the authorized half: functions that
 * switch to supervisor state return an authorized caller in the state
 * and key it came in.  (APF-AUTHORIZED, run from SYS2.LINKLIB.)
 *
 * For clib_auth_cde(), iefssreq() (through jescanj() on a job that does
 * not exist), __austep() and __uastep():
 *
 *   P  called in problem state: back in problem state
 *   S  called in supervisor state, key 0 (inside super_key_do): back in
 *      supervisor state with key 0
 *
 * Up to 2.18.1 every one of them returned in problem state with the TCB
 * key, so the S cases fail there.  __austep()/__uastep() change nothing:
 * SYS2.LINKLIB is authorized already, and __uastep() only undoes what
 * __austep() set.  jescanj() asks JES2 to cancel NOSUCHJB/JOB99999.
 *
 * The state is read with __issup(), the key with IPK only in supervisor
 * state (IPK may fail in problem state).
 *
 * BUILD (host) - AC=1 on the ld370 link, and again on the pack:
 *     cc370 -O1 -Iinclude -c test/mvs/tstapfau.c -o tstapfau.o
 *     ld370 --entry @@CRT0 --ac 1 -iebcopy -o TSTAPFAU tstapfau.o \
 *           -Lbuild/sdk -lc
 *     ld370 --pack --ac 1 TSTAPFAU=TSTAPFAU.iebcopy -o probe -xmit ...
 * Run: jcl/tstapfau.jcl (copies into SYS2.LINKLIB and removes again).
 * mvsdev JOB02458, 2026-10-18: this tree 11/11; 2.18.1 passed the P
 * cases and ended S0C2 in the first key-0 case - clib_auth_cde() had
 * returned it in problem state.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <errno.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/apf.h>
#include <mvs/jes2.h>
#include <ibm/mvs/ihacde.h>

static unsigned char pswkey(void)
{
    unsigned char key;

    __asm__("IPK\t0\n\tSTC\t2,0(,%0)" : : "r"(&key) : "2", "memory");
    return key;
}

/* each case, run from inside super_key_do(PSWKEY0, ...): the result
   word gets 1 when the state and key came back as they went in */
static int s_cde(int *ok)
{
    CDE cde;

    memset(&cde, 0, sizeof(cde));
    clib_auth_cde(&cde);
    *ok = __issup() && pswkey() == 0x00;
    return 0;
}

static int s_ssreq(int *ok)
{
    jescanj("NOSUCHJB", "JOB99999", 0);
    *ok = __issup() && pswkey() == 0x00;
    return 0;
}

static int s_austep(int *ok)
{
    __austep();
    *ok = __issup() && pswkey() == 0x00;
    return 0;
}

static int s_uastep(int *ok)
{
    __uastep();
    *ok = __issup() && pswkey() == 0x00;
    return 0;
}

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTAPFAU %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    CDE     cde;
    int     ok;
    int     rc;

    want(__isauth(), "running APF authorized");
    if (!__isauth()) goto quit;

    memset(&cde, 0, sizeof(cde));
    clib_auth_cde(&cde);
    want(!__issup(), "P clib_auth_cde: back in problem state");
    rc = jescanj("NOSUCHJB", "JOB99999", 0);
    wtof("TSTAPFAU     jescanj(NOSUCHJB) rc=%d", rc);
    want(!__issup(), "P iefssreq (jescanj): back in problem state");
    __austep();
    want(!__issup(), "P __austep: back in problem state");
    __uastep();
    want(!__issup(), "P __uastep: back in problem state");

    ok = 0; super_key_do(PSWKEY0, s_cde, &ok);
    want(ok, "S clib_auth_cde: still supervisor state, key 0");
    ok = 0; super_key_do(PSWKEY0, s_ssreq, &ok);
    want(ok, "S iefssreq (jescanj): still supervisor state, key 0");
    ok = 0; super_key_do(PSWKEY0, s_austep, &ok);
    want(ok, "S __austep: still supervisor state, key 0");
    ok = 0; super_key_do(PSWKEY0, s_uastep, &ok);
    want(ok, "S __uastep: still supervisor state, key 0");
    want(!__issup(), "after the S cases: back in problem state");

quit:
    wtof("TSTAPFAU VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
