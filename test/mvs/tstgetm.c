/*
 * tstgetm.c - libc370 #81 regression probe (MVS target, batch).
 *
 * ISSUE #81: malloc() could not fail.  @@GETM issued GETMAIN RU
 * (register form, UNconditional), which does not return a failure code
 * when storage is short - it abends S878.  So every `if (!p)` in libc370
 * and its consumers was dead code for the case it was written for:
 * storage shortage never surfaced as NULL, it surfaced as an abend
 * somewhere down whatever call chain asked for memory next (the #217
 * death spiral in httpd/mvsmf).
 *
 * The fix makes @@GETM use GETMAIN RC (conditional) and return NULL on
 * a nonzero R15, and malloc() sets errno=ENOMEM on the failure path.
 *
 * This probe runs in a deliberately small region (see jcl/tstgetm.jcl,
 * REGION=2048K) and allocates 256K blocks until the region is exhausted:
 *
 *   pre-fix:  the exhaustion loop abends S878 - the job fails (red)
 *   post-fix: the shortage arrives as NULL with errno=ENOMEM, every
 *             block is freed, allocation works again, RC=0 (green)
 *
 * The "do not probe storage by allocating" caution in #81 is about
 * production servers; a dedicated batch job with its own REGION is the
 * controlled version of the same experiment - the shortage stays inside
 * this address space.
 *
 * Console note: each failed malloc() emits one 'Out of memory' WTO plus
 * a save area traceback.  This job produces a few of them by design;
 * they are the (previously unreachable) diagnostic firing, not errors.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstgetm.c -flinker-output=iebcopy -o TSTGETM
 *     ld370 --pack TSTGETM.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstgetm.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define CHUNK   (256 * 1024)    /* per-allocation size */
#define MAXBLK  256             /* 64M cap - far beyond any REGION */

static int check(const char *what, int ok);

int main(void)
{
    static void *blk[MAXBLK];
    void        *p;
    unsigned    n   = 0;
    unsigned    i;
    int         bad = 0;
    int         e_exh;

    /* primes the stdio buffers while storage is still plentiful */
    printf("TSTGETM - libc370 #81 probe\n\n");

    /* invariants that never reach GETMAIN */
    p = malloc(0);
    bad += check("malloc(0) returns NULL", p == NULL);

    errno = 0;
    p = malloc(7 * 1024 * 1024);
    bad += check("malloc(7M) over MAX_CHUNK returns NULL", p == NULL);
    bad += check("... and sets errno=ENOMEM", errno == ENOMEM);

    /* the @@GETM prefix must survive the fix: the caller-requested size
       sits at p-4 (realloc() reads it) */
    p = malloc(100);
    bad += check("prefix intact: requested size at p-4",
                 p != NULL && ((unsigned *)p)[-1] == 100);
    free(p);

    /* THE #81 CASE: drive the region to exhaustion.  Pre-fix this loop
       abends S878 at the shortage; post-fix it gets a NULL. */
    errno = 0;
    while (n < MAXBLK && (blk[n] = malloc(CHUNK)) != NULL) {
        n++;
    }
    e_exh = errno;

    for (i = 0; i < n; i++) {
        free(blk[i]);
    }

    bad += check("shortage arrived as NULL (pre-fix: S878 abend)", n < MAXBLK);
    bad += check("at least one allocation succeeded first", n > 0);
    bad += check("errno is ENOMEM at the shortage", e_exh == ENOMEM);

    /* the region must be whole again after freeing everything */
    p = malloc(CHUNK);
    bad += check("allocation works again after free-all", p != NULL);
    free(p);

    printf("\n  %u x %uK obtained before the region was exhausted\n",
           n, CHUNK / 1024);
    printf("\nTSTGETM %s\n", bad ? "FAILED" : "PASSED");

    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-50s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
