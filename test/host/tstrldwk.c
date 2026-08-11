/*
 * tstrldwk.c - libc370 #100: process_rldr() (src/clib/@@loadhi.c) must bound
 * its RLD item walk by the record's own byte count, and must refuse an adcon
 * offset that is not inside the module.
 *
 * ISSUE #100: __loadhi() copies a module into CSA and then re-reads the member
 * from STEPLIB to redo its relocations.  The RLD data in each record is a
 * stream of items: a full item is 8 bytes (relocation ESD id, position ESD id,
 * flag, 3-byte offset), and when an item's T bit (RLD_FLAG_SAME, x'01') is set
 * the NEXT item shares those two ids and is only 4 bytes.
 *
 * The pre-fix walk let that T bit decide when to stop:
 *
 *     while (count < rldr->rldcnt) {
 *         ...
 *     next:
 *         buf = (unsigned char*)rldf;
 *         if (rldf->flag & RLD_FLAG_SAME) { count += 4; rldf = buf + 4; }
 *         else                            { count += 8; rldf = buf + 8; }
 *     }
 *
 * That accounting adds the size of the NEXT item, not the current one.  It
 * comes out exact only because the first item always costs 8 and the last item
 * was assumed to have the T bit clear - the two cancel.  When the last item in
 * a record has the T bit SET the sum lands 4 bytes short of rldcnt, the loop
 * runs once more, and the walk reads an item that is not in the record.
 *
 * An ld370 before mvslovers/cc370#42 emitted exactly that: it took the flag
 * byte verbatim from the input object, whose own continuation bit has a
 * different scope, so a record that filled to RLDMAX (236) exactly kept a
 * stale T bit on the item that ended up last.  It is not rare - UFSDSSIR, a
 * 29315-byte module, has two such records, and IRXJCL has 65.
 *
 * What the phantom item reads is the point.  __aread() on a RECFM=U member
 * does no deblocking (asm/@@aread.asm: "TM ZRECFM,DCBRECU  Also exit for U"):
 * it issues a READ for BLKSIZE bytes and hands back the buffer.  Behind a
 * short record sits the tail of the previous, longer one - residue that reads
 * as a perfectly plausible RLD item whose 3-byte offset can be anything.  The
 * pre-fix store() then wrote 2-4 bytes at highlp + that offset with no bounds
 * check at all: S0C4, deterministic for a given build, with nothing in the
 * output to point at the cause.  MVS program fetch is immune because it bounds
 * its own walk by the record byte count; that is what this fix adopts.
 *
 * THIS TEST LINKS AND EXECUTES THE REAL @@loadhi.c - no mirror of the walk to
 * keep in sync.  The record it walks is not synthetic either: rec_stale[] is
 * the 252 bytes at offset 27383 of a real ld370-linked UFSDSSIR, one of the
 * two records in that module whose last item carries the stale T bit.
 *
 * ====================================================================
 * WHAT THIS PINS - AND WHAT IT DOES NOT
 * --------------------------------------------------------------------
 * - Every case passes size = 0, so every item is refused by the new offset
 *   check BEFORE fetch()/store() run.  Nothing is dereferenced.  That is not
 *   a convenience: @@loadhi.c holds addresses in `unsigned`, which is correct
 *   on the 24-bit target and truncates on a 64-bit host, and macOS arm64
 *   refuses a mapping below 4 GB, so there is no arrangement in which the
 *   host can execute the relocation itself.  What is pinned here is the WALK -
 *   which items it visits and where it stops.  The arithmetic inside
 *   fetch()/store() is unchanged by this fix and is not covered.
 *
 * - The return value doubles as the walk's item count precisely because
 *   size = 0 rejects everything.  With a real size it is what it says: the
 *   number of items refused.
 *
 * - The two header counts are S/370 halfwords on disk, which is exactly what
 *   MMRLDR's `unsigned short` fields read natively on the target and NOT what
 *   a little-endian host reads.  hdr_to_host_order() below swaps those four
 *   bytes so the walk sees the counts the target sees.  Nothing else is
 *   touched: the RLD items are byte streams (a flag byte and a GET3 address),
 *   which is why they need no such treatment and why the walk itself is
 *   endian-independent.  A consequence worth stating plainly: this test does
 *   NOT cover the header decode, only the walk driven by it.
 *
 * - Case 3 pins the bound against the record LENGTH as well as rldcnt.  A
 *   record whose header claims more RLD data than __aread() delivered must not
 *   be believed - the same residue is on the other side of that boundary too.
 *
 * - Not covered: relocate_load()'s record loop, and the control/text
 *   interleaving it tracks.  The #100 report suspected a phase slip there;
 *   it is not what fails.  ld370 emits control records and RLD records
 *   separately (never a combined x'03'), so on every module this toolchain
 *   produces the RLD records arrive after all the text, with nothing to slip.
 *
 * --------------------------------------------------------------------
 * BUILD AND RUN
 *
 *   -D'__asm__(...)=' erases the S/370 assembler the host assembler rejects:
 *                     the file-scope statements in @@loadhi.c
 *                     (__asm__("\n&FUNC SETC 'process_rldr'")) and the inline
 *                     MVCL in clibstr.h's static memset().  It is variadic
 *                     because that second one is an extended asm with operand
 *                     lists, so a one-parameter macro does not match it.
 *   -D__volatile__=   for the same reason - the token sits between __asm__ and
 *                     the '(', which stops the macro matching at all.
 *   NOTE that this leaves libc370's memset() a no-op stub, so this file uses
 *                     plain loops rather than memset().  memcpy() is only a
 *                     declaration in clibstr.h and resolves to the host's.
 *   -D__32BIT__       is what libc370's own stddef.h keys size_t off.
 *   ASAN              is load-bearing here, not decoration: case 1 puts the
 *                     record in an allocation of exactly its own length, so a
 *                     walk that reads one item too far is a heap-buffer
 *                     overflow rather than a silent success.
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tstrldwk.c
 *     ./t                                             # 8/8, rc 0
 *
 * RED, against the pre-fix source.  Same fixture, same allocation, the real
 * pre-fix file - only the call adapts to its narrower signature.  Every item
 * is first marked unresolved (flag |= x'80') so the pre-fix walk performs no
 * fetch()/store() at all: what ASAN then reports is the walk reading past the
 * record, and nothing else.
 *
 *     git show <pre-fix-rev>:src/clib/@@loadhi.c > /tmp/old.c
 *     ... driver: same shims, #include /tmp/old.c, and
 *         process_rldr(exact, 0, 0)  on the same 252-byte allocation ...
 *
 *     RED: pre-fix process_rldr() over the same 41-item record,
 *          in a heap allocation of exactly its 252 bytes
 *
 *     ==ERROR: AddressSanitizer: heap-buffer-overflow
 *     READ of size 1 at 0x61100000013c thread T0
 *         #0 process_rldr+0x160
 *     0x61100000013c is located 0 bytes after 252-byte region
 *
 * One byte, immediately after the record: the phantom item's flag byte.  On
 * MVS that byte is not unmapped, it is the previous record's tail, and the
 * three bytes behind it become an offset that store() writes through.
 *
 * ====================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/clib/@@loadhi.c"

/* ---- shims -----------------------------------------------------------
 * Defined AFTER the translation unit so each one matches the prototype the
 * library's own headers declare, rather than a second opinion about it.
 * __loadhi() is not exercised; these only have to resolve.
 */
static int wtof_calls = 0;
void  wtof(const char *text, ...) { (void)text; wtof_calls++; }
void *__steplb(void) { return NULL; }
void *__load(void *dcb, const char *m, unsigned *sz, char *ac)
{ (void)dcb; (void)m; (void)sz; (void)ac; return NULL; }
int   __delete(const char *m) { (void)m; return 0; }
void *getmain(unsigned size, unsigned sp) { (void)size; (void)sp; return NULL; }
int   freemain(void *p) { (void)p; return 0; }
CDE  *clib_find_cde(const char *name) { (void)name; return NULL; }
int   __aread(void *handle, void *buf, size_t *len)
{ (void)handle; (void)buf; (void)len; return 8; }
/* libc370's toupper(c) is a macro indexing __toup[], reached from __loadhi().
** Never used from here; filled in at startup so it is not a trap either. */
static short toup_tbl[256];
short *__toup = toup_tbl;

/* ---- the fixture ------------------------------------------------------ */

/*
 * Bytes 27383..27634 of an ld370-linked UFSDSSIR (29315 bytes, the size the
 * #100 report calls good).  id x'02', ctlcnt 0, rldcnt 236 -> the record is
 * 16 + 236 = 252 bytes.  It holds 41 items, and the LAST one's flag is x'0D':
 * LL4 (x'0C') plus the stale T bit (x'01') that claims a 42nd item the record
 * does not contain.  Highest offset named is x'000C18', so with the module's
 * real size (24816) every one of the 41 is in range.
 */
static const unsigned char rec_stale[] = {
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEC, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x0D, 0x00, 0x00, 0x60,
    0x0D, 0x00, 0x00, 0x64, 0x0D, 0x00, 0x00, 0xB8, 0x0D, 0x00, 0x00, 0xC0,
    0x0D, 0x00, 0x01, 0x50, 0x0D, 0x00, 0x01, 0x60, 0x0D, 0x00, 0x02, 0x08,
    0x0D, 0x00, 0x02, 0x10, 0x0D, 0x00, 0x02, 0x14, 0x0D, 0x00, 0x02, 0x18,
    0x0D, 0x00, 0x07, 0xD8, 0x0D, 0x00, 0x07, 0xDC, 0x0D, 0x00, 0x07, 0xE4,
    0x0D, 0x00, 0x07, 0xE8, 0x0D, 0x00, 0x07, 0xF4, 0x0D, 0x00, 0x08, 0x00,
    0x0D, 0x00, 0x08, 0x2C, 0x0C, 0x00, 0x08, 0x5C, 0x00, 0x03, 0x00, 0x01,
    0x1D, 0x00, 0x01, 0x5C, 0x1C, 0x00, 0x08, 0x4C, 0x00, 0x04, 0x00, 0x01,
    0x1C, 0x00, 0x07, 0xEC, 0x00, 0x05, 0x00, 0x01, 0x1C, 0x00, 0x07, 0xFC,
    0x00, 0x06, 0x00, 0x01, 0x1C, 0x00, 0x08, 0x08, 0x00, 0x07, 0x00, 0x01,
    0x1C, 0x00, 0x08, 0x10, 0x00, 0x08, 0x00, 0x01, 0x1C, 0x00, 0x08, 0x14,
    0x00, 0x02, 0x00, 0x01, 0x1C, 0x00, 0x08, 0x24, 0x00, 0x09, 0x00, 0x01,
    0x1C, 0x00, 0x08, 0x34, 0x00, 0x0A, 0x00, 0x01, 0x1C, 0x00, 0x08, 0x44,
    0x00, 0x02, 0x00, 0x01, 0x1C, 0x00, 0x08, 0x50, 0x00, 0x02, 0x00, 0x02,
    0x0D, 0x00, 0x08, 0xF0, 0x0D, 0x00, 0x09, 0x00, 0x0D, 0x00, 0x09, 0x90,
    0x0C, 0x00, 0x09, 0x9C, 0x00, 0x03, 0x00, 0x02, 0x1C, 0x00, 0x08, 0xFC,
    0x00, 0x07, 0x00, 0x02, 0x1C, 0x00, 0x09, 0x98, 0x00, 0x03, 0x00, 0x03,
    0x0D, 0x00, 0x0A, 0x28, 0x0C, 0x00, 0x0A, 0x38, 0x00, 0x04, 0x00, 0x04,
    0x0D, 0x00, 0x0B, 0x10, 0x0C, 0x00, 0x0B, 0x24, 0x00, 0x0B, 0x00, 0x04,
    0x1C, 0x00, 0x0B, 0x1C, 0x00, 0x05, 0x00, 0x05, 0x0D, 0x00, 0x0C, 0x18,
};

/* See the note above: put the two S/370 header halfwords into host order so
** MMRLDR's native reads yield the values the target reads. */
static void hdr_to_host_order(unsigned char *rec)
{
    unsigned short one = 1;
    if (*(unsigned char *)&one) {           /* little-endian host */
        unsigned char t;
        t = rec[4]; rec[4] = rec[5]; rec[5] = t;    /* ctlcnt */
        t = rec[6]; rec[6] = rec[7]; rec[7] = t;    /* rldcnt */
    }
}

#define REC_LEN     ((size_t)sizeof rec_stale)      /* 252              */
#define REC_ITEMS   41                              /* real items in it */
#define UFSDSSIR_SZ 24816u                          /* its loaded text  */

/* ---- harness ---------------------------------------------------------- */

static int checks = 0, fails = 0;

#define CHECK_EQ(got, want, what)                                       \
    do {                                                                \
        long g_ = (long)(got), w_ = (long)(want);                       \
        checks++;                                                       \
        if (g_ != w_) {                                                 \
            fails++;                                                    \
            printf("FAIL %-52s got %ld, want %ld\n", (what), g_, w_);   \
        } else {                                                        \
            printf("ok   %-52s %ld\n", (what), g_);                     \
        }                                                               \
    } while (0)

int main(void)
{
    unsigned char *exact, *padded;
    int n, i;

    for (i = 0; i < 256; i++)
        toup_tbl[i] = (short)((i >= 'a' && i <= 'z') ? i - 'a' + 'A' : i);

    printf("tstrldwk - libc370 #100 RLD walk bounds\n\n");

    /* ---------------------------------------------------------------
     * 1. The real record, in an allocation of exactly its own length.
     *    With size = 0 every item is refused, so the return value is the
     *    number of items the walk visited: 41, not 42.  Under ASAN this
     *    case is also the over-read detector - there is nothing mapped
     *    after the record for a 42nd item to be read from.
     * --------------------------------------------------------------- */
    exact = malloc(REC_LEN);
    memcpy(exact, rec_stale, REC_LEN);
    hdr_to_host_order(exact);
    n = process_rldr(exact, REC_LEN, 0, 0, 0);
    CHECK_EQ(n, REC_ITEMS, "exact-size record: items walked");
    free(exact);

    /* ---------------------------------------------------------------
     * 2. The same record with a plausible RLD item planted right after
     *    it - flag x'0C' (LL4, resolved, T clear), offset x'FFFFFF'.
     *    This is what the I/O buffer residue looks like on MVS.  The
     *    walk must stop at rldcnt and never see it.
     * --------------------------------------------------------------- */
    padded = malloc(REC_LEN + 8);
    memcpy(padded, rec_stale, REC_LEN);
    hdr_to_host_order(padded);
    padded[REC_LEN + 0] = 0x0C;                     /* flag: LL4         */
    padded[REC_LEN + 1] = 0xFF;                     /* offset x'FFFFFF'  */
    padded[REC_LEN + 2] = 0xFF;
    padded[REC_LEN + 3] = 0xFF;
    padded[REC_LEN + 4] = 0x0C;                     /* and another       */
    padded[REC_LEN + 5] = 0xFF;
    padded[REC_LEN + 6] = 0xFF;
    padded[REC_LEN + 7] = 0xFE;
    n = process_rldr(padded, REC_LEN + 8, 0, 0, 0);
    CHECK_EQ(n, REC_ITEMS, "residue after the record is not walked");

    free(padded);

    /* ---------------------------------------------------------------
     * 3. The record length wins over a header that claims more.  Both
     *    directions: a short read must truncate the walk, and a longer
     *    buffer must not extend it past rldcnt.
     * --------------------------------------------------------------- */
    padded = malloc(REC_LEN + 8);
    memcpy(padded, rec_stale, REC_LEN);
    hdr_to_host_order(padded);
    for (i = 0; i < 8; i++) padded[REC_LEN + i] = 0x0C;
    n = process_rldr(padded, 16 + 8, 0, 0, 0);
    CHECK_EQ(n, 1, "reclen shorter than rldcnt truncates the walk");

    n = process_rldr(padded, 0, 0, 0, 0);
    CHECK_EQ(n, REC_ITEMS, "reclen 0 falls back to rldcnt alone");
    free(padded);

    /* ---------------------------------------------------------------
     * 4. The offset check must not wrap at the top of the 24-bit space.
     *    One synthetic item at the highest offset a 3-byte field can hold,
     *    LL4, against a module that is the whole address space: 0xFFFFFF
     *    plus 4 still does not fit, and computing that must not overflow
     *    into a value that looks like it does.
     * --------------------------------------------------------------- */
    padded = calloc(1, 32);
    padded[0] = 0x02;                               /* id: RLD           */
    padded[6] = 0x00; padded[7] = 0x08;             /* rldcnt = 8        */
    hdr_to_host_order(padded);
    padded[16] = 0x00; padded[17] = 0x01;           /* relid             */
    padded[18] = 0x00; padded[19] = 0x01;           /* posid             */
    padded[20] = 0x0C;                              /* flag: LL4, T off  */
    padded[21] = 0xFF; padded[22] = 0xFF; padded[23] = 0xFF;
    n = process_rldr(padded, 24, 0, 0, 0x00FFFFFFu);
    CHECK_EQ(n, 1, "offset FFFFFF + 4 refused against a 16 MB module");
    free(padded);

    /* ---------------------------------------------------------------
     * 5. A record with no RLD data at all must walk nothing rather than
     *    read a first item that is not there.
     * --------------------------------------------------------------- */
    padded = calloc(1, 32);
    padded[0] = 0x02;                               /* id: RLD, rldcnt 0 */
    n = process_rldr(padded, 32, 0, 0, 0);
    CHECK_EQ(n, 0, "rldcnt 0: no item is read");
    free(padded);

    /* 5b. and a record claiming 4 bytes of RLD data - too few for even the
     *     shortest full item - must not read a partial one. */
    padded = calloc(1, 32);
    padded[0] = 0x02;
    padded[6] = 0x00; padded[7] = 0x04;
    hdr_to_host_order(padded);
    n = process_rldr(padded, 20, 0, 0, 0);
    CHECK_EQ(n, 0, "rldcnt 4: too short for a full item, none read");
    free(padded);

    CHECK_EQ(wtof_calls, 0, "process_rldr issues no WTO of its own");

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
