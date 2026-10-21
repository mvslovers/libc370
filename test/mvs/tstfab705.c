/*
 * tstfab705.c - libc370 #705 on MVS: what __fabandon() returns, and
 * whether the DD is really still held, when the CLOSE it issues abends.
 *
 * MEASUREMENT ONLY.  There is no green or red: every case WTOs what it
 * saw, and the step ends RC 0 unless a case could not be set up.
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
 *   D  direct:   __fabandon() instead of fclose() - its own CLOSE STOWs
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
static FILE *thefp;
static long  written;
static int   nrecs = 10;        /* records the C and D rounds write       */

int main(int argc, char **argv);

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

        sprintf(name, "F%04d   ", n);
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
    for (i = 0; i < 0x34; i++) sprintf(hex + 2 * i, "%02X", d[i]);
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

/* What __fabandon() answered, and the DD and member afterwards */
static void after(const char *tag, int arc, const char *dd,
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

    if (mem) {
        sprintf(fn, "'%s(%s)'", dsn, mem);
        rf = fopen(fn, "rb");
        wtof("TF705 %s member %s %s", tag, mem, rf ? "EXISTS" : "absent");
        if (rf) fclose(rf);
    }
}

/* ---- C: fclose() under try(), then __fabandon() ------------------------ */
static void round_c(const char *tag, const char *dsn, const char *mem)
{
    char    fn[60], dd[9];
    int     abend, arc;

    sprintf(fn, "'%s(%s)'", dsn, mem);
    thefp = fopen(fn, "wb");
    if (!thefp) { wtof("TF705 %s fopen %s NULL", tag, fn); unmeasured++; return; }
    strcpy(dd, thefp->ddname);
    abend = try(fewrecs, thefp);
    wtof("TF705 %s fopen dd=%s write try=%08X", tag, dd, abend);
    dcbdump(tag, thefp);
    abend = try(closer, thefp);
    wtof("TF705 %s fclose under try=%08X", tag, abend);
    if (abend == 0) {
        /* fclose() completed and freed the FILE: nothing to abandon */
        wtof("TF705 %s fclose did NOT abend - no SB14", tag);
        unmeasured++;
        return;
    }
    dcbdump(tag, thefp);
    arc = __fabandon(thefp);
    after(tag, arc, dd, dsn, mem);
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

    sprintf(fn, "'%s(%s)'", dsn, mem);
    thefp = fopen(fn, "wb");
    if (!thefp) { wtof("TF705 %s fopen %s NULL", tag, fn); unmeasured++; return; }
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

    sprintf(fn, "'%s(%s)'", dsn, mem);
    thefp = fopen(fn, "wb");
    if (!thefp) { wtof("TF705 %s fopen %s NULL", tag, fn); unmeasured++; return; }
    strcpy(dd, thefp->ddname);
    abend = try(fewrecs, thefp);
    wtof("TF705 %s fopen dd=%s write try=%08X", tag, dd, abend);
    arc = __fabandon(thefp);
    after(tag, arc, dd, dsn, mem);
}

/* ---- E / S: write until full, then __fabandon() ------------------------ */
static void round_w(const char *tag, const char *fn, const char *dsn,
                    const char *mem)
{
    char    dd[9];
    int     abend, arc;

    thefp = fopen(fn, "wb");
    if (!thefp) { wtof("TF705 %s fopen %s NULL", tag, fn); unmeasured++; return; }
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
        sprintf(d1, "IBMUSER.TF705.%c%s", mode,
                (argc > 1 && argv[1][0]) ? argv[1] + 1 : "");
        if (create(PDS, d1, "5,1,1") || filldir(d1) <= 0) { unmeasured++; break; }
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

    case 'E':
        sprintf(d1, "IBMUSER.TF705.E1");
        sprintf(d2, "IBMUSER.TF705.E2");
        if (create(PDS, d1, "1,1,1") || create(PDS, d2, "1,1,1")) {
            unmeasured++;
            break;
        }
        sprintf(fn, "'%s(BIG)'", d1);
        round_w("E1", fn, d1, "BIG");
        sprintf(fn, "'%s(BIG)'", d2);
        round_w("E2", fn, d2, "BIG");
        scratch(d1);
        scratch(d2);
        break;

    case 'S':
        sprintf(d1, "IBMUSER.TF705.S1");
        sprintf(d2, "IBMUSER.TF705.S2");
        if (create(SEQ, d1, "1,0") || create(SEQ, d2, "1,0")) { unmeasured++; break; }
        sprintf(fn, "'%s'", d1);
        round_w("S1", fn, d1, NULL);
        sprintf(fn, "'%s'", d2);
        round_w("S2", fn, d2, NULL);
        scratch(d1);
        scratch(d2);
        break;

    default:
        wtof("TF705 unknown PARM - use C, D, E or S");
        unmeasured++;
        break;
    }
    wtof("TF705 END mode %c unmeasured=%d", mode, unmeasured);
    return unmeasured ? 8 : 0;
}
