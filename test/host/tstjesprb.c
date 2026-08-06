/*
 * tstjesprb.c - libc370 #25: host regression for the JES2 spool record walk
 * in __jesprb() (src/jes/jesprb.c).
 *
 * Unlike test/host/tstcmtt.c, this test LINKS AND EXECUTES THE REAL CODE.
 * That is the whole point of #25: the walk was lifted out of jesprint.c,
 * which cannot be built on the host (file-scope S/370 asm for the EX/TR
 * translate, and spool_read() is a BDAM READ/CHECK).  What is left in
 * jesprb.c is buffer in / lines out, so a change to the walk that breaks a
 * case below fails here - no mirror to keep in sync.
 *
 * ====================================================================
 * WHAT THESE CASES PIN - AND WHAT THEY DO NOT
 * --------------------------------------------------------------------
 * They pin the walk's contract as the code defines it: block header of 10
 * bytes, PRLINE/SPLINE layouts from jesprb.h, and for a spanned line the
 * layout the parser implies - a FIRST part whose data is [2-byte total]
 * [optional carriage control][len2 payload bytes], with the next record
 * starting len2 bytes after the payload begins.
 *
 * They do NOT prove that real JES2 blocks are laid out that way.  Whether
 * len2 on a FIRST part counts from sp->data (i.e. includes the 2-byte total
 * length) or from after it is exactly the open question in #29, and it can
 * only be settled by a real spanned record captured on the target - case 9
 * of the suite in #25, an MVS fixture, not a host test.  If #29 turns out to
 * be a real defect, the spanned fixtures below (3) and (4) move with the fix.
 *
 * Two cases from #25 are deliberately ABSENT: a truncated block whose last
 * record runs past the end (#23) and a MIDDLE/LAST part with no preceding
 * FIRST (#24).  Both are red today - they are the red->green gates for those
 * two fixes and belong in that change, not in this one.  A committed test
 * suite is green.
 * ====================================================================
 *
 * Build / run (host; no libc370 headers needed, the parser has none):
 *     cc -std=gnu99 -Wall -Wextra -I ../../src/jes \
 *        -o t tstjesprb.c ../../src/jes/jesprb.c && ./t
 * RC: 0 = all checks passed, 1 = a check failed.  #25.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jesprb.h"

/* --------------------------------------------------------------------------
 * Minimal mbtcheck.h-compatible harness, same contract as tstcmtt.c.
 * ------------------------------------------------------------------------ */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                       \
    do {                                       \
        mbt_run++;                             \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define CHECK_EQ(got, want, msg)                                          \
    do {                                                                  \
        long g_ = (long)(got), w_ = (long)(want);                         \
        mbt_run++;                                                        \
        if (g_ == w_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }    \
        else { mbt_failed++;                                              \
               printf("  FAIL: %s (got %ld, want %ld)\n", (msg), g_, w_); }\
    } while (0)

#define CHECK_STR(got, gotlen, want, msg)                                 \
    do {                                                                  \
        size_t w_ = strlen(want);                                         \
        mbt_run++;                                                        \
        if ((size_t)(gotlen) == w_ && !memcmp((got), (want), w_)) {       \
            mbt_passed++; printf("  PASS: %s\n", (msg)); }                \
        else { mbt_failed++;                                              \
               printf("  FAIL: %s (got \"%.*s\" [%u], want \"%s\")\n",    \
                      (msg), (int)(gotlen), (got), (unsigned)(gotlen), (want)); } \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * Capture what the walk emits.
 *
 * The walk hands EVERY record to emit(), including zero-length ones; on MVS
 * esc_print() returns immediately for those and the caller's callback never
 * sees them.  Empty and non-empty calls are therefore counted separately.
 * ------------------------------------------------------------------------ */
#define MAXCAP  16
static char     cap[MAXCAP][512];
static unsigned caplen[MAXCAP];
static int      ncap;               /* non-empty lines captured             */
static int      nempty;             /* emit() calls with linelen == 0       */
static int      emit_stop_at = -1;  /* capture index that returns emit_rc   */
static int      emit_rc = 0;

static int cap_emit(char *line, unsigned linelen, void *arg)
{
    (void)arg;

    if (!linelen) { nempty++; return 0; }

    if (ncap == emit_stop_at) return emit_rc;

    if (ncap < MAXCAP) {
        if (linelen > sizeof(cap[0])) linelen = sizeof(cap[0]);
        memcpy(cap[ncap], line, linelen);
        caplen[ncap] = linelen;
    }
    ncap++;
    return 0;
}

static void cap_reset(void)
{
    memset(cap, 0, sizeof(cap));
    memset(caplen, 0, sizeof(caplen));
    ncap = nempty = 0;
    emit_stop_at = -1;
    emit_rc = 0;
}

/* --------------------------------------------------------------------------
 * Block builders.  Offsets are returned so records can be chained; the block
 * header is 10 bytes and the first record starts there (jesprb.c).
 * ------------------------------------------------------------------------ */
#define BLKSIZE 128

static void blk_init(char *blk)
{
    memset(blk, 0, BLKSIZE);
}

/* a plain print record; cc != 0 prepends a carriage control byte */
static unsigned put_line(char *blk, unsigned off, const char *text, char cc)
{
    PRLINE  *l = (PRLINE *)&blk[off];
    unsigned len = (unsigned)strlen(text);
    unsigned n = 0;

    l->len   = (unsigned char)len;
    l->flags = cc ? FLAG_HASCC : 0;
    l->len2  = (unsigned char)len;
    if (cc) l->data[n++] = (unsigned char)cc;
    memcpy(&l->data[n], text, len);

    return off + sizeof(PRLINE) + n + len;
}

/* one part of a spanned line.  total is only used on a FIRST part (it sizes
 * the reassembly buffer); cc likewise only applies to a FIRST part.        */
static unsigned put_span(char *blk, unsigned off, const char *part,
                         unsigned flags, unsigned total, char cc)
{
    SPLINE  *s = (SPLINE *)&blk[off];
    unsigned len = (unsigned)strlen(part);
    unsigned n = 0;

    s->len   = 0;                       /* the walk ignores len on a span   */
    s->flags = (unsigned char)(FLAG_SPAN | flags);
    s->len2  = (unsigned short)len;
    if (flags & FLAG_FIRST) {
        *(unsigned short *)&s->data[0] = (unsigned short)total;
        n = 2;
        if (cc) { s->flags |= FLAG_HASCC; s->data[n++] = (unsigned char)cc; }
    }
    memcpy(&s->data[n], part, len);

    return off + sizeof(SPLINE) + n + len;
}

static void put_eob(char *blk, unsigned off)
{
    PRLINE *l = (PRLINE *)&blk[off];
    l->len = EOB;
}

int main(void)
{
    char    blk[BLKSIZE];
    char    blk2[BLKSIZE];
    JESPRB  pb;
    int     rc;
    unsigned off;

    printf("TSTJESPRB - libc370 #25: __jesprb() record walk\n\n");

    /* ------------------------------------------------------------------
     * (1) plain records, several per block
     * ---------------------------------------------------------------- */
    printf("(1) plain records\n");
    {
        cap_reset();
        memset(&pb, 0, sizeof pb);
        blk_init(blk);
        off = put_line(blk, 10, "AAA", 0);
        off = put_line(blk, off, "BB",  0);
        off = put_line(blk, off, "C",   0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(1) walk returns 0 - follow the chain");
        CHECK_EQ(pb.reason, JESPRB_OK, "(1) reason OK");
        CHECK_EQ(ncap, 3, "(1) three lines emitted");
        CHECK_EQ(pb.lines, 3, "(1) three lines counted");
        CHECK_STR(cap[0], caplen[0], "AAA", "(1) first line");
        CHECK_STR(cap[1], caplen[1], "BB",  "(1) second line");
        CHECK_STR(cap[2], caplen[2], "C",   "(1) third line");
    }

    /* ------------------------------------------------------------------
     * (2) FLAG_HASCC - the carriage control byte is skipped, not printed
     * ---------------------------------------------------------------- */
    printf("(2) carriage control\n");
    {
        cap_reset();
        memset(&pb, 0, sizeof pb);
        blk_init(blk);
        off = put_line(blk, 10, "HELLO", '1');   /* '1' = skip to channel 1 */
        off = put_line(blk, off, "WORLD", ' ');
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(2) walk returns 0");
        CHECK_EQ(ncap, 2, "(2) two lines emitted");
        CHECK_STR(cap[0], caplen[0], "HELLO", "(2) cc stripped from first line");
        CHECK_STR(cap[1], caplen[1], "WORLD", "(2) cc stripped from second line");
    }

    /* ------------------------------------------------------------------
     * (3) spanned line, FIRST/MIDDLE/LAST inside one block
     * ---------------------------------------------------------------- */
    printf("(3) spanned line within one block\n");
    {
        cap_reset();
        memset(&pb, 0, sizeof pb);
        blk_init(blk);
        off = put_span(blk, 10,  "ABCD", FLAG_FIRST,  9, 0);
        off = put_span(blk, off, "EF",   FLAG_MIDDLE, 0, 0);
        off = put_span(blk, off, "GHI",  FLAG_LAST,   0, 0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(3) walk returns 0");
        CHECK_EQ(ncap, 1, "(3) the three parts make ONE line");
        CHECK_EQ(pb.lines, 1, "(3) one line counted");
        CHECK_STR(cap[0], caplen[0], "ABCDEFGHI", "(3) parts reassembled in order");
        CHECK_EQ(pb.linelen, 0, "(3) reassembly state reset after LAST");
    }

    /* ------------------------------------------------------------------
     * (4) spanned line continuing across two blocks - the reason the
     *     reassembly state lives in JESPRB and not in one call
     * ---------------------------------------------------------------- */
    printf("(4) spanned line across two blocks\n");
    {
        cap_reset();
        memset(&pb, 0, sizeof pb);

        blk_init(blk);
        off = put_span(blk, 10, "ABCD", FLAG_FIRST, 7, 0);
        put_eob(blk, off);

        blk_init(blk2);
        off = put_span(blk2, 10, "EFG", FLAG_LAST, 0, 0);
        put_eob(blk2, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);
        CHECK_EQ(rc, 0, "(4) first block returns 0");
        CHECK_EQ(ncap, 0, "(4) nothing emitted while the line is incomplete");
        CHECK_EQ(pb.linelen, 4, "(4) four bytes held over to the next block");

        rc = __jesprb(blk2, BLKSIZE, &pb, cap_emit, NULL);
        CHECK_EQ(rc, 0, "(4) second block returns 0");
        CHECK_EQ(ncap, 1, "(4) one line after the LAST part");
        CHECK_STR(cap[0], caplen[0], "ABCDEFG", "(4) line reassembled across blocks");
    }

    /* ------------------------------------------------------------------
     * (5) EOB as the very first record - no lines, no over-read
     * ---------------------------------------------------------------- */
    printf("(5) immediate EOB\n");
    {
        cap_reset();
        memset(&pb, 0, sizeof pb);
        blk_init(blk);
        put_eob(blk, 10);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(5) walk returns 0");
        CHECK_EQ(ncap, 0, "(5) no lines emitted");
        CHECK_EQ(nempty, 0, "(5) emit() not called at all");
        CHECK_EQ(pb.lines, 0, "(5) no lines counted");
    }

    /* ------------------------------------------------------------------
     * (6) zero-filled block - terminates, produces no content.  A zero
     *     record is len 0 / flags 0, so the walk steps 3 bytes at a time
     *     and ends on the p < eob bound; it must not emit a line.
     * ---------------------------------------------------------------- */
    printf("(6) zero-filled block\n");
    {
        cap_reset();
        memset(&pb, 0, sizeof pb);
        blk_init(blk);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(6) walk returns 0 and terminates");
        CHECK_EQ(ncap, 0, "(6) no content lines");
        CHECK_EQ(pb.lines, 0, "(6) no lines counted");
        CHECK(nempty > 0, "(6) empty records were walked (and dropped by esc_print on MVS)");
    }

    /* ------------------------------------------------------------------
     * (S) the callback stops the walk.  jesprint() turns this into
     *     JESPR_STOPPED + st->prtrc; since #26 it is the ONLY way a
     *     caller learns the callback gave up, so pin it here.
     * ---------------------------------------------------------------- */
    printf("(S) callback stops the walk\n");
    {
        cap_reset();
        memset(&pb, 0, sizeof pb);
        emit_stop_at = 1;               /* the second line says stop        */
        emit_rc      = -77;
        blk_init(blk);
        off = put_line(blk, 10,  "ONE",   0);
        off = put_line(blk, off, "TWO",   0);
        off = put_line(blk, off, "THREE", 0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK(rc < 0, "(S) walk returns < 0 - stop the whole walk");
        CHECK_EQ(pb.reason, JESPRB_STOPPED, "(S) reason STOPPED");
        CHECK_EQ(pb.prtrc, -77, "(S) callback rc kept verbatim");
        CHECK_EQ(ncap, 1, "(S) only the line before the stop was captured");
        CHECK_EQ(pb.lines, 2, "(S) the stopping line is counted before emit()");
    }

    if (pb.prbuf) free(pb.prbuf);

    return mbt_test_summary("TSTJESPRB");
}
