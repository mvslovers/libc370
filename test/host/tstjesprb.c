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
 * Cases (7) to (10) are the red->green gates for #23 and #24.  Blocks are
 * allocated on the HEAP for that reason: three of those cases are memory-safety
 * cases, and reading past a stack array quietly "works".  RUN THEM UNDER
 * ASAN or they prove much less than they look like they do - without it, (7),
 * (8b) and (10) pass against the shipping walk of PR #40 as well.
 * ====================================================================
 *
 * Build / run (host; no libc370 headers needed, the parser has none):
 *     cc -std=gnu99 -Wall -Wextra -I ../../src/jes \
 *        -o t tstjesprb.c ../../src/jes/jesprb.c && ./t
 *
 * And the one that actually gates #23/#24:
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address -I ../../src/jes \
 *        -o t tstjesprb.c ../../src/jes/jesprb.c && ./t
 *
 * RC: 0 = all checks passed, 1 = a check failed.  #25, #23, #24.
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

/* start a case from a clean walk state.  The reassembly buffer belongs to the
 * caller (jesprint() frees it at the end of its walk), so a case that left one
 * behind has to free it here - "no leaks, ever" holds for tests too.        */
static void pb_reset(JESPRB *pb)
{
    if (pb->prbuf) free(pb->prbuf);
    memset(pb, 0, sizeof(*pb));
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

/* A fresh zeroed block, on the HEAP and not on the stack, for two reasons:
 * the real one is `calloc`'d too (jesprint.c), and a record that runs past the
 * end has to land in a sanitizer redzone - otherwise reading past a stack
 * array quietly "works" and the truncated-block case proves nothing.  Frees
 * the previous block, so each case starts clean and nothing leaks.         */
static char *blk_renew(char *old)
{
    char *b;

    if (old) free(old);
    b = calloc(1, BLKSIZE);
    if (!b) { printf("out of memory\n"); exit(2); }
    return b;
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

/* a record whose header sits inside the block but whose data does not: it
 * claims len bytes that are not there.  This is what a truncated or malformed
 * block looks like to the walk (#23).                                      */
static void put_trunc(char *blk, unsigned off, unsigned len)
{
    PRLINE *l = (PRLINE *)&blk[off];

    l->len   = (unsigned char)len;
    l->flags = 0;
    l->len2  = (unsigned char)len;
}

static void put_eob(char *blk, unsigned off)
{
    PRLINE *l = (PRLINE *)&blk[off];
    l->len = EOB;
}

int main(void)
{
    char    *blk  = NULL;
    char    *blk2 = NULL;
    JESPRB  pb;
    int     rc;
    unsigned off;

    memset(&pb, 0, sizeof pb);      /* pb_reset() frees, so start it clean */

    printf("TSTJESPRB - libc370 #25: __jesprb() record walk\n\n");

    /* ------------------------------------------------------------------
     * (1) plain records, several per block
     * ---------------------------------------------------------------- */
    printf("(1) plain records\n");
    {
        cap_reset();
        pb_reset(&pb);
        blk = blk_renew(blk);
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
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_line(blk, 10, "HELLO", '1');   /* '1' = skip to channel 1 */
        off = put_line(blk, off, "WORLD", ' ');
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(2) walk returns 0");
        CHECK_EQ(pb.reason, JESPRB_OK, "(2) reason OK");
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
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_span(blk, 10,  "ABCD", FLAG_FIRST,  9, 0);
        off = put_span(blk, off, "EF",   FLAG_MIDDLE, 0, 0);
        off = put_span(blk, off, "GHI",  FLAG_LAST,   0, 0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(3) walk returns 0");
        CHECK_EQ(pb.reason, JESPRB_OK, "(3) reason OK");
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
        pb_reset(&pb);

        blk = blk_renew(blk);
        off = put_span(blk, 10, "ABCD", FLAG_FIRST, 7, 0);
        put_eob(blk, off);

        blk2 = blk_renew(blk2);
        off = put_span(blk2, 10, "EFG", FLAG_LAST, 0, 0);
        put_eob(blk2, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);
        CHECK_EQ(rc, 0, "(4) first block returns 0");
        CHECK_EQ(ncap, 0, "(4) nothing emitted while the line is incomplete");
        CHECK_EQ(pb.linelen, 4, "(4) four bytes held over to the next block");

        rc = __jesprb(blk2, BLKSIZE, &pb, cap_emit, NULL);
        CHECK_EQ(rc, 0, "(4) second block returns 0");
        CHECK_EQ(pb.reason, JESPRB_OK, "(4) reason OK");
        CHECK_EQ(ncap, 1, "(4) one line after the LAST part");
        CHECK_STR(cap[0], caplen[0], "ABCDEFG", "(4) line reassembled across blocks");
    }

    /* ------------------------------------------------------------------
     * (5) EOB as the very first record - no lines, no over-read
     * ---------------------------------------------------------------- */
    printf("(5) immediate EOB\n");
    {
        cap_reset();
        pb_reset(&pb);
        blk = blk_renew(blk);
        put_eob(blk, 10);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(5) walk returns 0");
        CHECK_EQ(pb.reason, JESPRB_OK, "(5) reason OK");
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
        pb_reset(&pb);
        blk = blk_renew(blk);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(6) walk returns 0 and terminates");
        CHECK_EQ(pb.reason, JESPRB_OK,
                 "(6) reason OK - a padded tail is an ordinary end, not TRUNC");
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
        pb_reset(&pb);
        emit_stop_at = 1;               /* the second line says stop        */
        emit_rc      = -77;
        blk = blk_renew(blk);
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

    /* ------------------------------------------------------------------
     * (7) truncated block - #23.  The last record's HEADER is inside the
     *     block, its data is not.  The walk must notice before it reads
     *     anything it does not own, keep the lines it already had, and
     *     report; the chain itself is intact.
     *
     *     RED before the fix: the loop tested `line->len != EOB` before
     *     `p < eob`, so the header at the very end was dereferenced and the
     *     record's 200 bytes were read off the end of the block.
     * ---------------------------------------------------------------- */
    printf("(7) truncated block - last record runs past the end\n");
    {
        cap_reset();
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_line(blk, 10,  "GOOD1", 0);
        off = put_line(blk, off, "GOOD2", 0);
        put_trunc(blk, BLKSIZE - 6, 200);   /* header fits, 200 bytes do not */

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(7) walk returns 0 - the chain is intact");
        CHECK_EQ(pb.reason, JESPRB_TRUNC, "(7) reason TRUNC");
        CHECK_EQ(ncap, 2, "(7) the two complete lines were kept");
        CHECK_STR(cap[1], caplen[1], "GOOD2", "(7) last complete line intact");
    }

    /* ------------------------------------------------------------------
     * (8) MIDDLE/LAST with no FIRST - #24.  Two shapes; the second is the
     *     dangerous one.
     *
     *     (8a) nothing was ever assembled: prbuf is NULL.
     *     (8b) a spanned line COMPLETED earlier, so prbuf still exists and
     *          is sized for THAT line.  A MIDDLE arriving afterwards used to
     *          copy into it - RED: 60 bytes into a buffer sized 4+4.
     * ---------------------------------------------------------------- */
    printf("(8) spanned part with no FIRST opening the line\n");
    {
        cap_reset();
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_span(blk, 10, "XY", FLAG_LAST, 0, 0);   /* no FIRST at all */
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(8a) walk returns 0 - the chain is intact");
        CHECK_EQ(pb.reason, JESPRB_NOBUF, "(8a) reason NOBUF");
        CHECK_EQ(ncap, 0, "(8a) nothing emitted");

        cap_reset();
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_span(blk, 10,  "AB", FLAG_FIRST, 4, 0); /* completes below */
        off = put_span(blk, off, "CD", FLAG_LAST,  0, 0);
        off = put_span(blk, off, "0123456789012345678901234567890123456789"
                                 "01234567890123456789", FLAG_MIDDLE, 0, 0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(8b) walk returns 0 - the chain is intact");
        CHECK_EQ(pb.reason, JESPRB_NOBUF, "(8b) reason NOBUF");
        CHECK_EQ(ncap, 1, "(8b) only the completed line was emitted");
        CHECK_STR(cap[0], caplen[0], "ABCD", "(8b) completed line intact");
        CHECK_EQ(pb.linelen, 0, "(8b) nothing appended to the stale buffer");
    }

    /* ------------------------------------------------------------------
     * (9) the parts add up to more than the FIRST part announced - #24.
     *     RED: the length check happened AFTER the copy, so 60 bytes went
     *     into a buffer sized for a 6-byte line.  Now the part is refused
     *     before the copy, what was assembled is handed out as a fragment
     *     (better a visible truncation than a line that disappears), and
     *     the block is given up.
     * ---------------------------------------------------------------- */
    printf("(9) spanned parts exceed the announced total\n");
    {
        cap_reset();
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_span(blk, 10,  "ABCD", FLAG_FIRST, 6, 0);   /* announces 6 */
        off = put_span(blk, off, "0123456789012345678901234567890123456789"
                                 "01234567890123456789", FLAG_MIDDLE, 0, 0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(9) walk returns 0 - the chain is intact");
        CHECK_EQ(pb.reason, JESPRB_NOBUF, "(9) reason NOBUF");
        CHECK_EQ(ncap, 1, "(9) the assembled fragment was handed out");
        CHECK_STR(cap[0], caplen[0], "ABCD", "(9) fragment is what had arrived");
        CHECK_EQ(pb.linelen, 0, "(9) reassembly state dropped");
    }

    /* ------------------------------------------------------------------
     * (10) a block that gives up mid-line must not leave the next block
     *      appending across the gap - #24.  Block N opens a line and then
     *      runs into a truncated record; block N+1 carries the LAST part.
     *      Without the state drop the two halves are stitched together into
     *      a line that never existed.
     * ---------------------------------------------------------------- */
    printf("(10) reassembly state is dropped when a block gives up\n");
    {
        cap_reset();
        pb_reset(&pb);

        blk = blk_renew(blk);
        put_span(blk, 10, "ABCD", FLAG_FIRST, 20, 0);
        put_trunc(blk, BLKSIZE - 6, 200);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(10) first block returns 0");
        CHECK_EQ(pb.reason, JESPRB_TRUNC, "(10) reason TRUNC");
        CHECK_EQ(ncap, 1, "(10) the half-assembled line went out as a fragment");
        CHECK_STR(cap[0], caplen[0], "ABCD", "(10) fragment is what had arrived");
        CHECK_EQ(pb.assembling, 0, "(10) the line is no longer open");
        CHECK_EQ(pb.linelen, 0, "(10) assembled bytes dropped");

        blk2 = blk_renew(blk2);
        off = put_span(blk2, 10, "EFG", FLAG_LAST, 0, 0);
        put_eob(blk2, off);

        rc = __jesprb(blk2, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(10) second block returns 0");
        CHECK_EQ(pb.reason, JESPRB_NOBUF, "(10) its LAST part has no line to join");
        CHECK_EQ(ncap, 1, "(10) nothing stitched across the gap");
    }

    /* ------------------------------------------------------------------
     * (11) the clamp has to be THIS line's announced total, not the
     *      largest one seen so far - #24.  The reassembly buffer only ever
     *      grows, so a short line arriving after a long one would otherwise
     *      be measured against the long one's size and could overrun its
     *      own announcement by thousands of bytes without anyone noticing.
     * ---------------------------------------------------------------- */
    printf("(11) clamp is the current line's total, not the high-water mark\n");
    {
        cap_reset();
        pb_reset(&pb);

        blk = blk_renew(blk);
        off = put_span(blk, 10,  "AB", FLAG_FIRST, 3000, 0);  /* buffer -> 3000 */
        off = put_span(blk, off, "CD", FLAG_LAST,  0, 0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);
        CHECK_EQ(rc, 0, "(11) the long line completes");
        CHECK_STR(cap[0], caplen[0], "ABCD", "(11) long line intact");

        blk = blk_renew(blk);
        off = put_span(blk, 10,  "ABCD", FLAG_FIRST, 6, 0);   /* announces 6 */
        off = put_span(blk, off, "0123456789012345678901234567890123456789"
                                 "01234567890123456789", FLAG_MIDDLE, 0, 0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cap_emit, NULL);

        CHECK_EQ(rc, 0, "(11) walk returns 0 - the chain is intact");
        CHECK_EQ(pb.reason, JESPRB_NOBUF,
                 "(11) 64 bytes into a line that announced 6 is refused");
        CHECK_EQ(ncap, 2, "(11) only the fragment followed the long line");
        CHECK_STR(cap[1], caplen[1], "ABCD", "(11) fragment is what had arrived");
    }

    if (pb.prbuf) free(pb.prbuf);
    if (blk)  free(blk);
    if (blk2) free(blk2);

    return mbt_test_summary("TSTJESPRB");
}
