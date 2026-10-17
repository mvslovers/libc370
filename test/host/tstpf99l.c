/*
 * tstpf99l.c - the libc370 half of tstpf99 (#578): libc370's real
 * formatter - vsnprint.c, snprintf.c, @@examin.c, @@dblcvt.c - compiled
 * with libc370's headers and renamed l370_snprintf()/l370_vsnprintf(), so
 * that tstpf99.c can compare it with the host's snprintf() in the same
 * program.  See tstpf99.c for the build.
 */
#define vsnprintf   l370_vsnprintf
#define snprintf    l370_snprintf
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/stdio/vsnprint.c"
#include "../../src/stdio/snprintf.c"
#include "../../src/stdio/@@examin.c"
#include "../../src/stdio/@@dblcvt.c"

/* %lld and %llx go through the __64 helpers, which are big-endian only
   (they fail silently on a little-endian host); tstpf99 has no such case,
   and these stubs make sure none slips in unnoticed. */
static void no64(void) { abort(); }
void __64_from_i32(__64 *n, int32_t i) { (void)n; (void)i; no64(); }
int __64_is_zero(__64 *n) { (void)n; no64(); return 1; }
void __64_divmod(__64 *a, __64 *b, __64 *c, __64 *d)
{ (void)a; (void)b; (void)c; (void)d; no64(); }
int32_t __64_to_i32(__64 *n) { (void)n; no64(); return 0; }
void __64_copy(__64 *src, __64 *dst) { (void)src; (void)dst; no64(); }

/* libc370's ctype is table-driven and __examin() uses it live (isdigit()
   for the width, toupper() for the specifier): the host's ASCII values,
   as tstpfflt.c sets them.  tstpf99.c calls l370_init() first. */
static unsigned short isbuf_tbl[256];
unsigned short *__isbuf = isbuf_tbl;
static short toup_tbl[256];
short *__toup = toup_tbl;

void l370_init(void)
{
    int c;

    for (c = 0; c < 256; c++) toup_tbl[c] = (short)c;
    for (c = 'a'; c <= 'z'; c++) toup_tbl[c] = (short)(c - 'a' + 'A');
    for (c = '0'; c <= '9'; c++) isbuf_tbl[c] |= 0x0008U;   /* isdigit */
}
