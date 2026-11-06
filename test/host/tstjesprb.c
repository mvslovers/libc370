/*
 * tstjesprb.c - libc370 #25: host regression for the JES2 spool record walk
 * in __jesprb() (src/mvs/jes2/jesprb.c).
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
 * bytes, PRLINE/SPLINE layouts from jesprb.h, and for a spanned line a FIRST
 * part whose data is [2-byte total][optional carriage control][len2 payload
 * bytes], with the next record starting len2 bytes after the payload begins.
 *
 * Case (12) is the one that proves real JES2 blocks are laid out that way: it
 * is a byte-for-byte reconstruction of a record captured on MVS 3.8j, and it
 * is what settled #29 (len2 counts the payload AFTER the prefix).  Everything
 * else here is synthetic, and cases (3) and (4) hold the shapes that capture
 * confirmed exist - a record spanning two blocks really does arrive as a
 * FIRST part in one and a LAST part in the next.
 *
 * Worth knowing when reading them: "spanned" does not mean "too big for a
 * block".  PRLINE.len is a single byte, so ANY record over 255 bytes takes
 * the spanned form; a 500-byte record that fits its block arrives as one part
 * with FIRST, MIDDLE and LAST all set at once (flags 0x1E).
 *
 * Cases (7) to (11) are the red->green gates for #23 and #24.  Blocks are
 * allocated on the HEAP for that reason: they are memory-safety cases, and
 * reading past a stack array quietly "works".  RUN THEM UNDER ASAN or they
 * prove much less than they look like they do - without it, (7), (8b) and (10)
 * pass against the shipping walk of PR #40 as well.
 * ====================================================================
 *
 * Build / run (host; no libc370 headers needed, the parser has none):
 *     cc -std=gnu99 -Wall -Wextra -I ../../src/jes \
 *        -o t tstjesprb.c ../../src/mvs/jes2/jesprb.c && ./t
 *
 * And the one that actually gates #23/#24:
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address -I ../../src/jes \
 *        -o t tstjesprb.c ../../src/mvs/jes2/jesprb.c && ./t
 *
 * THE SAME SOURCE RUNS ON THE TARGET.  Since #25 the walk needs no assembler
 * and no I/O, so cc370 compiles this file as it stands and a batch job drives
 * it against the same synthetic blocks - see jcl/tstjesprb.jcl.  What that run
 * adds is what a host run cannot give: cc370's code generation, 24-bit
 * pointers under the bounds arithmetic, and the record layouts of case (0).
 * What it does NOT add is the memory-safety verdict - there is no sanitizer on
 * MVS 3.8j, so the host ASan run stays the gate for #23 and #24.
 *
 * Cases (13) to (19) are #760: the control byte the walk reports.
 *
 * RC: 0 = all checks passed, 1 = a check failed.  #25, #23, #24, #760.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "src/mvs/jes2/jesprb.h"

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

/* Cases (13) to (18), #760: what the walk says about the control byte.  Unlike
 * cap_emit this keeps an empty call that has a byte, because that is how a page
 * eject arrives; an empty one without a byte is dropped as esc_print() drops it,
 * since the zeroes behind the last record of a block walk 3 bytes at a time
 * into exactly that.  arg is the JESPRB of the walk.                         */
static unsigned ccval[MAXCAP];
static unsigned cclen[MAXCAP];
static char     cctxt[MAXCAP][16];
static int      ncc;

static int cc_emit(char *line, unsigned linelen, void *arg)
{
    JESPRB *pb = (JESPRB *)arg;

    if (!linelen && !pb->cc) return 0;

    if (ncc < MAXCAP) {
        ccval[ncc] = pb->cc;
        cclen[ncc] = linelen;
        memset(cctxt[ncc], 0, sizeof(cctxt[0]));
        memcpy(cctxt[ncc], line,
               linelen < sizeof(cctxt[0]) ? linelen : sizeof(cctxt[0]) - 1);
    }
    ncc++;
    return 0;
}

/* Case (12) checks a 4000-byte line, which is too big to park in cap[] and
 * whose point is the seam anyway: it verifies the pattern in place instead of
 * copying it out.  A = the FIRST part's payload, a = the LAST part's.       */
#define BIGBLK  3664                /* the JES2 BUFSIZE measured on the target */
static unsigned seam_len;           /* length of the reassembled line        */
static int      seam_ok = -1;       /* 1 = every byte where it belongs       */

static int seam_emit(char *line, unsigned linelen, void *arg)
{
    unsigned i;

    (void)arg;
    if (!linelen) return 0;

    seam_len = linelen;
    seam_ok  = (linelen == 4000);
    for (i = 0; i < 3647 && seam_ok; i++)
        if (line[i] != (char)('A' + (i % 26))) seam_ok = 0;
    for (i = 0; i < 353 && seam_ok; i++)
        if (line[3647+i] != (char)('a' + (i % 26))) seam_ok = 0;

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

/* a plain record with the flags of a real one (#760): the walk reads the byte
 * when FLAG_HASCC is on, and the other two bits say what kind it is         */
static unsigned put_ccl(char *blk, unsigned off, const char *text,
                        unsigned flags, unsigned char cc)
{
    PRLINE  *l = (PRLINE *)&blk[off];
    unsigned len = (unsigned)strlen(text);
    unsigned n = 0;

    l->len   = (unsigned char)len;
    l->flags = (unsigned char)flags;
    l->len2  = (unsigned char)len;
    if (flags & FLAG_HASCC) l->data[n++] = cc;
    memcpy(&l->data[n], text, len);

    return off + sizeof(PRLINE) + n + len;
}

/* what jesccsp() says for one value */
static void check_sp(unsigned cc, int before, int after, const char *msg)
{
    int b = 99, a = 99;

    jesccsp(cc, &b, &a);
    CHECK(b == before && a == after, msg);
    if (b != before || a != after)
        printf("        got before=%d after=%d, want %d, %d\n", b, a, before, after);
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
     * (0) the record layouts the walk measures against.  Every bounds
     *     check below is `p + sizeof(PRLINE) > end` and friends, so a
     *     compiler that pads these differently would move the boundary
     *     the memory-safety cases are testing.  Cheap on the host, the
     *     whole point on the target: only an MVS run says what cc370
     *     makes of them (jcl/tstjesprb.jcl).
     * ---------------------------------------------------------------- */
    printf("(0) record layout\n");
    CHECK_EQ(sizeof(PRLINE), 3, "(0) PRLINE header is 3 bytes");
    CHECK_EQ(sizeof(SPLINE), 4, "(0) SPLINE header is 4 bytes");
    /* NOT sizeof(PRBLOCK): that is 12, because the compiler pads behind the
     * 2-byte dsid.  The block header on the spool is 10 bytes and the walk
     * starts at the literal blk[10]; what has to hold is where the three
     * fields sit, which is what jesprint() reads them at. */
    CHECK_EQ(offsetof(PRBLOCK, next),   0, "(0) block header: next at +0");
    CHECK_EQ(offsetof(PRBLOCK, jobkey), 4, "(0) block header: jobkey at +4");
    CHECK_EQ(offsetof(PRBLOCK, dsid),   8, "(0) block header: dsid at +8");

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

    /* ------------------------------------------------------------------
     * (12) THE REAL THING - a record too long for one block, captured on
     *      MVS 3.8j (2026-08-06, JES2 BUFSIZE 3664).  IEBDG wrote 4000-byte
     *      records to SYSOUT; TSTJESLG dumped what JES2 put on the spool:
     *
     *        blk 0  +10  00 18 0E3F | 0FA0 C1C2C3C4...
     *                    len=0  flags=SPAN|FIRST  len2=3647
     *                    total=4000, then 3647 bytes of payload
     *        blk 1  +10  00 12 0161 | C8C9D1D2...
     *                    len=0  flags=SPAN|LAST   len2=353, no prefix
     *
     *      3647 + 353 = 4000, exactly the announced total - which is what
     *      the clamp in the walk measures against, so this fixture is also
     *      the proof that the clamp cannot fire on legitimate data.
     *
     *      Note what the FIRST part does to the block: 10 + 4 + 2 + 3647 =
     *      3663 of 3664 bytes.  One byte is left, too little for another
     *      record header - the "short tail is an ordinary end" rule, on real
     *      data.
     * ---------------------------------------------------------------- */
    printf("(12) real capture: a 4000-byte record across two blocks\n");
    {
        char     *b1 = calloc(1, BIGBLK);
        char     *b2 = calloc(1, BIGBLK);
        SPLINE   *sp;
        unsigned  i;

        if (!b1 || !b2) { printf("out of memory\n"); exit(2); }
        pb_reset(&pb);
        seam_len = 0; seam_ok = -1;

        sp = (SPLINE *)&b1[10];                 /* FIRST part */
        sp->len   = 0;
        sp->flags = FLAG_SPAN | FLAG_FIRST;
        sp->len2  = 3647;
        *(unsigned short *)&sp->data[0] = 4000;
        for (i = 0; i < 3647; i++) sp->data[2+i] = (unsigned char)('A' + (i % 26));

        rc = __jesprb(b1, BIGBLK, &pb, seam_emit, NULL);

        CHECK_EQ(rc, 0, "(12) first block returns 0");
        CHECK_EQ(pb.reason, JESPRB_OK,
                 "(12) the 1-byte tail behind the part is an ordinary end");
        CHECK_EQ(pb.total, 4000, "(12) the announced total was picked up");
        CHECK_EQ(pb.linelen, 3647, "(12) 3647 bytes held over");
        CHECK_EQ(seam_len, 0, "(12) nothing emitted yet");

        sp = (SPLINE *)&b2[10];                 /* LAST part, no prefix */
        sp->len   = 0;
        sp->flags = FLAG_SPAN | FLAG_LAST;
        sp->len2  = 353;
        for (i = 0; i < 353; i++) sp->data[i] = (unsigned char)('a' + (i % 26));
        ((PRLINE *)&b2[10 + 4 + 353])->len = EOB;

        rc = __jesprb(b2, BIGBLK, &pb, seam_emit, NULL);

        CHECK_EQ(rc, 0, "(12) second block returns 0");
        CHECK_EQ(pb.reason, JESPRB_OK, "(12) reason OK");
        CHECK_EQ(seam_len, 4000, "(12) 3647 + 353 = the announced 4000");
        CHECK_EQ(seam_ok, 1, "(12) every byte in place across the seam");
        CHECK_EQ(pb.linelen, 0, "(12) the line is closed");

        free(b1);
        free(b2);
    }

    /* ------------------------------------------------------------------
     * (13) THE JOB LOG, shaped as measured on MVS 3.8j (#760): an empty record
     *      with an immediate skip to channel 1, the heading with write and
     *      space 3, then records without a byte.
     * ---------------------------------------------------------------- */
    printf("(13) machine control bytes, plain records\n");
    {
        ncc = 0;
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_ccl(blk, 10,  "",     0xA0, 0x8B);
        off = put_ccl(blk, off, "HEAD", 0xA0, 0x19);
        off = put_ccl(blk, off, "NEXT", 0x00, 0);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cc_emit, &pb);

        CHECK_EQ(rc, 0, "(13) rc 0");
        CHECK_EQ(ncc, 3, "(13) three calls, the empty record included");
        CHECK_EQ(cclen[0], 0, "(13) the eject has no text");
        CHECK_EQ(ccval[0], JESCC_HAS | JESCC_MACH | 0x8B, "(13) eject: X'8B', machine");
        CHECK_STR(cctxt[1], cclen[1], "HEAD", "(13) the text does not hold the byte");
        CHECK_EQ(ccval[1], JESCC_HAS | JESCC_MACH | 0x19, "(13) heading: X'19', machine");
        CHECK_STR(cctxt[2], cclen[2], "NEXT", "(13) record without a byte");
        CHECK_EQ(ccval[2], 0, "(13) no byte, and no stale value from the record before");
        CHECK_EQ(pb.lines, 2, "(13) lines counts those with text");
    }

    /* ------------------------------------------------------------------
     * (14) ASA, plain, as IDCAMS SYSPRINT has it: flags X'C0'.  The literals
     *      are the host's characters here and EBCDIC on the target.
     * ---------------------------------------------------------------- */
    printf("(14) ASA control bytes, plain records\n");
    {
        ncc = 0;
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_ccl(blk, 10,  "TITLE", 0xC0, '1');
        off = put_ccl(blk, off, " ",     0xC0, '0');
        off = put_ccl(blk, off, "TEXT",  0xC0, ' ');
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cc_emit, &pb);

        CHECK_EQ(ncc, 3, "(14) three lines");
        CHECK_EQ(ccval[0], JESCC_HAS | JESCC_ASA | (unsigned char)'1', "(14) '1', ASA");
        CHECK_EQ(ccval[1], JESCC_HAS | JESCC_ASA | (unsigned char)'0', "(14) '0', ASA");
        CHECK_EQ(ccval[2], JESCC_HAS | JESCC_ASA | (unsigned char)' ', "(14) blank, ASA");
        CHECK_STR(cctxt[0], cclen[0], "TITLE", "(14) the text starts behind the byte");
        CHECK_STR(cctxt[2], cclen[2], "TEXT", "(14) the next record is found");
        CHECK_EQ(rc, 0, "(14) rc 0");
    }

    /* ------------------------------------------------------------------
     * (15) a byte whose kind the record does not state.  Not seen on the
     *      spool; the caller gets the byte and can look at the RECFM.
     * ---------------------------------------------------------------- */
    printf("(15) control byte of no stated kind\n");
    {
        ncc = 0;
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = put_ccl(blk, 10, "X", FLAG_HASCC, 0x09);
        put_eob(blk, off);

        rc = __jesprb(blk, BLKSIZE, &pb, cc_emit, &pb);

        CHECK_EQ(rc, 0, "(15) rc 0");
        CHECK_EQ(ccval[0], JESCC_HAS | 0x09, "(15) the byte, with neither kind");
    }

    /* ------------------------------------------------------------------
     * (16) spanned WITHOUT a byte: every spanned record measured on MVS 3.8j.
     *      One part with X'1E', then FIRST + LAST across a block.  A plain
     *      record with a byte before them must not leak into them.
     * ---------------------------------------------------------------- */
    printf("(16) spanned, no control byte (as measured)\n");
    {
        ncc = 0;
        pb_reset(&pb);
        blk  = blk_renew(blk);
        blk2 = blk_renew(blk2);
        off = put_ccl(blk, 10, "P", 0xC0, '0');
        off = put_span(blk, off, "ONE", FLAG_FIRST | FLAG_MIDDLE | FLAG_LAST, 3, 0);
        off = put_span(blk, off, "AB",  FLAG_FIRST, 4, 0);

        rc = __jesprb(blk, BLKSIZE, &pb, cc_emit, &pb);
        CHECK_EQ(rc, 0, "(16) first block rc 0");

        off = put_span(blk2, 10, "CD", FLAG_LAST, 0, 0);
        put_eob(blk2, off);
        rc = __jesprb(blk2, BLKSIZE, &pb, cc_emit, &pb);
        CHECK_EQ(rc, 0, "(16) second block rc 0");

        CHECK_EQ(ncc, 3, "(16) three lines");
        CHECK_EQ(ccval[1], 0, "(16) one-part span: no byte");
        CHECK_STR(cctxt[1], cclen[1], "ONE", "(16) one-part span: text");
        CHECK_EQ(ccval[2], 0, "(16) two-part span: no byte");
        CHECK_STR(cctxt[2], cclen[2], "ABCD", "(16) two-part span: text");
    }

    /* ------------------------------------------------------------------
     * (17) spanned WITH a byte.  SYNTHETIC: FLAG_HASCC on a FIRST part has not
     *      been seen on the spool.  This pins the walk's contract: the byte
     *      comes from the FIRST part, is not part of the text, survives a
     *      block boundary and a plain record in between, and does not leak
     *      into the next line.
     * ---------------------------------------------------------------- */
    printf("(17) spanned, with a control byte (synthetic)\n");
    {
        unsigned o2;

        ncc = 0;
        pb_reset(&pb);
        blk  = blk_renew(blk);
        blk2 = blk_renew(blk2);
        o2  = 10;
        off = put_span(blk, o2, "AB", FLAG_FIRST, 4, '0');
        ((SPLINE *)&blk[o2])->flags |= FLAG_ASA;
        off = put_ccl(blk, off, "P", 0xA0, 0x11);

        rc = __jesprb(blk, BLKSIZE, &pb, cc_emit, &pb);
        CHECK_EQ(rc, 0, "(17) first block rc 0");

        off = put_span(blk2, 10, "CD", FLAG_LAST, 0, 0);
        off = put_span(blk2, off, "EF", FLAG_FIRST | FLAG_LAST, 2, 0);
        put_eob(blk2, off);
        rc = __jesprb(blk2, BLKSIZE, &pb, cc_emit, &pb);
        CHECK_EQ(rc, 0, "(17) second block rc 0");

        CHECK_EQ(ncc, 3, "(17) three lines");
        CHECK_EQ(ccval[0], JESCC_HAS | JESCC_MACH | 0x11, "(17) the plain record has its own");
        CHECK_STR(cctxt[1], cclen[1], "ABCD", "(17) the byte is not in the text");
        CHECK_EQ(ccval[1], JESCC_HAS | JESCC_ASA | (unsigned char)'0',
                 "(17) the FIRST part's byte reaches the LAST part");
        CHECK_EQ(ccval[2], 0, "(17) and not the line after it");
    }

    /* ------------------------------------------------------------------
     * (18) a fragment keeps its byte, and a FIRST part whose byte would lie
     *      behind the end of the block is not read (run under ASan).
     * ---------------------------------------------------------------- */
    printf("(18) spanned, cut short\n");
    {
        unsigned o2;

        ncc = 0;
        pb_reset(&pb);
        blk = blk_renew(blk);
        o2  = 10;
        off = put_span(blk, o2, "AB", FLAG_FIRST, 8, '-');
        ((SPLINE *)&blk[o2])->flags |= FLAG_ASA;
        put_trunc(blk, off, 0);
        ((SPLINE *)&blk[off])->flags = FLAG_SPAN | FLAG_MIDDLE;
        ((SPLINE *)&blk[off])->len2  = BLKSIZE;

        rc = __jesprb(blk, BLKSIZE, &pb, cc_emit, &pb);
        CHECK_EQ(rc, 0, "(18) the block is given up, not the walk");
        CHECK_EQ(pb.reason, JESPRB_TRUNC, "(18) reason TRUNC");
        CHECK_EQ(ncc, 1, "(18) the fragment goes out");
        CHECK_EQ(ccval[0], JESCC_HAS | JESCC_ASA | (unsigned char)'-', "(18) with its byte");

        ncc = 0;
        pb_reset(&pb);
        blk = blk_renew(blk);
        off = BLKSIZE - sizeof(SPLINE) - 2;     /* the total is the last thing */
        ((SPLINE *)&blk[off])->flags = FLAG_SPAN | FLAG_FIRST | FLAG_HASCC | FLAG_ASA;
        ((SPLINE *)&blk[off])->len2  = 1;
        *(unsigned short *)&blk[off + sizeof(SPLINE)] = 1;
        ((PRLINE *)&blk[10])->len    = (unsigned char)(off - 10 - sizeof(PRLINE));
        ((PRLINE *)&blk[10])->flags  = 0;

        rc = __jesprb(blk, BLKSIZE, &pb, cc_emit, &pb);
        CHECK_EQ(rc, 0, "(18) no byte behind the block: rc 0");
        CHECK_EQ(pb.reason, JESPRB_TRUNC, "(18) no byte behind the block: TRUNC");
    }

    /* ------------------------------------------------------------------
     * (19) jesccsp(): the paper movement of every class of value
     * ---------------------------------------------------------------- */
    printf("(19) jesccsp()\n");
    {
#define A_(c) (JESCC_HAS | JESCC_ASA | (unsigned char)(c))
#define M_(b) (JESCC_HAS | JESCC_MACH | (b))
        check_sp(0,            0, 1,   "(19) no byte: write, then space 1");
        check_sp(JESCC_HAS | 0x09, 0, 1, "(19) no stated kind: the same");

        check_sp(A_(' '),      1, 0,   "(19) ASA blank: space 1 before");
        check_sp(A_('0'),      2, 0,   "(19) ASA 0: space 2 before");
        check_sp(A_('-'),      3, 0,   "(19) ASA -: space 3 before");
        check_sp(A_('+'),      0, 0,   "(19) ASA +: overprint");
        check_sp(A_('1'),     -1, 0,   "(19) ASA 1: new page");
        check_sp(A_('2'),     -2, 0,   "(19) ASA 2: channel 2");
        check_sp(A_('9'),     -9, 0,   "(19) ASA 9: channel 9");
        check_sp(A_('A'),    -10, 0,   "(19) ASA A: channel 10");
        check_sp(A_('C'),    -12, 0,   "(19) ASA C: channel 12");
        check_sp(A_('X'),      1, 0,   "(19) ASA invalid: space 1 before");

        check_sp(M_(0x01),     0, 0,   "(19) X'01': write, no space");
        check_sp(M_(0x09),     0, 1,   "(19) X'09': write, space 1");
        check_sp(M_(0x11),     0, 2,   "(19) X'11': write, space 2");
        check_sp(M_(0x19),     0, 3,   "(19) X'19': write, space 3");
        check_sp(M_(0x89),     0, -1,  "(19) X'89': write, new page");
        check_sp(M_(0x91),     0, -2,  "(19) X'91': write, channel 2");
        check_sp(M_(0xE1),     0, -12, "(19) X'E1': write, channel 12");
        check_sp(M_(0x03),     0, 0,   "(19) X'03': no operation");
        check_sp(M_(0x0B),     1, 0,   "(19) X'0B': space 1 at once");
        check_sp(M_(0x1B),     3, 0,   "(19) X'1B': space 3 at once");
        check_sp(M_(0x8B),    -1, 0,   "(19) X'8B': new page at once");
        check_sp(M_(0xE3),   -12, 0,   "(19) X'E3': channel 12 at once");
        check_sp(M_(0x00),     0, 1,   "(19) machine invalid: as if no byte");
        check_sp(M_(0x21),     0, 1,   "(19) X'21' is no carriage code: as if no byte");
        check_sp(M_(0xE9),     0, 1,   "(19) X'E9' is past channel 12: as if no byte");
#undef A_
#undef M_
    }

    if (pb.prbuf) free(pb.prbuf);
    if (blk)  free(blk);
    if (blk2) free(blk2);

    return mbt_test_summary("TSTJESPRB");
}
