/*
 * tstardel.c - libc370 #583: arraydel() on a full array must not read the
 * word past its last element.
 *
 * The shift loop ran to index < count and so read element [count]; with
 * count == size that is one word past the allocation.  The value was
 * overwritten at once, so only the read was wrong - which is what
 * AddressSanitizer sees.
 *
 * The array block is built here, header followed by exactly size
 * elements, rather than by arraynew(): on the host a pointer is 8 bytes,
 * ARRAY_SIZE (sizeof(ARRAY) / sizeof(void *)) comes out 1 for a 12-byte
 * header, and arraynew()'s block is 4 bytes short of a full array - a
 * host artefact that would hide what this test is about.
 *
 * BUILD / RUN (host, from the repository root):
 *
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I include -I . -o t test/host/tstardel.c \
 *        src/ext/array/@@ardel.c && ./t
 *
 * RED, 2026-10-17: before the fix ASan stops with heap-buffer-overflow
 * (READ of size 8) in arraydel().
 *
 * RC: 0 = every check passed, 1 = a check failed (ASan aborts on its own).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ext/array.h"

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

/* a full array of n elements: the block holds the header and n slots */
static void **full(unsigned n, ARRAY **hdr)
{
    char    *block = malloc(sizeof(ARRAY) + n * sizeof(void *));
    void    **items;
    unsigned i;

    *hdr = (ARRAY *)block;
    memcpy((*hdr)->eye, ARRAY_EYE, 4);
    (*hdr)->size  = n;
    (*hdr)->count = n;
    items = (void **)(block + sizeof(ARRAY));
    for (i = 0; i < n; i++) items[i] = (void *)(unsigned long)(0x100 + i);
    return items;
}

int main(void)
{
    ARRAY   *hdr;
    void    **items = full(3, &hdr);
    void    *got;

    got = arraydel(&items, 1);          /* the first of three, full */
    check(got == (void *)0x100, "arraydel(1) returns the first item");
    check(hdr->count == 2, "count is 2");
    check(items[0] == (void *)0x101 && items[1] == (void *)0x102,
          "the rest shifted left");
    check(items[2] == NULL, "the freed slot is cleared");

    got = arraydel(&items, 2);          /* the last one */
    check(got == (void *)0x102, "arraydel(2) returns the last item");
    check(hdr->count == 1 && items[0] == (void *)0x101 && items[1] == NULL,
          "one left, its slot after cleared");

    check(arraydel(&items, 2) == NULL, "an index past count gives NULL");

    free(hdr);
    printf("tstardel: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
