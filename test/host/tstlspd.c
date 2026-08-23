/*
 * tstlspd.c - libc370 #80 defect 2: __listpd() (src/clib/@@listpd.c) must bound
 * its directory walk by the bytes fread() actually delivered, not by the
 * halfword it finds inside the block.
 *
 * ISSUE #80 lists three defects in one file.  This test covers the SECOND, the
 * one that appears in no heading and is the only one of the three that reaches
 * the BOUNDED callers too - mvsMF dsapi.c:713 and ftpd ftpd#mvs.c:1191 -
 * because the filter is applied inside the loop, after the damage is done:
 *
 *     len = fread(buf, 1, sizeof(buf), fp);   // buf is 256 bytes
 *     if (len <= 0) goto quit;
 *
 *     len = *(unsigned short *)buf;           // length from INSIDE the block
 *     for(pos = 2; pos < len; pos += size) {
 *         if (memcmp(&buf[pos], "\xFF..." , 8)==0) goto quit;
 *         size  = 12;
 *         size += ((buf[pos+11] & 0x1F) * 2);
 *         ...
 *         memcpy(pdslist, &buf[pos], size);
 *
 * Three reads are unbounded by that condition, and clamping `len` to the read
 * is NOT on its own enough for any of them:
 *
 *   - the 8-byte end-of-directory memcmp    needs pos + 8  <= len
 *   - buf[pos+11], the user data length     needs pos + 12 <= len
 *   - the memcpy, up to 12 + 2*31 = 74      needs pos + size <= len
 *
 * A full 256-byte block is the ordinary case that trips it.  Entries are at
 * least 12 bytes, so 21 of them fill bytes 2..253 and leave two bytes over:
 * pos = 254 still satisfies `pos < 256`, and the first thing the body does is
 * read buf[254..261] - six bytes past the end of a 256-byte STACK array, in a
 * function that then builds a member record out of whatever was there,
 * calloc()s it, and adds it to the array the caller believes.
 *
 * The fix keeps the fixed-part bound in the loop condition, where it covers the
 * sentinel and the length byte together, and tests the variable part once the
 * entry size is known:
 *
 *     nread = fread(buf, 1, sizeof(buf), fp);
 *     if (nread < 2) goto quit;
 *     len = *(unsigned short *)buf;
 *     if (len > nread) len = nread;
 *     for(pos = 2; pos + 12 <= len; pos += size) {
 *         ...
 *         if (pos + size > len) break;
 *
 * ====================================================================
 * WHAT THIS PINS - AND WHAT IT DOES NOT
 * --------------------------------------------------------------------
 * - Cases (1) and (7) are the regression half: a well-formed block must still
 *   yield every entry, with the names intact, and the filter must still select.
 *   `pos + 12 <= len` is not a stricter walk on a well-formed block - the last
 *   entry ends exactly at len, so the loop leaves at pos == len either way.
 *
 * - Case (2) is the RED one.  The block is well-formed and full; nothing about
 *   it is damaged or hostile.  That is the point - it is what an ordinary full
 *   directory block looks like, and the pre-fix walk reads past the buffer on
 *   it.
 *
 * - Case (3) separates the two bounds.  The block header claims 256 while
 *   fread() delivered 60, which is the short-read half of the defect.  Post-fix
 *   the walk stops at 60; pre-fix it walks to 256, first over 196 bytes of
 *   stack the read never wrote, then past the buffer.
 *
 * - Case (5) is the only one whose pre-fix behaviour is NOT a buffer overrun:
 *   a final entry whose user data runs past `len` is copied out of bytes that
 *   are inside buf[] but outside the block, so it is a FABRICATED member rather
 *   than a crash.  It is the quietest form of the defect, and it needs a count
 *   assertion rather than ASAN to catch.
 *
 * - NOT covered, deliberately: defect 1 (unbounded allocation - needs a `max`
 *   parameter, i.e. a signature change and a relink round) and defect 3 (a
 *   calloc() failure returns a silently truncated list - the convention has to
 *   be settled together with #61).  Case (5) stops the walk and keeps what the
 *   block already yielded; SIGNALLING that shortfall to the caller is defect 3
 *   and is not attempted here.  #80 stays open for both.
 *
 * - The block length is stored in HOST order by fx_setlen(), because
 *   __listpd() reads it with a native `unsigned short` load.  On the target
 *   both sides are big-endian and agree; on a little-endian host both sides
 *   are little-endian and agree.  What this test does NOT cover, therefore, is
 *   the header decode - only the walk that length drives.  Same standing as
 *   hdr_to_host_order() in test/host/tstrldwk.c.
 *
 * - Host encoding, not the target's.  Member names here are ASCII and padded
 *   with the host blank; on MVS they are EBCDIC and padded with x'40'.  Nothing
 *   under test depends on which: the walk is byte arithmetic, and the x'FF'
 *   sentinel is the same byte value in either codepage.  Not a codepage test.
 *
 * - arrayadd() is SHIMMED rather than linked, unlike tstjestx.c.  ARRAY_SIZE
 *   is `sizeof(ARRAY) / sizeof(void *)` (clibary.h:18), which is exactly 3 on
 *   the 4-byte-pointer target and truncates to 1 on a 64-bit host - so the real
 *   @@aradd.c under-provides its slots by 4 bytes per generation and corrupts
 *   itself the moment an array grows past ARRAY_DEFAULT (20).  Case (2) needs
 *   21 entries.  That is a host artefact and NOT a target defect: on MVS
 *   3 * 4 == sizeof(ARRAY) exactly - measured, not assumed: cc370 -S over a
 *   file holding sizeof(ARRAY), ARRAY_SIZE and sizeof(void *) emits
 *   DC F'12', DC F'3' and DC F'4', so the struct carries no padding and the
 *   division is exact on the target.  It is also why tstjestx.c can link the
 *   real array code and this cannot - its arrays never reach 20 elements.
 *   The array layer is not what is under test here; the walk is.
 *
 * ====================================================================
 *
 * BUILD / RUN (host, from test/host):
 *
 *   -D'__asm__(...)='  erases the file-scope S/370 statement in @@listpd.c and
 *                      the inline MVCL in clibstr.h's static memset().
 *                      Variadic because extended asm carries commas.
 *   -D__volatile__=    that memset() is written "__asm__ __volatile__(", and
 *                      the token stops the erase above from matching at all.
 *                      NOTE this leaves memset() a no-op stub, so this file
 *                      zeroes with explicit loops.
 *   -D__32BIT__        is what libc370's stddef.h/stdlib.h key size_t off.
 *   ASAN               is load-bearing, not decoration: buf[] is a 256-byte
 *                      STACK array, so a walk that reads one entry too far is
 *                      a stack-buffer-overflow rather than a silent success.
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tstlspd.c "$R/src/clib/@@patmat.c"
 *     ./t                                             # 15/15, rc 0
 *
 * RED, against the pre-fix source.  Same fixtures, same shims, the real
 * pre-fix file - the driver only swaps which @@listpd.c it includes:
 *
 *     git show <pre-fix-rev>:src/clib/@@listpd.c > /tmp/old.c
 *
 * Case (1) passes, then case (2) aborts.  The ASAN report is the only output
 * from there on: the abort does not flush stdout, so the PASS lines are lost
 * with it.
 *
 *     ==ERROR: AddressSanitizer: stack-buffer-overflow
 *     READ of size 8 at 0x00016fd5dec0 thread T0
 *         #0 memcmp
 *         #1 __listpd
 *         #2 case_full_block
 *
 *     Address 0x00016fd5dec0 is located in stack of thread T0 at offset 320
 *     in frame #0 __listpd
 *       This frame has 3 object(s):
 *         [32, 40)   'array'
 *         [64, 320)  'buf' <== Memory access at offset 320 overflows
 *         [384, 396) 'member'
 *
 * "READ of size 8 ... at offset 320" where buf is [64, 320) is the whole defect
 * in one line: the read begins exactly AT the end of the 256-byte array.  That
 * is the sentinel memcmp at pos = 254 reading buf[254..261].  On MVS the six
 * bytes past the end are not a redzone - they are whatever follows buf[] in
 * __listpd()'s own frame, member[] and the saved registers among them - and the
 * entry built out of them is handed to the caller as a member of the data set.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/clib/@@listpd.c"

/* ---- shims -------------------------------------------------------------
 * Defined AFTER the translation unit so each one matches the prototype the
 * library's own headers declare, rather than a second opinion about it.
 */

/* The array layer.  See the header note: the real @@aradd.c is not host-safe
** past 20 elements, and the array is not what is under test.  This keeps the
** items exactly as __listpd() calloc'd them, so ASAN still sees every record
** at its true allocated size. */
#define FX_MAX 512
static void     *fx_slots[FX_MAX];
static unsigned  fx_count;

int arrayadd(void *varray, void *vitem)
{
    void ***carray = varray;

    if (fx_count >= FX_MAX) return -1;
    fx_slots[fx_count++] = vitem;
    *carray = (void **)fx_slots;    /* non-NULL: __listpd() returns it */
    return 0;
}

static void fx_free_items(void)
{
    unsigned n;
    for (n = 0; n < fx_count; n++) free(fx_slots[n]);
    fx_count = 0;
}

/* The FILE layer.  feof() is a macro over ->flags (clibio.h:138), so the shim
** FILE has to be a real struct _file and the flag has to be set for real. */
static unsigned char fx_block[256];  /* what the "data set" holds        */
static size_t        fx_nread;       /* what fread() reports having read */
static int           fx_served;      /* one block per case               */
static FILE          fx_file;

FILE *fopen(const char *filename, const char *mode)
{
    (void)filename; (void)mode;
    fx_file.flags = 0;
    fx_served     = 0;
    return &fx_file;
}

int fclose(FILE *stream) { (void)stream; return 0; }

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t want = size * nmemb;
    size_t copy = fx_nread;
    size_t i;

    if (fx_served) {
        stream->flags |= _FILE_FLAG_EOF;
        return 0;
    }
    fx_served = 1;

    /* Copy only what the case says was read.  The rest of the caller's buffer
    ** is left exactly as the real fread() would leave it - untouched - which is
    ** what case (3) needs in order to be honest. */
    if (copy > want) copy = want;
    for (i = 0; i < copy; i++) ((unsigned char *)ptr)[i] = fx_block[i];

    stream->flags |= _FILE_FLAG_EOF;
    return fx_nread;
}

/* ---- fixture builders -------------------------------------------------- */

static void fx_clear(void)
{
    size_t i;
    for (i = 0; i < sizeof(fx_block); i++) fx_block[i] = 0;
    fx_nread = 0;
    fx_count = 0;
}

/* Host order on purpose - __listpd() reads this with a native load. */
static void fx_setlen(unsigned len)
{
    unsigned short hw = (unsigned short)len;
    memcpy(fx_block, &hw, sizeof(hw));
}

/* One directory entry: 8 byte name, 3 byte TTR, 1 byte indicator whose low
** five bits are the user data length in HALFWORDS.  Returns its size. */
static unsigned fx_entry(unsigned pos, const char *name, unsigned udata_hw)
{
    unsigned i;
    unsigned size = 12 + udata_hw * 2;

    for (i = 0; i < 8; i++)            fx_block[pos + i] = ' ';
    for (i = 0; i < 8 && name[i]; i++) fx_block[pos + i] = (unsigned char)name[i];
    fx_block[pos +  8] = 0x00;              /* TTR */
    fx_block[pos +  9] = 0x01;
    fx_block[pos + 10] = 0x02;
    fx_block[pos + 11] = (unsigned char)(udata_hw & 0x1F);
    for (i = 12; i < size; i++) fx_block[pos + i] = 0xEE;   /* user data */
    return size;
}

static void fx_sentinel(unsigned pos)
{
    unsigned i;
    for (i = 0; i < 8; i++) fx_block[pos + i] = 0xFF;
}

/* member name of the n-th entry walked (0-based), blank-trimmed */
static void fx_name(unsigned n, char *out)
{
    unsigned char *rec = (unsigned char *)fx_slots[n];
    int i;

    for (i = 0; i < 8; i++) out[i] = (char)rec[i];
    out[8] = 0;
    for (i = 7; i >= 0 && out[i] == ' '; i--) out[i] = 0;
}

/* ---- harness ----------------------------------------------------------- */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define CHECK_EQ(got, want, msg)                                           \
    do {                                                                   \
        long g_ = (long)(got), w_ = (long)(want);                          \
        mbt_run++;                                                         \
        if (g_ == w_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }     \
        else { mbt_failed++;                                               \
               printf("  FAIL: %s (got %ld, want %ld)\n", (msg), g_, w_); }\
    } while (0)

#define CHECK_STR(got, want, msg)                                          \
    do {                                                                   \
        mbt_run++;                                                         \
        if (strcmp((const char *)(got), (want)) == 0) {                    \
            mbt_passed++; printf("  PASS: %s\n", (msg)); }                 \
        else { mbt_failed++;                                               \
               printf("  FAIL: %s (got \"%s\", want \"%s\")\n",            \
                      (msg), (const char *)(got), (want)); }               \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed ? 1 : 0;
}

/* ---- cases ------------------------------------------------------------- */

static const char *ALPHABETA[8] = { "ALPHA1", "ALPHA2", "ALPHA3", "ALPHA4",
                                    "BETA1",  "BETA2",  "BETA3",  "BETA4" };

/* (1) A well-formed block yields every entry, with the names intact. */
static void case_wellformed(void)
{
    PDSLIST **array;
    char      nm[9];
    unsigned  pos = 2;
    int       i;

    printf("\n(1) well-formed 98-byte block, eight 12-byte entries\n");

    fx_clear();
    for (i = 0; i < 8; i++) pos += fx_entry(pos, ALPHABETA[i], 0);
    fx_setlen(pos);                 /* 2 + 8*12 = 98 */
    fx_nread = sizeof(fx_block);    /* the read delivered a full block */

    array = __listpd("IGNORED.DSN", NULL);
    CHECK(array != NULL, "an array came back");
    CHECK_EQ(fx_count, 8, "all eight entries walked");

    fx_name(0, nm);
    CHECK_STR(nm, "ALPHA1", "first member name intact");
    fx_name(7, nm);
    CHECK_STR(nm, "BETA4", "last member name intact");

    fx_free_items();
}

/* (2) THE RED CASE.  A full 256-byte block: 21 entries end at 254 and leave
**     two bytes over.  Pre-fix, pos = 254 passes `pos < 256` and the sentinel
**     memcmp reads buf[254..261] - past the 256-byte stack array. */
static void case_full_block(void)
{
    PDSLIST **array;
    unsigned  pos = 2;
    int       i;

    printf("\n(2) full 256-byte block, 21 entries + 2 bytes over\n");

    fx_clear();
    for (i = 0; i < 21; i++) pos += fx_entry(pos, "MEM", 0);
    CHECK_EQ(pos, 254, "fixture: 21 entries end at 254");
    fx_setlen(sizeof(fx_block));    /* the block claims all 256 bytes */
    fx_nread = sizeof(fx_block);

    array = __listpd("IGNORED.DSN", NULL);
    CHECK(array != NULL, "an array came back");
    CHECK_EQ(fx_count, 21, "21 entries, and the two spare bytes are not one");

    fx_free_items();
}

/* (3) The block header claims more than fread() delivered. */
static void case_short_read(void)
{
    PDSLIST **array;
    unsigned  pos = 2;
    int       i;

    printf("\n(3) header claims 256, fread() delivered 60\n");

    fx_clear();
    for (i = 0; i < 21; i++) pos += fx_entry(pos, "MEM", 0);
    fx_setlen(sizeof(fx_block));    /* the block claims 256 ... */
    fx_nread = 60;                  /* ... the read produced 60 */

    array = __listpd("IGNORED.DSN", NULL);
    CHECK_EQ(fx_count, 4, "walk stops at the bytes actually read");
    (void)array;

    fx_free_items();
}

/* (4) A read too short to hold a block length at all. */
static void case_no_length(void)
{
    PDSLIST **array;

    printf("\n(4) fread() delivered one byte\n");

    fx_clear();
    fx_block[0] = 0x01;
    fx_nread    = 1;

    array = __listpd("IGNORED.DSN", NULL);
    CHECK(array == NULL, "no array built from a block with no length");
    CHECK_EQ(fx_count, 0, "and nothing was allocated");
}

/* (5) A final entry whose user data runs past the block.  Pre-fix this is not
**     an overrun - it is a member fabricated out of bytes past `len`. */
static void case_entry_past_end(void)
{
    PDSLIST **array;
    unsigned  pos = 2;

    printf("\n(5) last entry's user data runs past the block length\n");

    fx_clear();
    pos += fx_entry(pos, "KEEP1", 0);
    pos += fx_entry(pos, "KEEP2", 0);
    CHECK_EQ(pos, 26, "fixture: third entry starts at 26");
    fx_entry(pos, "TOOLONG", 5);    /* 12 + 10 = 22 bytes, from 26 to 48 */
    fx_setlen(40);                  /* ... but the block ends at 40 */
    fx_nread = sizeof(fx_block);

    array = __listpd("IGNORED.DSN", NULL);
    CHECK_EQ(fx_count, 2, "the over-long entry is not reported");
    (void)array;

    fx_free_items();
}

/* (6) The x'FF' end-of-directory sentinel still stops the walk. */
static void case_sentinel(void)
{
    PDSLIST **array;
    unsigned  pos = 2;

    printf("\n(6) end-of-directory sentinel\n");

    fx_clear();
    pos += fx_entry(pos, "ONE", 0);
    pos += fx_entry(pos, "TWO", 0);
    fx_sentinel(pos);
    fx_setlen(50);                  /* room for the sentinel to be reached */
    fx_nread = sizeof(fx_block);

    array = __listpd("IGNORED.DSN", NULL);
    CHECK_EQ(fx_count, 2, "walk stops at the sentinel");
    (void)array;

    fx_free_items();
}

/* (7) The filter still selects, and still selects from a bounded walk. */
static void case_filter(void)
{
    PDSLIST **array;
    char      nm[9];
    unsigned  pos = 2;
    int       i;

    printf("\n(7) filter \"ALPHA*\" over the same block\n");

    fx_clear();
    for (i = 0; i < 8; i++) pos += fx_entry(pos, ALPHABETA[i], 0);
    fx_setlen(pos);
    fx_nread = sizeof(fx_block);

    array = __listpd("IGNORED.DSN", "ALPHA*");
    CHECK_EQ(fx_count, 4, "four ALPHA members selected");
    fx_name(3, nm);
    CHECK_STR(nm, "ALPHA4", "and the last of them is ALPHA4");
    (void)array;

    fx_free_items();
}

int main(void)
{
    printf("=== tstlspd: __listpd() block bounds (#80 defect 2) ===\n");

    case_wellformed();
    case_full_block();
    case_short_read();
    case_no_length();
    case_entry_past_end();
    case_sentinel();
    case_filter();

    return mbt_test_summary("tstlspd");
}
