/*
 * tstscnll.c - libc370 #318: scanf's length modifiers hh, ll, j, z, t
 * and L, next to h and l.
 *
 * ISSUE #318: vvscanf() knew h and l only.  %lld set "long" twice and
 * stored a long - on S/370 the HIGH word of a long long, the low word
 * left as it was; %hhd stored a short into a char object, one byte past
 * it; j, z and t were not modifiers at all and derailed the format.
 *
 * This run #includes src/stdio/vvscanf.c and drives its string path
 * (fp == NULL), which is what sscanf() uses.  Every target sits between
 * two guard bytes, so a store of the wrong width shows as a changed
 * guard, not only as a wrong value.  test/mvs/tstscnll.c runs the same
 * cases on MVS through the real sscanf().
 *
 * <ctype.h> is suppressed (its macros index the EBCDIC tables), and so
 * is <string.h> (its memset() is inline S/370 assembler, see
 * test/host/tstfprls.c); the host's functions stand in.
 *
 * BUILD / RUN (host, from the repository root):
 *
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I include -I . -o t test/host/tstscnll.c && ./t
 *
 * GREEN 23/23 (36/36 with #316's digit and float cases).  RED, 2026-10-03, the same test against vvscanf.c before
 * #318: 8 cases fail (hh, j, L, z, t, the negated %lu) and %hhn then
 * overruns its char under ASan.  The %lld cases pass even there: host
 * long is 64 bits, so storing a long fills a long long.  That half of
 * #318 is visible on MVS only - test/mvs/tstscnll.c.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#define __CTYPE_INCLUDED
#define STRING_H
#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned long size_t;
#endif
void *memset(void *, int, size_t);
void *memchr(const void *, int, size_t);
char *strchr(const char *, int);
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int toupper(int c);
int tolower(int c);

#include "../../src/stdio/vvscanf.c"

static int run = 0, failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        run++;                                                            \
        if (!(cond)) { failed++; printf("  FAIL: %s\n", (msg)); }         \
    } while (0)

static int scan(const char *s, const char *f, ...)
{
    va_list ap;
    int r;

    va_start(ap, f);
    r = vvscanf(f, ap, NULL, s);
    va_end(ap);
    return (r);
}

#include "../mvs/tstscncase.h"

int main(void)
{
    printf("=== tstscnll: scanf length modifiers (#318), host ===\n");
    scn_cases();
    printf("=== tstscnll: %d/%d passed ===\n", run - failed, run);
    return failed > 0 ? 1 : 0;
}
