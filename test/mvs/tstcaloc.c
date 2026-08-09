/*
 * tstcaloc.c - libc370 #84 regression probe (MVS target, batch).
 *
 * ISSUE #84: calloc() computed
 *
 *     total = ((nmemb * size) + 7) & 0x00FFFFF8;
 *
 * The mask rounds up to 8 - fine - but it also TRUNCATES the product
 * to 24 bits.  calloc(1, 0x1000009) therefore became a valid 16-byte
 * allocation for a 16 MB request, and the caller overran the heap at
 * first use.  No message, no NULL, no abend at the allocation site.
 * There was no nmemb*size overflow check at all, so a product past
 * 32 bits wrapped the same silent way.
 *
 * The fix refuses any product a 32-bit size_t cannot hold (NULL,
 * errno=ENOMEM) and otherwise passes the full untruncated total to
 * malloc(), whose 6 MB MAX_CHUNK cap rejects merely-huge requests.
 *
 * This is an assertion-red probe, no abend involved:
 *
 *   pre-fix:  calloc(1, 16M+9) returns a tiny non-NULL block (the
 *             prefix at p-4 says 16), calloc(0x8001, 0x20000) returns
 *             a 128K block for a 4 GB request - checks FAIL, RC=8
 *   post-fix: both return NULL with errno=ENOMEM, RC=0
 *
 * Console note: each refused request that reaches malloc() emits its
 * 'Out of memory' WTO - expected, not a failure.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstcaloc.c -flinker-output=iebcopy -o TSTCALOC
 *     ld370 --pack TSTCALOC.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstcaloc.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static int check(const char *what, int ok);

int main(void)
{
    unsigned char   *p;
    unsigned        i;
    int             bad = 0;
    int             zeroed;

    printf("TSTCALOC - libc370 #84 probe\n\n");

    /* a normal calloc still works, is zeroed, and carries the rounded
       request size in the prefix word realloc() reads */
    p = calloc(3, 100);
    zeroed = (p != NULL);
    if (p) {
        for (i = 0; i < 300; i++) {
            if (p[i] != 0) { zeroed = 0; break; }
        }
    }
    bad += check("calloc(3,100) works and is zeroed", zeroed);
    bad += check("... prefix at p-4 is the rounded total (304)",
                 p != NULL && ((unsigned *)(void *)p)[-1] == 304);
    free(p);

    /* THE #84 CASES - silent 24-bit truncation */
    errno = 0;
    p = calloc(1, 0x1000009);   /* 16 MB + 9 */
    if (p != NULL) {
        printf("  (pre-fix behavior: got %u bytes for a 16M request)\n",
               ((unsigned *)(void *)p)[-1]);
    }
    bad += check("calloc(1,16M+9) returns NULL (pre-fix: 16 bytes)",
                 p == NULL);
    bad += check("... and sets errno=ENOMEM", p == NULL && errno == ENOMEM);
    free(p);

    errno = 0;
    p = calloc(0x8001, 0x20000);    /* 4 GB + 128K, wraps to 128K */
    if (p != NULL) {
        printf("  (pre-fix behavior: got %u bytes for a 4G request)\n",
               ((unsigned *)(void *)p)[-1]);
    }
    bad += check("calloc(32769,128K) returns NULL (pre-fix: 128K)",
                 p == NULL);
    bad += check("... and sets errno=ENOMEM", p == NULL && errno == ENOMEM);
    free(p);

    /* product wrapping to exactly 0 - NULL on both sides, pinned so
       the overflow check never lets it through as calloc(0) */
    errno = 0;
    p = calloc(0x10000, 0x10000);   /* 2^32 exactly */
    bad += check("calloc(64K,64K) returns NULL", p == NULL);

    /* a legitimately large request must still succeed and be zeroed */
    p = calloc(1, 5 * 1024 * 1024);
    zeroed = (p != NULL);
    if (p) {
        if (p[0] != 0 || p[5 * 1024 * 1024 - 1] != 0) zeroed = 0;
    }
    bad += check("calloc(1,5M) still works and is zeroed", zeroed);
    free(p);

    printf("\nTSTCALOC %s\n", bad ? "FAILED" : "PASSED");

    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-50s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
