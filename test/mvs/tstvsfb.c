/*
 * tstvsfb.c - libc370 #625 on MVS: __vsfdbk() reports R15, the feedback
 * code, RECLEN and RBA of each VSAM request, and only of that request.
 *
 * ISSUE #625: the VSAM functions return -1/-2 or "8"; the codes behind
 * them were only in vs->rc/vs->rsn/vs->component, which the LERAD and
 * SYNAD exits write on an error and nothing else touches.  After a
 * successful read they still held the previous error, and the end of
 * data (the EODAD exit) left them as they were, so a caller could not
 * learn the feedback code of the request it just made.
 *
 * DD VSAMKSDS is a three-record KSDS (keys KEY00001..KEY00003, 80 bytes)
 * that jcl/tstvsfb.jcl defines, loads and deletes again.  One handle,
 * dynamic access, update mode, four requests in this order:
 *   (b) read by a key that is not there     -> R15 8, feedback X'10'
 *   (a) read KEY00002                       -> R15 0, feedback 0,
 *                                              RECLEN 80, RBA 80
 *   (c) write KEY00001, which is there      -> R15 8, feedback X'08'
 *   (d) read KEY00003, then read on         -> R15 8, feedback X'04'
 *
 * Built twice from this source.  TSTVSFBG (-DNEWAPI) asks __vsfdbk()
 * after each request and links this tree's libc.a.  TSTVSFBR, the red
 * control, links the library of origin/main, which has no __vsfdbk():
 * it reads what that library offers for the same checks - vs->rc,
 * vs->rsn, vs->component, the return value of vsread() and vstell().
 *
 * Build (S a scratch directory, R a build of origin/main):
 *          make
 *          cc370 -O1 -Wall -DNEWAPI -Iinclude -L build/sdk \
 *                test/mvs/tstvsfb.c -o $S/TSTVSFBG -flinker-output=iebcopy
 *          cc370 -O1 -Wall -I$R/include -L $R/build/sdk \
 *                test/mvs/tstvsfb.c -o $S/TSTVSFBR -flinker-output=iebcopy
 *          cd $S && ld370 --pack TSTVSFBG=TSTVSFBG.iebcopy \
 *                TSTVSFBR=TSTVSFBR.iebcopy -o vsfb -xmit \
 *                --dsn IBMUSER.LIBC370.VSFBSCR
 * Install: jcl/recvvsfb.jcl.   Run: jcl/tstvsfb.jcl.
 *
 * Own MVS/CE container, 2026-10-20 (RECEIVE JOB00018, run JOB00019):
 * GREEN CC 0000, 13/13; RED CC 0001, 7/13.  The red step's codes stayed
 * from the request before: after (a) feedback X'10', after the end of
 * data X'08'; and vs->rc was 0 throughout, because the exits take it
 * from SHOWCB FIELDS=FDBK, which on MVS 3.8j gives the reason code
 * alone.  The RPL feedback words were X'14080010' (b), 0 (a),
 * X'0A080008' (c), X'00080004' (d) and X'07080004' (d').
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <mvs/vsam.h>
#include <mvs/wto.h>

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) failed++;
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) wtof("TSTVSFB FAIL: %s", what);
}

/* what the caller learns about the request it just made */
struct seen {
    int         r15;
    unsigned    fdbk;
    unsigned    comp;
    unsigned    reclen;
    unsigned    rba;
};

static void learn(VSFILE *vs, int ret, struct seen *s)
{
#ifdef NEWAPI
    VSFDBK  fb;
    int     rc;

    (void)ret;
    memset(&fb, 0xFF, sizeof(fb));
    rc = __vsfdbk(vs, &fb);
    if (rc) printf("  __vsfdbk() returned %d\n", rc);
    s->r15      = fb.r15;
    s->fdbk     = fb.fdbk;
    s->comp     = fb.comp;
    s->reclen   = fb.reclen;
    s->rba      = fb.rba;
    printf("  __vsfdbk: r15 %d fdbk X'%02X' comp %u reclen %u rba %u"
           " recaddr %p rsvd %u/%u/%u\n", fb.r15, fb.fdbk, fb.comp,
           fb.reclen, fb.rba, fb.recaddr, fb.rsvd1[0], fb.rsvd1[1],
           fb.rsvd2);
#else
    /* everything the 2.23 interface has: the handle's codes, the value
       the call returned (a record length for a read) and vstell() */
    s->r15      = vs->rc;
    s->fdbk     = vs->rsn;
    s->comp     = vs->component;
    s->reclen   = ret > 0 ? (unsigned)ret : 0;
    s->rba      = vstell(vs);
#endif
    printf("  RPL feedback word X'%08X'\n", vs->rpl.rplfdbwd);
    printf("  seen: ret %d r15 %d fdbk X'%02X' comp %u reclen %u rba %u\n",
           ret, s->r15, s->fdbk, s->comp, s->reclen, s->rba);
}

int main(void)
{
    VSFILE      *vs = NULL;
    char        buf[81];
    char        key[9];
    struct seen s;
    int         rc;
    int         r;

#ifdef NEWAPI
    printf("=== tstvsfb (GREEN, __vsfdbk): VSAM feedback per request"
           " (#625) ===\n");
#else
    printf("=== tstvsfb (RED, handle codes): VSAM feedback per request"
           " (#625) ===\n");
#endif

    rc = vsopen("VSAMKSDS", VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_UPD, &vs);
    check(rc == 0 && vs != NULL, "vsopen() of the three-record KSDS");
    if (rc != 0 || vs == NULL) {
        printf("  vsopen rc=%d\n", rc);
        goto done;
    }

    /* (b) a key that is not there */
    printf(" (b) read KEY00009 (not there)\n");
    memcpy(key, "KEY00009", 9);
    r = vsread(vs, buf, 80, key, 8);
    learn(vs, r, &s);
    check(r == -2, "(b) vsread() returns -2");
    check(s.r15 == 8 && s.fdbk == 0x10, "(b) R15 8, feedback X'10'");

    /* (a) a key that is there, right after the error */
    printf(" (a) read KEY00002\n");
    memset(buf, 0, sizeof buf);
    memcpy(key, "KEY00002", 9);
    r = vsread(vs, buf, 80, key, 8);
    learn(vs, r, &s);
    check(r == 80 && memcmp(buf, "KEY00002", 8) == 0,
          "(a) vsread() returns the 80-byte record KEY00002");
    check(s.r15 == 0 && s.fdbk == 0, "(a) R15 0, feedback 0");
    check(s.reclen == 80, "(a) RECLEN 80");
    check(s.rba == 80, "(a) RBA 80, the second record of the CI");

    /* (c) a duplicate key */
    printf(" (c) write KEY00001 (already there)\n");
    memset(buf, ' ', 80);
    memcpy(buf, "KEY00001 DUPLICATE", 18);
    r = vswrite(vs, buf, 80, buf, 8);
    learn(vs, r, &s);
    check(r == 8, "(c) vswrite() returns 8");
    check(s.r15 == 8 && s.fdbk == 0x08, "(c) R15 8, feedback X'08'");

    /* (d) the end of the data */
    printf(" (d) read KEY00003, then the next record\n");
    memcpy(key, "KEY00003", 9);
    r = vsread(vs, buf, 80, key, 8);
    learn(vs, r, &s);
    check(r == 80 && s.r15 == 0 && s.fdbk == 0 && s.rba == 160,
          "(d) KEY00003 read: R15 0, feedback 0, RBA 160");
    r = vsread(vs, buf, 80, NULL, 0);
    learn(vs, r, &s);
    check(r == -1, "(d) the read after the last record returns -1");
    check(s.r15 == 8 && s.fdbk == 0x04,
          "(d) end of data: R15 8, feedback X'04'");
    printf(" (d') one more read past the end\n");
    r = vsread(vs, buf, 80, NULL, 0);
    learn(vs, r, &s);
    check(r == -1 && s.r15 == 8 && s.fdbk == 0x04,
          "(d') past the end again: -1, R15 8, feedback X'04'");

    vsclose(vs);

done:
    printf("=== tstvsfb: %d/%d passed ===\n", run - failed, run);
    wtof("TSTVSFB %s %d/%d", failed ? "RED" : "GREEN", run - failed, run);
    return failed ? 1 : 0;
}
