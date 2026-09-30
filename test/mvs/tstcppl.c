/*
 * tstcppl.c - libc370 #210: CLIBPPA.ppacppl is never set, so tsocmd()
 * always answers "No CPPL" (MVS target, batch and batch TSO).
 *
 * ISSUE #210: ppacppl (PPA +X'2C') is read by tsocmd() and written by
 * nobody.  @@CRT0 clears the PPA (XC) before filling it, so the field is
 * always 0 and tsocmd() - and ispexec() on top of it - return 8 without
 * ever LINKing, even when the program runs as a TSO command processor.
 *
 * The CPPL is there all along: R1 on entry is saved as PGMR1 and handed to
 * __start() as pgmr1, whose words land in grt->grtptrs.  Under TSO, R1 is
 * the CPPL address and grtptrs[0..3] are its four words.
 *
 * ONE MODULE, FOUR WAYS IN.  The first operand says how the probe was
 * started, so each run knows what it should see:
 *
 *   BATCH  EXEC PGM=TSTCPPL,PARM='BATCH'         - no CPPL
 *   CALL   TSO CALL 'lib(TSTCPPL)' 'CALL'        - no CPPL: CALL passes a
 *          PARM-style list, not a CPPL
 *   CP     TSTCPPL CP, as a command in SYSTSIN   - a CPPL
 *   CHILD  started by the CP run through tsocmd() - a CPPL built by tsocmd
 *
 * CELLS
 *
 *   BATCH/CALL
 *     (n1) GRTFLAG1_TSO is off
 *     (n2) ppacppl is NULL
 *     (n3) tsocmd() returns 8 without LINKing (no CPPL to pass on)
 *     (n4) grtptrs holds one word: PARM and CALL lists carry the VL bit
 *          on their first word, and the copy stops there (#218 control)
 *   CP
 *     (c0) precondition: GRTFLAG1_TSO is on.  Otherwise the run was not a
 *          command processor and measures nothing (RC 12).
 *     (c1) ppacppl is not NULL                         RED before the fix
 *     (c2) ppacppl points at the CPPL: its four words equal grtptrs[0..3],
 *          and its PSCB word equals the PSCB that @@CRT0 took from
 *          EXTRACT (ppapscb) - an independent control
 *     (c3) tsocmd("TSTCPPL", "CHILD") returns 42      RED before the fix (8)
 *     (c4) the CHILD leaves the caller alone: after tsocmd() returns, the
 *          caller's ppacppl, the four words it points at and its grtptrs
 *          (array, count, words 0-3) are what they were before.  The CHILD
 *          runs its own __start() in the same address space and task, so
 *          this is where a shared anchor would show.
 *     (c5) grtptrs holds exactly the CPPL's four words     RED before #218
 *          (n=10: a CPPL has no VL bit, and __start() copied on for
 *          ten words, six of them from past the list)
 *   CHILD
 *     GRTFLAG1_TSO on, ppacppl not NULL, and its CBUF holds the command
 *     line "TSTCPPL CHILD" that tsocmd() built, and (k4) grtptrs holds
 *     four words - here the words past the CPPL are the caller's stack
 *     frame (RED before #218).  Returns 42 when all hold, 9 otherwise -
 *     so (c3) tells the two failures apart; the CHILD's WTOs say which
 *     cell it was.
 *
 * The CHILD is the probe itself, LINKed from the same STEPLIB.  No system
 * command is used: TIME, which brexx370 met the bug with, has no load
 * module of that name in SYS1.CMDLIB, SYS1.LINKLIB, SYS2.CMDLIB or
 * SYS2.LINKLIB on mvsdev (LPALIB not checked), so it is not a safe LINK
 * target for tsocmd().
 *
 * NO WRITABLE STATICS.  For the foreground run the module has to sit in
 * the link list (the logon procedure has no STEPLIB), and a module fetched
 * from the LNKLST cannot store into its own statics - S0C4.  The failure
 * count therefore travels through automatic storage.
 *
 * Every cell is reported by wtof() before the next one runs: a wrong
 * ppacppl makes tsocmd() copy from a wild address, and stdio output does
 * not survive an abend.  ppaflag is reported too: under batch IKJEFT01
 * it is X'40' (PPAFLAG_TSOBG) for the CALL run and the CP run alike
 * (JOB00677), so the PPA flag cannot tell a command processor from CALL.
 * That is why the fix keys on GRTFLAG1_TSO.
 *
 * Measured on mvsdev, 2026-09-29.  Red JOB00677 (and JOB00681, rebuilt
 * with cc370 b91f913): BATCH and CALL pass, CP fails (c1) and (c3),
 * tsocmd rc=8.  grtptrs[2] there equals ppapscb, so the words at R1 are
 * the CPPL.  Green with the fix: JOB00683, every cell passes and the
 * CHILD returns 42; with (c4) added and the statics removed: JOB00686.
 * TSO foreground, 2026-09-29: a 3270 session as MVSCE01 (TN3270 port
 * 3270 on mvsdev - 3272 is a different system), the module IEBCOPYed
 * into SYS2.LINKLIB for the run and removed afterwards: "TSTCPPL CP"
 * passes c1-c4 and k1-k3, "CALL ...(TSTCPPL) 'CALL'" passes n1-n3.
 * From the link list in batch too (no STEPLIB): JOB00689.
 *
 * #218, mvsdev 2026-09-29, batch IKJEFT01 only (the foreground and
 * link-list runs above predate these cells).  Red JOB00699: CP and CHILD
 * see n=10, (c5) and (k4) fail, (c3) with them because the CHILD returns
 * 9; BATCH and CALL n=1, (n4) passes.  Green with the fix: JOB00701,
 * every cell passes, CP and CHILD n=4; with (c5) after (c2): JOB00704.
 *
 * BUILD (host):
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstcppl.c -o TSTCPPL \
 *           -flinker-output=iebcopy
 *     ld370 --pack TSTCPPL=TSTCPPL.iebcopy -o tstcppl -xmit \
 *           --dsn IBMUSER.LIBC370.CPPLLIB
 * Upload tstcppl.xmit to IBMUSER.LIBC370.CPPLXMI (FB 80) and submit
 * jcl/recvcppl.jcl, then jcl/tstcppl.jcl.
 *
 * RC:  0 every cell of this run passed
 *      8 a cell failed
 *     12 CP run without a CPPL environment - nothing measured
 *     42 CHILD: all held (9: a CHILD check failed)
 */
#include <stdio.h>
#include <string.h>
#include "libc370/array.h"
#include "mvs/crt.h"
#include "mvs/tso.h"
#include "mvs/wto.h"
#include "ibm/mvs/ikjcppl.h"

static void
cell(int *fails, const char *mode, const char *id, int ok, const char *what)
{
    if (!ok) (*fails)++;
    printf("%-5s %-3s %s  %s\n", mode, id, ok ? "PASS" : "FAIL", what);
    wtof("TSTCPPL %s %s %s %s", mode, id, ok ? "PASS" : "FAIL", what);
}

static void
state(const char *mode, CLIBPPA *ppa, CLIBGRT *grt)
{
    void        **p = grt->grtptrs;
    unsigned    n   = p ? arraycount(&grt->grtptrs) : 0;

    wtof("TSTCPPL %s TSO=%d PPAFLAG=%02X PSCB=%08X CPPL=%08X",
         mode, (grt->grtflag1 & GRTFLAG1_TSO) ? 1 : 0,
         (unsigned char)ppa->ppaflag, (unsigned)ppa->ppapscb,
         (unsigned)ppa->ppacppl);
    wtof("TSTCPPL %s PTRS n=%u %08X %08X %08X %08X", mode, n,
         n > 0 ? (unsigned)p[0] : 0, n > 1 ? (unsigned)p[1] : 0,
         n > 2 ? (unsigned)p[2] : 0, n > 3 ? (unsigned)p[3] : 0);
}

/* started by the CP run through tsocmd() */
static int
child(CLIBPPA *ppa, CLIBGRT *grt)
{
    static const char want[] = "TSTCPPL CHILD";
    CPPL    *cppl   = ppa->ppacppl;
    TSOCBUF *cbuf;
    int     len;
    int     fails   = 0;

    state("CHILD", ppa, grt);
    cell(&fails, "CHILD", "k1", (grt->grtflag1 & GRTFLAG1_TSO) != 0,
         "GRTFLAG1_TSO on");
    cell(&fails, "CHILD", "k2", cppl != NULL, "ppacppl set");
    if (cppl) {
        cbuf = cppl->cpplcbuf;
        len  = cbuf ? cbuf->cbuflen - 4 : -1;
        wtof("TSTCPPL CHILD CBUF len=%d '%.*s'", len,
             len > 0 && len < 80 ? len : 0, cbuf ? cbuf->cmdname : "");
        cell(&fails, "CHILD", "k3", len == (int)strlen(want)
             && memcmp(cbuf->cmdname, want, len) == 0,
             "CBUF is the command line tsocmd() built");
    }
    cell(&fails, "CHILD", "k4", grt->grtptrs
         && arraycount(&grt->grtptrs) == 4, "grtptrs is the CPPL, n=4");
    return fails ? 9 : 42;
}

/* BATCH and CALL: no CPPL may be seen, and tsocmd() must refuse */
static int
nocppl(const char *mode, CLIBPPA *ppa, CLIBGRT *grt)
{
    int     rc;
    int     fails   = 0;

    state(mode, ppa, grt);
    cell(&fails, mode, "n1", (grt->grtflag1 & GRTFLAG1_TSO) == 0,
         "GRTFLAG1_TSO off");
    cell(&fails, mode, "n2", ppa->ppacppl == NULL, "ppacppl NULL");
    if (ppa->ppacppl == NULL) {
        rc = tsocmd("TSTCPPL", "CHILD");
        wtof("TSTCPPL %s tsocmd rc=%d", mode, rc);
        cell(&fails, mode, "n3", rc == 8, "tsocmd() refuses with 8");
    }
    cell(&fails, mode, "n4", grt->grtptrs
         && arraycount(&grt->grtptrs) == 1, "grtptrs stops at the VL bit, n=1");
    return fails ? 8 : 0;
}

static int
cp(CLIBPPA *ppa, CLIBGRT *grt)
{
    CPPL        *cppl   = ppa->ppacppl;
    CPPL        words;
    void        **ptrs;
    void        *ptr4[4];
    unsigned    n;
    int         rc;
    int         fails   = 0;

    state("CP", ppa, grt);
    if (!(grt->grtflag1 & GRTFLAG1_TSO) || !grt->grtptrs
        || arraycount(&grt->grtptrs) < 4) {
        wtof("TSTCPPL CP VOID - not started as a command processor");
        return 12;
    }
    cell(&fails, "CP", "c1", cppl != NULL, "ppacppl set");
    if (!cppl) {
        /* what the fix is for: tsocmd() cannot run without it */
        rc = tsocmd("TSTCPPL", "CHILD");
        wtof("TSTCPPL CP tsocmd rc=%d", rc);
        cell(&fails, "CP", "c3", rc == 42, "tsocmd() runs the CHILD");
        return 8;
    }
    cell(&fails, "CP", "c2", memcmp(cppl, grt->grtptrs, sizeof(CPPL)) == 0
         && cppl->cpplpscb == ppa->ppapscb,
         "ppacppl is the CPPL (grtptrs, ppapscb)");
    cell(&fails, "CP", "c5", arraycount(&grt->grtptrs) == 4,
         "grtptrs is the CPPL, n=4");

    /* the caller's state, to compare after the CHILD has run */
    memcpy(&words, cppl, sizeof(words));
    ptrs = grt->grtptrs;
    n    = arraycount(&grt->grtptrs);
    memcpy(ptr4, ptrs, sizeof(ptr4));

    rc = tsocmd("TSTCPPL", "CHILD");
    wtof("TSTCPPL CP tsocmd rc=%d", rc);
    cell(&fails, "CP", "c3", rc == 42, "tsocmd() runs the CHILD");

    state("CP", ppa, grt);
    cell(&fails, "CP", "c4", ppa->ppacppl == cppl
         && memcmp(cppl, &words, sizeof(words)) == 0
         && grt->grtptrs == ptrs && arraycount(&grt->grtptrs) == n
         && memcmp(grt->grtptrs, ptr4, sizeof(ptr4)) == 0,
         "caller's CPPL and grtptrs unchanged by the CHILD");
    return fails ? 8 : 0;
}

int
main(int argc, char **argv)
{
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    const char  *mode   = argc > 1 ? argv[1] : "";
    int         rc;

    if (!ppa || !grt) {
        wtof("TSTCPPL no PPA or GRT");
        return 8;
    }

    if      (strcmp(mode, "CHILD") == 0) rc = child(ppa, grt);
    else if (strcmp(mode, "CP") == 0)    rc = cp(ppa, grt);
    else if (strcmp(mode, "BATCH") == 0
          || strcmp(mode, "CALL") == 0)  rc = nocppl(mode, ppa, grt);
    else {
        wtof("TSTCPPL unknown mode '%s'", mode);
        rc = 12;
    }

    wtof("TSTCPPL %s RC=%d", mode, rc);
    return rc;
}
