/*
 * tstfab705.c - libc370 #705 on MVS: what __fabandon() returns, and
 * whether the DD is really still held, when the CLOSE it issues abends.
 *
 * Written to measure #705, and then the regression probe for its three
 * fixes.  Every case WTOs what it saw; the checks (PASS/FAIL lines) are
 * the verdict, and the step ends RC 0 green, RC 8 when a check failed or
 * a case could not be set up:
 *
 *   - the return agrees with the DD: -2 exactly when the DD is still in
 *     the DSAB chain, >= 0 when it went (every round of every case)
 *   - C: no stale buffer pointer in the work area after the abended
 *     CLOSE, and a block claimed where a freed buffer was is not freed
 *     a second time by __fabandon() (a verdict only when GETMAIN places
 *     the block there)
 *   - M: __fabandon() on a member with blocks written stores no member
 *     and leaves an existing one alone
 *
 * The consumer shape (mvslovers/ftpd): fopen("'PDS(MEMBER)'","wb") on a
 * PDS whose directory is full, fwrite, fflush, fclose().  The STOW inside
 * CLOSE fails and IGG0201Z abends B14-0C; the caller's ESTAE recovers
 * and calls __fabandon() on the FILE that fclose() never finished.
 *
 * PARM selects the case.  Every case runs TWO ROUNDS in the same step,
 * each on a FILE of its own, because the issue reports a different
 * result for the first and the second occurrence per address space.
 *
 *   C  consumer: fclose() under try() abends SB14, then __fabandon()
 *   D  direct:   __fabandon() instead of fclose() on the full directory
 *   M  member:   __fabandon() on a member with room in the directory and
 *                two blocks on DASD; M2 over an existing member
 *   E  E37:      a member of a TRK(1,1,1) PDS written until the secondary
 *                space is used up (SE37-04, which no exit catches, #543),
 *                under try(); then __fabandon()
 *   S  D37:      a sequential TRK(1,0) data set written until it is full.
 *                Since #176 that is ENOSPC, not an abend; __fabandon()
 *                instead of fclose() all the same
 *   A  diagnostic: C, but the second CLOSE is __aclose() called bare,
 *                without an ESTAE, so an abend in it ends the step with
 *                a SYSUDUMP that shows which service failed
 *
 * A second PARM letter is appended to the data set names (PARM='CR'
 * works on IBMUSER.TF705.CR), so steps of two builds do not collide.
 *
 * Per round the WTOs record: the try() code of the failing call, the
 * DCB as fclose() left it, __fabandon()'s return, tryinfo() of the CLOSE
 * inside __fabandon() (with the addresses of @@ACLOSE and main to place
 * the PSW), whether the DD is still in the DSAB chain and its open-DCB
 * count, what __dsfree() of it answers, and whether the member exists.
 * At the end of the step remove() of each data set - IDCAMS DELETE from
 * inside the address space, which a held DD refuses.
 *
 * The full directory is made with STOW 'A' of dummy entries on a BPAM
 * DCB of the probe's own (like @@pdsupd.c), not with fclose(): a fill by
 * CLOSE would itself take SB14 in this address space, and the first SB14
 * per process is what is being measured.  The dummy entries point at TTR
 * 000001 and are never read.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -I. -L build/sdk \
 *         test/mvs/tstfab705.c -o $S/TSTF705G -flinker-output=iebcopy
 *
 * and the same probe against libc370 2.10.3, the library the issue was
 * measured with (R, a detached worktree of tag v2.10.3 after make; the
 * library predates tryinfo() and __fx37(), hence -DOLDLIB):
 *
 *     cc370 -O1 -Wall -Werror -DOLDLIB -I$R/include -I$R -L $R/build/sdk \
 *         test/mvs/tstfab705.c -o $S/TSTF705R -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTF705G=TSTF705G.iebcopy \
 *         TSTF705R=TSTF705R.iebcopy -o f705 -xmit \
 *         --dsn IBMUSER.LIBC370.F705SCR
 *
 * then jcl/recvf705.jcl and jcl/tstf705.jcl.  Data sets IBMUSER.TF705.*
 * must not exist when a step starts.
 */
#include <mvs/dynalloc.h>
#include <mvs/file.h>
#include <mvs/recovery.h>
#include <mvs/wto.h>
#include <mvs/dd.h>
#include <mvs/osio.h>
#include <mvs/pds.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "src/internal/fileio.h"
#include "src/internal/bsam.h"

#define PDS     "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PO;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(%s)"
#define SEQ     "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=800;UNIT=SYSDA;SPACE=TRK(%s)"

#define LIMIT   60000L          /* records; E37 needs 16 tracks of them   */

static int   unmeasured;
static int   bad;               /* checks that failed                     */
static int   memexists;         /* after(): the member was found          */
static FILE *thefp;
static long  written;
static int   nrecs = 10;        /* records the C and D rounds write       */

int main(int argc, char **argv);

static void check(int cond, const char *tag, const char *what)
{
    wtof("TF705 %s %s: %s", tag, cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* GETMAIN/FREEMAIN RC in subpool 0, without getmain()'s prefix and
 * rounding: the probe asks for exactly what @@ACLOSE gave back, so the
 * block can land where a freed buffer was */
static void *gm0(unsigned lv)
{
    int     rc;
    void    *a;

    __asm__ __volatile__("LR\t0,%2\n\t"
            "GETMAIN RC,LV=(0),SP=0\n\t"
            "LR\t%0,15\n\t"
            "LR\t%1,1"
            : "=r"(rc), "=r"(a) : "r"(lv) : "0", "1", "14", "15", "memory");
    return rc ? 0 : a;
}

static int fm0(void *a, unsigned lv)
{
    int     rc;

    __asm__ __volatile__("LR\t0,%1\n\t"
            "LR\t1,%2\n\t"
            "FREEMAIN RC,LV=(0),A=(1),SP=0\n\t"
            "LR\t%0,15"
            : "=r"(rc) : "r"(lv), "r"(a) : "0", "1", "14", "15", "memory");
    return rc;
}

static void writer(FILE *fp)
{
    char    rec[80];

    memset(rec, 'X', sizeof(rec));
    for (written = 0; written < LIMIT; written++) {
        if (fwrite(rec, 1, sizeof(rec), fp) != sizeof(rec)) break;
    }
}

static void fewrecs(FILE *fp)
{
    char    rec[80];
    int     i;

    memset(rec, 'Y', sizeof(rec));
    for (i = 0; i < nrecs; i++) fwrite(rec, 1, sizeof(rec), fp);
    fflush(fp);
}

static void closer(FILE *fp)
{
    fclose(fp);
}

/* fopen(fn, "wb"), or say why not and count the case as unmeasured */
static FILE *opened(const char *tag, const char *fn)
{
    FILE    *fp = fopen(fn, "wb");

    if (!fp) {
        wtof("TF705 %s fopen %s NULL", tag, fn);
        unmeasured++;
    }
    return fp;
}

/* ------------------------------------------------------------------------ */
static int create(const char *fmt, const char *dsn, const char *space)
{
    char    dd[9] = {0};
    int     rc = __dsalcf(dd, fmt, dsn, space);

    if (rc == 0) rc = __dsfree(dd);         /* the free catalogs it        */
    wtof("TF705 create %s rc=%d", dsn, rc);
    return rc;
}

/* Fill the directory with STOW 'A' until STOW refuses.  Returns the number
 * of entries added; the last STOW rc is WTO'd (12 = no directory space). */
static int filldir(const char *dsn)
{
    char    dd[9] = {0};
    char    ent[12];
    DCB     *dcb;
    int     n = 0, rc;

    if (__dsalcf(dd, "DSN=%s;DISP=SHR", dsn)) return -1;
    dcb = osbdcb(dd, NULL);
    if (!dcb) { __dsfree(dd); return -1; }
    dcb->dcbdsrg1 = DCBDSGPO;
    dcb->dcbdsrg2 = 0;
    if (osbopen(dcb, 0, "write")) {
        osbclose(dcb, NULL, 1, 0);
        __dsfree(dd);
        return -1;
    }
    for (;;) {
        char name[9];

        snprintf(name, sizeof(name), "F%04d   ", n);
        memset(ent, 0, sizeof(ent));
        memcpy(ent, name, 8);
        ent[10] = 1;                        /* TTR 000001, never read      */
        rc = __stow(dcb, ent, 'A');
        if (rc) break;
        if (++n > 500) break;
    }
    osbclose(dcb, NULL, 1, 0);
    __dsfree(dd);
    wtof("TF705 filldir %s entries=%d last STOW rc=%d", dsn, n, rc);
    return n;
}

static void dcbdump(const char *tag, FILE *fp)
{
    const unsigned char *d = (const unsigned char *)fp->dcb;
    char    hex[2 * 0x34 + 1];
    int     i;

    if (!d) {
        wtof("TF705 %s dcb=0", tag);
        return;
    }
    for (i = 0; i < 0x34; i++) snprintf(hex + 2 * i, 3, "%02X", d[i]);
    wtof("TF705 %s dcb=%08X flags=%08X", tag, (unsigned)d,
         (unsigned)fp->flags);
    wtof("TF705 %s +00 %.40s", tag, hex);
    wtof("TF705 %s +14 %.32s", tag, hex + 40);
    wtof("TF705 %s +24 %.32s", tag, hex + 72);
    /* ZBUFF1 and ZBUFF2 (address, length) at +A4 of the work area, as
       maclib/clibsupa.copy lays it out: the buffers @@ACLOSE frees */
    wtof("TF705 %s zbuff1=%08X,%X zbuff2=%08X,%X file=%08X buf=%08X",
         tag, *(const unsigned *)(d + 0xA4), *(const unsigned *)(d + 0xA8),
         *(const unsigned *)(d + 0xAC), *(const unsigned *)(d + 0xB0),
         (unsigned)fp, (unsigned)fp->buf);
}

/* What __fabandon() answered, and the DD and member afterwards.  Returns
 * 1 when the DD was still allocated. */
static int after(const char *tag, int arc, const char *dd,
                  const char *dsn, const char *mem)
{
#ifndef OLDLIB
    const TRYINFO *ti = tryinfo();
#endif
    DSAB    *dsab;
    FILE    *rf;
    char    fn[60];
    int     frc;

    wtof("TF705 %s __fabandon rc=%d (%08X) tryrc=%08X", tag, arc,
         (unsigned)arc, tryrc());
#ifndef OLDLIB
    wtof("TF705 %s tryinfo psw=%08X %08X r14=%08X aclose=%08X main=%08X",
         tag, ti->psw[0], ti->psw[1], ti->r14,
         (unsigned)__aclose, (unsigned)main);
#else
    wtof("TF705 %s aclose=%08X main=%08X", tag, (unsigned)__aclose,
         (unsigned)main);
#endif

    dsab = get_dsab(0, dd);
    if (dsab)
        wtof("TF705 %s DD %s STILL IN DSAB chain, opct=%u", tag, dd,
             (unsigned)dsab->dsabopct);
    else
        wtof("TF705 %s DD %s gone from the DSAB chain", tag, dd);
    if (dsab) {
        frc = __dsfree(dd);
        wtof("TF705 %s __dsfree(%s) rc=%d", tag, dd, frc);
    }

    /* the contract of #705: -2 when the DD is still there, never
       otherwise; anything >= 0 says the DD went */
    check(dsab ? arc == -2 : arc >= 0, tag,
          "the return agrees with the DD (#705)");

    memexists = 0;
    if (mem) {
        snprintf(fn, sizeof(fn), "'%s(%s)'", dsn, mem);
        rf = fopen(fn, "rb");
        memexists = rf != 0;
        wtof("TF705 %s member %s %s", tag, mem, rf ? "EXISTS" : "absent");
        if (rf) fclose(rf);
    }
    return dsab != 0;
}

/* ---- C: fclose() under try(), then __fabandon() ------------------------ */
static void round_c(const char *tag, const char *dsn, const char *mem)
{
    char    fn[60], dd[9];
    int     abend, arc, i, frc;
    const unsigned *w;
    unsigned ba[2], bl[2];
    void    *claim[2];

    snprintf(fn, sizeof(fn), "'%s(%s)'", dsn, mem);
    thefp = opened(tag, fn);
    if (!thefp) return;
    strcpy(dd, thefp->ddname);
    abend = try(fewrecs, thefp);
    wtof("TF705 %s fopen dd=%s write try=%08X", tag, dd, abend);
    dcbdump(tag, thefp);
    w = (const unsigned *)thefp->dcb;
    for (i = 0; i < 2; i++) {
        ba[i] = w[(0xA4 + 8 * i) / 4];
        bl[i] = (w[(0xA8 + 8 * i) / 4] + 7) & ~7u;
    }
    abend = try(closer, thefp);
    wtof("TF705 %s fclose under try=%08X", tag, abend);
    if (abend == 0) {
        /* fclose() completed and freed the FILE: nothing to abandon */
        wtof("TF705 %s fclose did NOT abend - no SB14", tag);
        unmeasured++;
        return;
    }
    dcbdump(tag, thefp);

    /* (#705) The CLOSE abended, the work area lives on, and __fabandon()
       runs @@ACLOSE on it a second time.  Buffer pointers still set here
       are what that second run FREEMAINs again. */
    check(w[0xA8 / 4] == 0 && w[0xB0 / 4] == 0, tag,
          "no stale buffer pointer after the abended CLOSE");

    /* ... and whether that second FREEMAIN hits live storage.  Claim the
       storage the buffers occupied, the way any GETMAIN in between could:
       a block that lands exactly on a freed buffer must still be ours
       after __fabandon().  Where GETMAIN places it is not the probe's to
       choose, so a block that lands elsewhere decides nothing. */
    for (i = 0; i < 2; i++) {
        claim[i] = gm0(bl[i]);
        if (claim[i] && (unsigned)claim[i] != ba[i]) {
            fm0(claim[i], bl[i]);
            claim[i] = 0;
        }
        wtof("TF705 %s buffer %d %08X,%X claimed %s", tag, i + 1, ba[i],
             bl[i], claim[i] ? "in place" : "elsewhere - no verdict");
    }

    arc = __fabandon(thefp);
    after(tag, arc, dd, dsn, mem);

    for (i = 0; i < 2; i++) {
        if (!claim[i]) continue;
        frc = fm0(claim[i], bl[i]);
        wtof("TF705 %s claimed block %d FREEMAIN rc=%d", tag, i + 1, frc);
        check(frc == 0, tag, "the claimed block was not freed a second time");
    }
}

/* ---- A: C, but the second CLOSE without any ESTAE ---------------------- */
/* __fabandon()'s steps by hand, up to the try(__aclose), which is called
 * bare instead: an abend in it ends the step with a SYSUDUMP, whose PSW
 * and registers say which service failed.  A diagnostic case for the
 * SA0A of the issue, not a supported way to close anything. */
static void round_a(const char *tag, const char *dsn, const char *mem)
{
    char    fn[60], dd[9];
    int     abend, rc;

    snprintf(fn, sizeof(fn), "'%s(%s)'", dsn, mem);
    thefp = opened(tag, fn);
    if (!thefp) return;
    strcpy(dd, thefp->ddname);
    abend = try(fewrecs, thefp);
    dcbdump(tag, thefp);
    abend = try(closer, thefp);
    wtof("TF705 %s fclose under try=%08X", tag, abend);
    if (abend == 0) { unmeasured++; return; }
    dcbdump(tag, thefp);
    wtof("TF705 %s dcb=%08X aclose=%08X main=%08X - bare __aclose next",
         tag, (unsigned)thefp->dcb, (unsigned)__aclose, (unsigned)main);
    __adisc(thefp->dcb);
    rc = __aclose(thefp->dcb);
    wtof("TF705 %s bare __aclose returned %d", tag, rc);

    /* the DCB and its work area are gone: teardown only, or the exit
       routine's fclose() of every survivor closes them a second time */
    thefp->flags &= ~_FILE_FLAG_OPEN;
    thefp->dcb    = 0;
    thefp->asmbuf = 0;
    fclose(thefp);
    /* its "__fabandon rc" line carries the bare __aclose()'s return and
       its tryrc the B14 of the fclose() above */
    after(tag, rc, dd, dsn, mem);
}

/* ---- D: __fabandon() instead of fclose() ------------------------------- */
static void round_d(const char *tag, const char *dsn, const char *mem)
{
    char    fn[60], dd[9];
    int     abend, arc;

    snprintf(fn, sizeof(fn), "'%s(%s)'", dsn, mem);
    thefp = opened(tag, fn);
    if (!thefp) return;
    strcpy(dd, thefp->ddname);
    abend = try(fewrecs, thefp);
    wtof("TF705 %s fopen dd=%s write try=%08X", tag, dd, abend);
    arc = __fabandon(thefp);
    after(tag, arc, dd, dsn, mem);
}

/* ---- M: __fabandon() on a member, with room in the directory ---------- */
/* Blocks are on DASD already: 25 records of 80 in blocks of 800 are two
 * full blocks written and five records pending.  Before #705 the CLOSE
 * in __fabandon() STOWed what was on DASD as the member - a new member
 * with part of its data, or (M2) an existing one replaced by it. */
static void round_m(const char *tag, const char *dsn, const char *mem,
                    int replace)
{
    char    fn[60], dd[9], rec[80];
    FILE    *rf;
    int     abend, arc;

    snprintf(fn, sizeof(fn), "'%s(%s)'", dsn, mem);
    if (replace) {
        rf = opened(tag, fn);
        if (!rf) return;
        memset(rec, 'O', sizeof(rec));
        fwrite(rec, 1, sizeof(rec), rf);
        wtof("TF705 %s old member written, fclose rc=%d", tag, fclose(rf));
    }
    thefp = opened(tag, fn);
    if (!thefp) return;
    strcpy(dd, thefp->ddname);
    nrecs = 25;
    abend = try(fewrecs, thefp);
    nrecs = 10;
    wtof("TF705 %s fopen dd=%s 25 records try=%08X", tag, dd, abend);
    arc = __fabandon(thefp);
    after(tag, arc, dd, dsn, mem);
    check(arc == 0, tag, "__fabandon() rc=0");
    if (!replace) {
        check(!memexists, tag, "no member stored by __fabandon()'s CLOSE");
        return;
    }
    /* the old member must be untouched: one record of 'O' */
    memset(rec, 0, sizeof(rec));
    rf = fopen(fn, "rb");
    if (rf) {
        if (fread(rec, 1, sizeof(rec), rf) != sizeof(rec)) rec[0] = 0;
        fclose(rf);
    }
    wtof("TF705 %s old member first byte %02X", tag, (unsigned char)rec[0]);
    check(rf && rec[0] == 'O', tag, "the old member was not replaced");
}

/* ---- E / S: write until full, then __fabandon() ------------------------ */
static void round_w(const char *tag, const char *fn, const char *dsn,
                    const char *mem)
{
    char    dd[9];
    int     abend, arc;

    thefp = opened(tag, fn);
    if (!thefp) return;
    strcpy(dd, thefp->ddname);
    abend = try(writer, thefp);
#ifndef OLDLIB
    wtof("TF705 %s dd=%s recs=%ld write try=%08X ferror=%d fx37=%08X",
         tag, dd, written, abend, ferror(thefp), __fx37(thefp));
#else
    wtof("TF705 %s dd=%s recs=%ld write try=%08X ferror=%d",
         tag, dd, written, abend, ferror(thefp));
#endif
    arc = __fabandon(thefp);
    after(tag, arc, dd, dsn, mem);
}

static void scratch(const char *dsn)
{
    wtof("TF705 remove %s rc=%d", dsn, remove(dsn));
}

int main(int argc, char **argv)
{
    char    mode = (argc > 1 && argv[1][0])
                   ? (char)toupper((unsigned char)argv[1][0]) : 'C';
    char    d1[45], d2[45], fn[60];

    wtof("TF705 START mode %c", mode);
    switch (mode) {
    case 'C':
    case 'D':
    case 'A':
        /* a second PARM letter names the library, so a step that
           abended and left its data set behind does not block the next */
        snprintf(d1, sizeof(d1), "IBMUSER.TF705.%c%s", mode,
                (argc > 1 && argv[1][0]) ? argv[1] + 1 : "");
        if (create(PDS, d1, "5,1,1") || filldir(d1) <= 0) {
            unmeasured++;
            break;
        }
        if (mode == 'C') {
            round_c("C1", d1, "NEW1");
            round_c("C2", d1, "NEW2");
        }
        else if (mode == 'A') {
            round_a("A1", d1, "NEW1");
            round_a("A2", d1, "NEW2");
        }
        else {
            round_d("D1", d1, "NEW1");
            round_d("D2", d1, "NEW2");
        }
        scratch(d1);
        break;

    case 'M':
        snprintf(d1, sizeof(d1), "IBMUSER.TF705.M%s",
                (argc > 1 && argv[1][0]) ? argv[1] + 1 : "");
        if (create(PDS, d1, "5,1,1")) { unmeasured++; break; }
        round_m("M1", d1, "NEW1", 0);
        round_m("M2", d1, "OLD1", 1);
        scratch(d1);
        break;

    case 'E':
        snprintf(d1, sizeof(d1), "IBMUSER.TF705.E1");
        snprintf(d2, sizeof(d2), "IBMUSER.TF705.E2");
        if (create(PDS, d1, "1,1,1") || create(PDS, d2, "1,1,1")) {
            unmeasured++;
            break;
        }
        snprintf(fn, sizeof(fn), "'%s(BIG)'", d1);
        round_w("E1", fn, d1, "BIG");
        snprintf(fn, sizeof(fn), "'%s(BIG)'", d2);
        round_w("E2", fn, d2, "BIG");
        scratch(d1);
        scratch(d2);
        break;

    case 'S':
        snprintf(d1, sizeof(d1), "IBMUSER.TF705.S1");
        snprintf(d2, sizeof(d2), "IBMUSER.TF705.S2");
        if (create(SEQ, d1, "1,0") || create(SEQ, d2, "1,0")) {
            unmeasured++;
            break;
        }
        snprintf(fn, sizeof(fn), "'%s'", d1);
        round_w("S1", fn, d1, NULL);
        snprintf(fn, sizeof(fn), "'%s'", d2);
        round_w("S2", fn, d2, NULL);
        scratch(d1);
        scratch(d2);
        break;

    default:
        wtof("TF705 unknown PARM - use C, D, M, E, S or A");
        unmeasured++;
        break;
    }
    wtof("TF705 END mode %c failed=%d unmeasured=%d", mode, bad, unmeasured);
    return (bad || unmeasured) ? 8 : 0;
}
