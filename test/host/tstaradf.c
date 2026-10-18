/*
 * tstaradf.c - libc370 #398: arrayaddf() formats without a stack buffer
 * and frees its copy when arrayadd() fails.
 *
 * #includes the real @@aradf.c.  arrayadd() is a stand-in that records
 * the pointer it is given and fails on request; calloc() and free() are
 * counted, so a leaked copy shows as a count that does not come back.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstaradf.c && ./t
 *
 * RED, 2026-10-18, against main: ASan stack-buffer-overflow in
 * arrayaddf() on the first case, a 2000-character string.
 *
 * RC: 0 = every check passed, 1 = a check failed (ASan aborts on its own).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

static int live;            /* calloc() minus free() */

static void *tcalloc(size_t n, size_t s) { live++; return calloc(n, s); }
static void tfree(void *p) { if (p) live--; free(p); }

#define calloc tcalloc
#define free   tfree
#include "../../src/ext/array/@@aradf.c"
#undef calloc
#undef free

int *__errno(void) { static int e; return &e; }

static void *added;
static int  fail_add;

int arrayadd(void *varray, void *item)
{
    (void)varray;
    if (fail_add) return -1;
    added = item;
    return 0;
}

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

int main(void)
{
    static char long_s[2001];
    void        *array = (void *)1;     /* never dereferenced here */
    int         i;

    /* a loop: libc370's memset() is inline assembler */
    for (i = 0; i < 2000; i++) long_s[i] = 'L';
    long_s[2000] = 0;

    /* 1: longer than the old 1024-byte buffer */
    added = NULL; live = 0;
    check(arrayaddf(&array, "%s-%d", long_s, 7) == 0, "2000 characters: rc 0");
    check(added && strlen(added) == 2002 && !strcmp((char *)added + 2000, "-7"),
          "2000 characters: the whole string is added");
    tfree(added);
    check(live == 0, "2000 characters: one allocation");

    /* 2: an ordinary one */
    added = NULL;
    check(arrayaddf(&array, "A%cB", 'x') == 0 && added && !strcmp(added, "AxB"),
          "short string");
    tfree(added);

    /* 3: arrayadd() fails - the copy is freed */
    fail_add = 1; live = 0;
    check(arrayaddf(&array, "LOST %d", 1) == -1, "arrayadd fails: rc -1");
    check(live == 0, "arrayadd fails: the copy is freed");
    fail_add = 0;

    /* 4: NULL array */
    check(arrayaddf(NULL, "X") == -1, "NULL varray: rc -1");

    printf("tstaradf: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
