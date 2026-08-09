/*
 * tstaopn.c - libc370 #83 regression probe (MVS target, batch).
 *
 * ISSUE #83: after #81/#82 made malloc() fail cleanly, three paths kept
 * their unconditional GETMAINs and still abended S878 on storage
 * shortage instead of failing the call:
 *
 *   - the FUNHEAD SAVE=(name,len,sp) dynamic save area (mvsmacs.macro),
 *     used by @@AOPEN, @@ACLOSE, @@ALINE and @@SYSTEM - so fopen()
 *     abended before its own code ran
 *   - @@AOPEN's DCB area and its three buffer GETMAINs
 *   - @@SVC99's work area
 *
 * The fix makes them all GETMAIN RC with per-site cleanup:
 *
 *   __aopen()  ->  -1  when the save area cannot be obtained,
 *                  -12 (-ENOMEM) from the DCB/buffer sites, with
 *                  everything acquired up to that point released
 *   __svc99()  ->  -1, the SVC is never issued
 *
 * This probe exhausts its own region (256K -> 4K -> 64-byte phases;
 * after the last phase no free hole is 64 bytes or larger, so the
 * ~480-byte save area and the 96-byte SVC99 work area cannot be
 * obtained) and then calls __aopen() and __svc99() directly:
 *
 *   pre-fix:  __aopen abends S878 in the FUNHEAD GETMAIN (red)
 *   post-fix: both return negative, everything is freed, and the
 *             same __aopen then succeeds against DD:SYSUT1 (green)
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstaopn.c -flinker-output=iebcopy -o TSTAOPN
 *     ld370 --pack TSTAOPN.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstaopn.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <mvssupa.h>
#include <rb99.h>

#define P1CHUNK (256 * 1024)
#define P2CHUNK 4096
#define P1MAX   8
#define P2MAX   512
#define P3MAX   16384

static void *p1[P1MAX];
static void *p2[P2MAX];
static void *p3[P3MAX];

static int check(const char *what, int ok);

int main(void)
{
    unsigned    n1 = 0, n2 = 0, n3 = 0, i;
    int         bad = 0;
    int         mode, recfm, lrecl, blksize;
    void        *asmbuf = NULL;
    void        *h;
    int         aopen_oom, svc99_oom;
    RB99        rb99;

    /* primes the stdio buffers while storage is still plentiful */
    printf("TSTAOPN - libc370 #83 probe\n\n");

    /* exhaust the region: large, medium, then 64-byte granules */
    while (n1 < P1MAX && (p1[n1] = malloc(P1CHUNK)) != NULL) n1++;
    while (n2 < P2MAX && (p2[n2] = malloc(P2CHUNK)) != NULL) n2++;
    while (n3 < P3MAX && (p3[n3] = malloc(1))       != NULL) n3++;

    /* THE #83 CASES: these must fail, not abend.  Pre-fix, __aopen
       abends S878 right here in the FUNHEAD save area GETMAIN. */
    mode = 1;  recfm = 0;  lrecl = 80;  blksize = 3120;
    h = __aopen("SYSUT1  ", &mode, &recfm, &lrecl, &blksize,
                &asmbuf, NULL);
    aopen_oom = (int)h;

    memset(&rb99, 0, sizeof(rb99));
    svc99_oom = __svc99(&rb99);

    /* give the storage back */
    for (i = 0; i < n3; i++) free(p3[i]);
    for (i = 0; i < n2; i++) free(p2[i]);
    for (i = 0; i < n1; i++) free(p1[i]);

    bad += check("region was exhausted (64-byte phase ran dry)",
                 n3 < P3MAX);
    bad += check("__aopen under shortage failed (pre-fix: S878)",
                 aopen_oom < 0);
    bad += check("__svc99 under shortage returned -1 (pre-fix: S878)",
                 svc99_oom == -1);

    /* the region must be whole again: the same open must now work */
    mode = 1;  recfm = 0;  lrecl = 80;  blksize = 3120;
    asmbuf = NULL;
    h = __aopen("SYSUT1  ", &mode, &recfm, &lrecl, &blksize,
                &asmbuf, NULL);
    bad += check("__aopen works again after free-all", (int)h > 0);
    if ((int)h > 0) {
        __aclose(h);
    }

    printf("\n  exhaustion: %u x 256K, %u x 4K, %u x 64B;"
           " __aopen oom rc=%d, __svc99 oom rc=%d\n",
           n1, n2, n3, aopen_oom, svc99_oom);
    printf("\nTSTAOPN %s\n", bad ? "FAILED" : "PASSED");

    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-50s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
