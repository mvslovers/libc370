/*
 * tstjeslg.c - libc370 #4 diagnostic probe (MVS target, batch).
 *
 * ISSUE #4: jesprint() returns ZERO lines for the SYSLOG spool datasets, even
 * though jesjob() finds the SYSLOG STC and its DDs carry a non-zero PDBMTTR.
 *
 * When this probe was written, jesprint()'s block loop had FOUR ways to produce
 * no output and ALL FOUR returned rc=0 - "success, nothing printed".  From the
 * outside they were indistinguishable, which is why the issue was stuck:
 *
 *   (1) spool_read() fails                     break, rc=0
 *   (2) block jobkey != job->jobkey            break, rc=0
 *   (3) block dsid   != jesdd->dsid            break, rc=0
 *   (4) all three OK, but the record loop emits nothing
 *
 * Issues #21/#22 fixed exactly that: jesprint() now reports the outcome of its
 * walk through a JESPRST out-parameter (JESPR_END / _EMPTY / _IOERR / _FOREIGN
 * / _DSID / _LOOP / _CAP / _STOPPED) and no longer follows the chain unbounded.
 * PARM=',PRINT' drives the real jesprint() so its report can be compared with
 * what this probe reconstructs independently.
 *
 * This probe does exactly what jesprint() does - jesjob() -> follow jesdd->mttr
 * through block->next with spool_read() - but REPORTS every step instead of
 * breaking silently.  It prints, per DD:
 *
 *   - the PDDB view jesjob() built (dsid, mttr, records, lrecl, recfm, flags)
 *   - per block: the raw MTTR (M/TT/R), the spool_read() rc, the 10-byte block
 *     header (next / jobkey / dsid) and MATCH/MISMATCH against the job
 *   - a hex+char dump of the first blocks of each DD (the record data itself)
 *   - the number of print records the jesprint() record loop would find
 *   - the exact reason the walk stopped
 *
 * It changes nothing: read-only, no library code is modified, and it needs no
 * new libc370 API - everything it calls is public.  It deliberately does NOT
 * call jesopen(): that wraps the two opens in try()/ESTAE and reports failures
 * with wtof(), i.e. to the console - which on this system means the SYSLOG we
 * cannot read.  Opening HASPCKPT / HASPACE1 here puts every failure in
 * SYSPRINT instead.
 *
 * PARM='<jobname>[,IOT|,SCAN|,PRINT]' - jobname is a jesjob() FILTER_JOBNAME
 * pattern (default SYSLOG):
 *
 *   'SYSLOG'        walk every DD's block chain (above)
 *   'SYSLOG,IOT'    + the raw IOT chain: header, track group map, and every
 *                     PDDB at the offset jesjob.c uses (cp->pddb1)
 *   'SYSLOG,SCAN'   + brute-force scan of the whole spool volume for blocks
 *                     carrying this job's jobkey (~66k reads; needs TIME=1440)
 *   'SYSLOG,PRINT'  + call the real jesprint() per DD and print its JESPRST:
 *                     stop reason, blocks accepted, lines emitted, stop MTTR.
 *                     Red  case: a purged data set -> reason=FOREIGN, lines=0
 *                     Green case: a held generation -> reason=END, lines=n
 *
 * Always run a CONTROL alongside: 'HTTPD' (an active STC whose SYSOUT
 * jesprint() prints correctly) and a finished batch job.  The comparison is
 * what makes a finding conclusive - see doc/jes-syslog-issue4.md.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstjeslg.c -flinker-output=iebcopy -o TSTJESLG
 *     ld370 --pack TSTJESLG.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN (MVS batch) - see jcl/tstjeslg.jcl.  Needs the two JES2 data sets on DDs
 * HASPCKPT / HASPACE1 (the DDs jesopen() expects), SYSPRINT, and REGION=4M -
 * the checkpoint buffer alone is 140 KB and the 512 K default region fails.
 *
 * RC: 0 = probe ran, 8 = an open or jesjob() failed (nothing to look at).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hasphct.h"    /* JES Checkpoint Control Table                     */
#include "hasppddb.h"   /* JES PDDB                                         */
#include "haspiot.h"    /* JES IOT (+ track group map)                      */
#include "clibjes2.h"   /* jesopen/jesjob/jesprint                          */
#include "clibary.h"    /* arraycount                                       */

#define MAXBLK      500 /* chain-follow cap: a stale block can chain wildly */
#define DUMPBLKS    2   /* hex dump this many blocks per DD                 */
#define DUMPLEN     96  /* bytes per hex dump                               */
#define SCANHDR     64  /* scan: match the jobkey in this many leading bytes */
#define MAXR        8   /* scan: records per track to try (R=5 DOES occur -
                           0001CC05 / 00002805; reads past the end just fail) */

/* the 10-byte spool block header jesprint.c's PRBLOCK assumes */
#define BLK_NEXT(b) (*(unsigned int   *)&(b)[0])
#define BLK_KEY(b)  (*(unsigned int   *)&(b)[4])
#define BLK_DSID(b) (*(unsigned short *)&(b)[8])
#define BLK_DATA    10

/* PRLINE flags, mirrored from jesprint.c */
#define EOB         0xff
#define FLAG_HASCC  0x80
#define FLAG_SPAN   0x10
#define FLAG_FIRST  0x08
#define FLAG_LAST   0x02

static void dump(const unsigned char *p, unsigned len);
static unsigned scanblk(const unsigned char *buf, unsigned bufsize, unsigned *nspan);
static void walk(HASPJS *js, unsigned char *buf, unsigned bufsize,
                 JESJOB *job, JESDD *dd);
static void dumpiot(HASPCP *cp, HASPJS *js, unsigned char *buf, unsigned bufsize,
                    JESJOB *job, unsigned start, const char *what);
static void scanspool(HASPJS *js, unsigned char *buf, unsigned bufsize,
                      JESJOB *job);
static char prt(unsigned char c);

/* ,PRINT mode - context handed to the jesprint() callback through its void
   *arg parameter.  Before that parameter existed every consumer had to route
   this through the per-task GRT (grtapp1..3); see mvsmf and ftpd. */
typedef struct prctx {
    unsigned    lines;              /* lines the callback actually saw       */
    unsigned    chars;              /* bytes the callback actually saw       */
    unsigned    shown;              /* lines echoed so far                   */
} PRCTX;

#define PRSHOW      5               /* echo this many lines per DD, then count */

static int prline(const char *line, unsigned linelen, void *arg);
static const char *prreason(int reason);

int main(int argc, char **argv)
{
    JES         *jes    = NULL;
    JESJOB      **jobs  = NULL;
    HASPCP      *cp     = NULL;
    HASPJS      *js     = NULL;
    __HCT       *hct    = NULL;
    unsigned char *buf  = NULL;
    const char  *filter = "SYSLOG";
    unsigned    bufsize;
    unsigned    njobs;
    unsigned    ndd;
    unsigned    j;
    unsigned    d;
    int         iotdump = 0;
    int         scan    = 0;
    int         useapi  = 0;
    int         rc      = 8;
    char        filt[12];

    /* PARM='<jobname>[,IOT]' - ,IOT adds the raw IOT / PDDB / track-group-map
       dump (verbose; use it on one job at a time) */
    if (argc > 1 && argv[1] && argv[1][0] > ' ') {
        unsigned i;

        for (i = 0; i < sizeof(filt) - 1 && argv[1][i] && argv[1][i] != ','; i++) {
            filt[i] = argv[1][i];
        }
        filt[i] = 0;
        if (argv[1][i] == ',') {
            iotdump = 1;
            if (argv[1][i+1] == 'S') { iotdump = 0; scan = 1; }
            if (argv[1][i+1] == 'P') { iotdump = 0; useapi = 1; }
        }
        filter = filt;
    }

    printf("TSTJESLG - libc370 #4 probe, jobname filter '%s'%s%s%s\n",
           filter, iotdump ? " (+IOT dump)" : "", scan ? " (+spool scan)" : "",
           useapi ? " (+jesprint API)" : "");

    /* jesopen() would do this, but it wraps everything in try()/ESTAE and
       reports failures with wtof() - i.e. to the console, which on this system
       means the SYSLOG we cannot read.  Open the two data sets ourselves so
       every failure lands in SYSPRINT. */
    cp = checkpoint_open("DD:HASPCKPT");
    printf("checkpoint_open('DD:HASPCKPT') -> %08X\n", (unsigned) cp);
    if (!cp) {
        printf("  FAILED: OPEN rejected the DCB, or GETMAIN failed (REGION?)\n");
        goto quit;
    }

    hct = &cp->hct;
    printf("  HCT bufsize=%u maxjobs=%u numjoes=%u numtgv=%u numda=%u\n",
           hct->_BUFSIZE, hct->_MAXJOBS, hct->_NUMJOES,
           hct->_NUMTGV, hct->_NUMDA);
    printf("  pddb1=%u jqeblks=%u jotblks=%u buflen=%u buf=%08X\n",
           cp->pddb1, cp->jqeblks, cp->jotblks, cp->buflen, (unsigned) cp->buf);
    if (!cp->buf) {
        printf("  FAILED: checkpoint buffer not allocated (REGION too small)\n");
        goto quit;
    }

    js = spool_open("DD:HASPACE1");
    printf("spool_open('DD:HASPACE1') -> %08X\n", (unsigned) js);
    if (!js) {
        printf("  FAILED: OPEN rejected the spool DCB\n");
        goto quit;
    }
    printf("  spool ddname=%-8.8s trkcyl=%u\n", js->ddname, js->trkcyl);

    /* hand-build the JES handle jesjob() expects (same shape jesopen() builds) */
    jes = calloc(1, sizeof(JES));
    if (!jes) {
        printf("calloc(JES) failed\n");
        goto quit;
    }
    strcpy(jes->eye, JES_EYE);
    jes->cp = cp;
    arrayadd(&jes->js, js);

    bufsize = hct->_BUFSIZE;

    buf = calloc(1, bufsize);
    if (!buf) {
        printf("calloc(%u) failed\n", bufsize);
        goto quit;
    }

    jobs = jesjob(jes, filter, FILTER_JOBNAME, 1);
    njobs = arraycount(&jobs);
    printf("jesjob('%s', FILTER_JOBNAME, dd=1) -> %u job(s)\n\n", filter, njobs);

    rc = 0;

    for (j = 0; j < njobs; j++) {
        JESJOB *job = jobs[j];

        if (!job) continue;

        printf("JOB %-8.8s %-8.8s owner=%-8.8s type=%02X f1=%02X f2=%02X\n",
               job->jobname, job->jobid, job->owner,
               job->q_type, job->q_flag1, job->q_flag2);
        printf("    jobkey=%08X iotmttr=%08X spinmttr=%08X jtflg=%02X\n",
               job->jobkey, job->iotmttr, job->spinmttr, job->jtflg);

        ndd = arraycount(&job->jesdd);
        printf("    %u DD(s)\n", ndd);

        for (d = 0; d < ndd; d++) {
            JESDD *dd = job->jesdd[d];

            if (!dd) continue;

            printf("\n  DD %-8.8s dsid=%-5u mttr=%08X records=%-6u lrecl=%-5u"
                   " recfm=%02X class=%c flag=%02X\n",
                   dd->ddname, dd->dsid, dd->mttr, dd->records, dd->lrecl,
                   dd->recfm, prt(dd->oclass), dd->flag);
            printf("     dsname=%-44.44s\n", dd->dsname);

            if (!dd->mttr) {
                printf("     -> mttr=0, jesprint() reports JESPR_EMPTY\n");
                continue;
            }

            walk(js, buf, bufsize, job, dd);

            /* ,PRINT - drive the real jesprint() and report what it now says
               about its own walk.  This is the red/green case for #21:
                 red   a purged data set  -> reason=FOREIGN, lines=0
                 green a held generation  -> reason=END,     lines=n      */
            if (useapi) {
                PRCTX   ctx;
                JESPRST st;
                int     prc;

                memset(&ctx, 0, sizeof(ctx));
                prc = jesprint(jes, job, dd->dsid, prline, &ctx, &st);

                printf("     jesprint() rc=%d  reason=%s\n",
                       prc, prreason(st.reason));
                printf("       blocks=%u lines=%u stopmttr=%08X prtrc=%d"
                       "   callback saw %u line(s) / %u char(s)\n",
                       st.blocks, st.lines, st.mttr, st.prtrc,
                       ctx.lines, ctx.chars);
                /* the counters agree by construction - what they prove is
                   that arg reached every call intact, not that the line
                   count is independently correct */
                if (st.lines != ctx.lines) {
                    printf("       *** arg did not reach every callback\n");
                }
            }
        }

        if (iotdump) {
            dumpiot(cp, js, buf, bufsize, job, job->iotmttr, "IOT");
            dumpiot(cp, js, buf, bufsize, job, job->spinmttr, "SPIN IOT");
        }
        if (scan) { scanspool(js, buf, bufsize, job); break; }  /* one job */
        printf("\n");
    }

quit:
    if (buf) free(buf);
    if (jobs) jesjobfr(&jobs);
    if (jes) {
        jesclose(&jes);         /* closes cp + js as well */
    }
    else {
        if (js) spool_close(js);
        if (cp) checkpoint_close(cp);
    }
    printf("TSTJESLG rc=%d\n", rc);
    return rc;
}

/* follow the spool block chain exactly like jesprint()'s block loop, but report
   every decision instead of breaking silently */
static void walk(HASPJS *js, unsigned char *buf, unsigned bufsize,
                 JESJOB *job, JESDD *dd)
{
    const char  *stop   = "chain end (next=0)";
    unsigned    mttr    = dd->mttr;
    unsigned    blocks  = 0;
    unsigned    lines   = 0;
    unsigned    spans   = 0;
    unsigned    next;
    unsigned    key;
    unsigned    dsid;
    unsigned    n;
    unsigned    ns;
    int         rc;

    while (mttr) {
        if (blocks >= MAXBLK) { stop = "block cap reached (chain too long/looping)"; break; }

        memset(buf, 0, bufsize);
        rc = spool_read(js, mttr, buf, bufsize);

        if (blocks < DUMPBLKS || rc) {
            printf("     blk %-4u mttr=%08X M=%02X TT=%u R=%u spool_read rc=%d\n",
                   blocks, mttr, (mttr >> 24) & 0xFF, (mttr >> 8) & 0xFFFF,
                   mttr & 0xFF, rc);
        }
        if (rc) { stop = "spool_read() FAILED  -> JESPR_IOERR"; break; }

        next = BLK_NEXT(buf);
        key  = BLK_KEY(buf);
        dsid = BLK_DSID(buf);
        n    = scanblk(buf, bufsize, &ns);

        if (blocks < DUMPBLKS) {
            printf("       hdr next=%08X jobkey=%08X (job %08X %s)"
                   " dsid=%u (dd %u %s)\n",
                   next, key, job->jobkey,
                   key == job->jobkey ? "MATCH" : "MISMATCH",
                   dsid, dd->dsid,
                   dsid == dd->dsid ? "MATCH" : "MISMATCH");
            printf("       records in block: %u (%u spanned part(s))\n", n, ns);
            dump(buf, bufsize < DUMPLEN ? bufsize : DUMPLEN);
        }

        if (key != job->jobkey) { stop = "jobkey MISMATCH  -> JESPR_FOREIGN"; break; }
        if (dsid != dd->dsid)   { stop = "dsid MISMATCH  -> JESPR_DSID"; break; }

        blocks++;
        lines += n;
        spans += ns;

        if (next == mttr) { stop = "block chains to ITSELF"; break; }
        mttr = next;
    }

    printf("     WALK: %u block(s) accepted, %u record(s) (%u spanned part(s))\n",
           blocks, lines, spans);
    printf("     STOP: %s\n", stop);
    printf("     => jesprint() would print %u line(s) for this DD\n", lines);
}

/* Brute-force scan of the spool volume for blocks that carry this job's
   jobkey.  This answers the question the PDDB cannot: are there data blocks
   for this job at all, and at which MTTRs?  Reads are sequential by track;
   past the end of the HASPACE extent every read fails, which is how the scan
   finds its own end. */
static void scanspool(HASPJS *js, unsigned char *buf, unsigned bufsize,
                      JESJOB *job)
{
    unsigned    tt;
    unsigned    r;
    unsigned    mttr;
    unsigned    hits    = 0;
    unsigned    reads   = 0;
    unsigned    lasttt  = 0;
    unsigned    dead    = 0;

    printf("  SCAN of the spool for jobkey=%08X ...\n", job->jobkey);
    printf("  (the key is matched at every fullword offset in the first %u"
           " bytes of a block:\n"
           "   data blocks carry it at +4, JCT/IOT control records at +8)\n",
           SCANHDR);

    for (tt = 0; tt < 65535 && dead < 100; tt++) {
        unsigned trkok = 0;

        for (r = 1; r <= MAXR; r++) {
            unsigned off;
            unsigned found = 0xFFFFFFFF;

            mttr = (tt << 8) | r;
            if (spool_read(js, mttr, buf, bufsize)) continue;
            reads++;
            trkok = 1;

            for (off = 0; off + 4 <= SCANHDR && off + 4 <= bufsize; off += 4) {
                if (*(unsigned *)&buf[off] == job->jobkey) { found = off; break; }
            }
            if (found == 0xFFFFFFFF) continue;

            hits++;
            if (hits <= 24) {
                unsigned ns;
                printf("    HIT mttr=%08X TT=%u R=%u key@+%u  +0=%08X"
                       " +4=%08X dsid=%u records=%u\n",
                       mttr, tt, r, found, BLK_NEXT(buf), BLK_KEY(buf),
                       BLK_DSID(buf), scanblk(buf, bufsize, &ns));
                if (hits <= 3) dump(buf, 32);
            }
        }
        if (trkok) { dead = 0; lasttt = tt; }
        else       { dead++; }
    }

    printf("  SCAN done: %u block(s) read up to TT=%u, %u block(s) carry"
           " jobkey %08X\n", reads, lasttt, hits, job->jobkey);
    if (!hits) {
        printf("  NOTE: zero hits is only meaningful if the same scan finds"
               " the control job's\n        blocks - run PARM='HTTPD,SCAN'"
               " and compare.\n");
    }
}

/* dump the raw IOT chain: header, track group map, and every PDDB at the
   offset jesjob.c uses (cp->pddb1).  This is what says whether a PDDB's
   PDBMTTR points INSIDE the job's own track group allocation - if it does not,
   the PDDB is stale and jesprint() is reading somebody else's block. */
static void dumpiot(HASPCP *cp, HASPJS *js, unsigned char *buf, unsigned bufsize,
                    JESJOB *job, unsigned start, const char *what)
{
    __IOT       *iot    = (__IOT *) buf;
    __PDDB      *pddb;
    unsigned char *tgm;
    unsigned    tgbytes = ((cp->hct._NUMTGV + 7) / 8);   /* per volume       */
    unsigned    mttr;
    unsigned    n;
    unsigned    i;
    unsigned    set;

    if (!start) {
        printf("    %s: none (mttr=0)\n", what);
        return;
    }

    for (mttr = start, n = 0; mttr && n < 8; n++) {
        if (spool_read(js, mttr, buf, bufsize)) {
            printf("    %s #%u mttr=%08X: spool_read FAILED\n", what, n, mttr);
            break;
        }

        printf("    %s #%u mttr=%08X id=%-4.4s leng=%u flag1=%02X flag2=%02X\n",
               what, n, mttr, iot->IOTID, iot->IOTLENG,
               iot->IOTFLAG1, iot->IOTFLAG2);
        printf("      jbkey=%08X (job %08X %s) iottrack=%08X next=%08X"
               " spiot=%08X pddbp=%u sjb=%08X\n",
               iot->IOTJBKEY, job->jobkey,
               iot->IOTJBKEY == job->jobkey ? "MATCH" : "MISMATCH",
               iot->IOTTRACK, iot->IOTIOTTR, iot->IOTSPIOT,
               iot->IOTPDDBP, iot->IOTSJB);
        dump(buf, 64);

        /* track group map: one bit per track group, per spool volume */
        tgm = &buf[0x40];
        printf("      TGMAP (%u byte(s)/vol, %u vol(s)), groups held:",
               tgbytes, cp->hct._NUMDA);
        for (i = 0, set = 0; i < tgbytes * 8 * cp->hct._NUMDA; i++) {
            if (&tgm[i/8] >= &buf[bufsize]) break;
            if (tgm[i/8] & (0x80 >> (i % 8))) {
                if (set < 40) printf(" %u/%u", i / (tgbytes * 8), i % (tgbytes * 8));
                set++;
            }
        }
        printf("%s  (%u total)\n", set > 40 ? " ..." : "", set);

        /* the PDDBs, at the offset jesjob.c:178 computes */
        for (i = 0; ; i++) {
            unsigned off = cp->pddb1 + i * PDBLENG;

            if (off + PDBLENG > bufsize) break;
            pddb = (__PDDB *) &buf[off];
            if (pddb->PDBDSKEY == 0) break;

            printf("      PDDB#%-2u +%-4u dskey=%-5u mttr=%08X recct=%-6u"
                   " lrecl=%-5u recfm=%02X class=%c f1=%02X f2=%02X\n",
                   i, off, pddb->PDBDSKEY, pddb->PDBMTTR, pddb->PDBRECCT,
                   pddb->PDBLRECL, pddb->PDBRECFM, prt(pddb->PDBCLASS),
                   pddb->PDBFLAG1, pddb->PDBFLAG2);
        }

        mttr = iot->IOTIOTTR;
    }
}

/* count the print records jesprint()'s record loop would find in this block.
   Bounds are checked BEFORE every dereference (jesprint.c tests line->len
   first and p < eob second - see the analysis doc). */
static unsigned scanblk(const unsigned char *buf, unsigned bufsize, unsigned *nspan)
{
    const unsigned char *p   = &buf[BLK_DATA];
    const unsigned char *end = &buf[bufsize];
    unsigned            n    = 0;

    *nspan = 0;

    while (p + 3 <= end) {
        unsigned len   = p[0];
        unsigned flags = p[1];

        if (len == EOB) break;

        if (flags & FLAG_SPAN) {
            unsigned len2;

            if (p + 4 > end) break;
            len2 = (unsigned) ((p[2] << 8) | p[3]);
            (*nspan)++;
            if (flags & FLAG_LAST) n++;
            p += 4 + len2;
            continue;
        }

        n++;
        p += 3 + len;

        /* a zero-length, zero-flag record is fill, not data: jesprint() walks
           it 3 bytes at a time and prints nothing - report it as such */
        if (len == 0 && flags == 0) { n--; }
    }

    return n;
}

static char prt(unsigned char c)
{
    return (char) (isprint(c) ? c : ' ');
}

static void dump(const unsigned char *p, unsigned len)
{
    char        hex[40];
    char        chr[20];
    unsigned    i;
    unsigned    j;

    for (i = 0; i < len; i += 16) {
        unsigned n = (len - i) < 16 ? (len - i) : 16;

        for (j = 0; j < 16; j++) {
            if (j < n) sprintf(&hex[j*2], "%02X", p[i+j]);
            else       sprintf(&hex[j*2], "  ");
            chr[j] = j < n ? prt(p[i+j]) : ' ';
        }
        hex[32] = 0;
        chr[16] = 0;
        printf("       +%03X %-32.32s |%-16.16s|\n", i, hex, chr);
    }
}

/* ,PRINT - the jesprint() callback.  Everything it needs arrives through
   arg; nothing is fished out of thread-global storage. */
static int prline(const char *line, unsigned linelen, void *arg)
{
    PRCTX *ctx = (PRCTX *) arg;

    if (!ctx) return 0;             /* would be a bug in the arg plumbing */

    ctx->lines++;
    ctx->chars += linelen;

    if (ctx->shown < PRSHOW) {
        ctx->shown++;
        printf("       | %-*.*s\n", linelen, linelen, line);
    }
    else if (ctx->shown == PRSHOW) {
        ctx->shown++;
        printf("       | ... (further lines counted, not echoed)\n");
    }

    return 0;
}

static const char *prreason(int reason)
{
    switch (reason) {
    case JESPR_END:     return "END      chain end, data set read in full";
    case JESPR_EMPTY:   return "EMPTY    PDDB carries no MTTR";
    case JESPR_IOERR:   return "IOERR    spool_read() failed";
    case JESPR_FOREIGN: return "FOREIGN  foreign block - data set purged?";
    case JESPR_DSID:    return "DSID     block belongs to another dsid";
    case JESPR_LOOP:    return "LOOP     block chains to itself";
    case JESPR_CAP:     return "CAP      iteration cap hit, walk truncated";
    case JESPR_STOPPED: return "STOPPED  callback asked to stop";
    case JESPR_NOBUF:   return "NOBUF    spanned part with no FIRST part";
    case JESPR_NOMEM:   return "NOMEM    buffer allocation failed";
    case JESPR_OPENEND: return "OPENEND  end of an OPEN data set (normal)";
    }
    return "?        unknown reason";
}
