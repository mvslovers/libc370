/*
 * tstabuf.c - libc370 #90 regression probe (MVS target, batch).
 *
 * ISSUE #90: @@AOPEN's cleanup for "buffer 1 obtained, VBS record area
 * not" was written as
 *
 *     FREEMAIN R,LV=(0),A=(1),SP=SUBPOOL     (@@aopen.asm:460)
 *
 * which the FREEMAIN macro rejects (IHB019: SP= is not allowed with
 * LV=(0) - the R form wants SP packed into R0's high byte by the
 * caller) and then MEXITs having generated NO CODE AT ALL.  as370
 * treats the severity-12 MNOTE as a printing no-op, so the build
 * stayed green and the #83 failure path silently leaks the buffer
 * obtained at @@aopen.asm:435 - precisely when storage is already
 * short.
 *
 * Reproduction: SYSUT1 is RECFM=VS with LRECL(27994) far above
 * BLKSIZE(3120), so buffer 1 (BLKSIZE+4 -> 3128 getmained) is small
 * and the VBS record area (LRECL+4 = 27998) is huge.  The probe
 * exhausts its region (256K -> 4K -> 64-byte phases, the tstaopn
 * technique - after the last phase no free hole is 64 bytes or
 * larger), then builds a DETERMINISTIC window: free one 256K block
 * (a 262208-byte hole) and plug it with a 240000-byte malloc
 * (240064 getmained), leaving one 22144-byte gap that is the only
 * hole in the region bigger than 63 bytes.  The open's transients -
 * save area (~500), DCB area (ZDCBLEN 392), buffer 1 (3128) - can
 * only come from that gap and fit it; the 28K VBS area cannot, so
 * every __aopen() walks exactly onto the broken cleanup path and
 * returns -12.
 *
 *   pre-fix:  each attempt leaks buffer 1 (3128) into the gap; after
 *             ATTEMPTS=3 opens 9384 bytes are gone and an 18000-byte
 *             probe malloc no longer fits (red, RC 8)
 *   post-fix: the FREEMAIN exists, the gap survives all attempts
 *             whole, the 18000-byte probe fits (green, RC 0)
 *
 * The window probe is the oracle, not the return codes: buffer-1
 * failure and VBS-area failure both surface as -12, only the leak
 * tells them apart.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstabuf.c -flinker-output=iebcopy -o TSTABUF
 *     ld370 --pack TSTABUF.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstabuf.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvssupa.h>
#include <clibwto.h>

#define P1CHUNK (256 * 1024)
#define P2CHUNK 4096
#define P1MAX   8
#define P2MAX   512
#define P3MAX   16384

#define ATTEMPTS  3             /* squeezed opens, one leak each pre-fix */
#define PLUG      240000        /* rounds to 240064 in the 262208 hole */
#define WPROBE    18000         /* rounds to 18048: fits the whole
                                   22144 gap, not the leaked-on gap   */

static void *p1[P1MAX];
static void *p2[P2MAX];
static void *p3[P3MAX];

static int check(const char *what, int ok);

int main(void)
{
    unsigned    n1 = 0, n2 = 0, n3 = 0, i;
    int         bad = 0;
    int         rc12 = 0;
    int         mode, recfm, lrecl, blksize;
    void        *asmbuf;
    void        *h;
    int         a_rc[ATTEMPTS];

    /* primes the stdio buffers while storage is still plentiful */
    printf("TSTABUF - libc370 #90 probe\n\n");

    /* exhaust the region: large, medium, then 64-byte granules */
    while (n1 < P1MAX && (p1[n1] = malloc(P1CHUNK)) != NULL) n1++;
    while (n2 < P2MAX && (p2[n2] = malloc(P2CHUNK)) != NULL) n2++;
    while (n3 < P3MAX && (p3[n3] = malloc(1))       != NULL) n3++;
    bad += check("region was exhausted (64-byte phase ran dry)",
                 n3 < P3MAX);
    bad += check("the 256K phase yielded a block to carve", n1 >= 1);
    if (n1 < 1) goto out;

    /* the deterministic window: one 256K hole, mostly plugged */
    n1--;
    free(p1[n1]);
    p1[n1] = malloc(PLUG);      /* plug stays allocated to the end */
    bad += check("window plug obtained", p1[n1] != NULL);

    /* THE #90 CASES: transients + buffer 1 fit the 22K gap, the 28K
       VBS record area never does - every attempt exercises the
       broken cleanup path */
    for (i = 0; i < ATTEMPTS; i++) {
        mode = 1;  recfm = 0;  lrecl = 27994;  blksize = 3120;
        asmbuf = NULL;
        h = __aopen("SYSUT1  ", &mode, &recfm, &lrecl, &blksize,
                    &asmbuf, NULL);
        a_rc[i] = (int)h;
        if (a_rc[i] == -12) rc12++;
        if (a_rc[i] > 0) __aclose(h);   /* unexpected success: window
                                           was not tight enough */
    }
    wtof("TSTABUF: squeezed opens rc=%d,%d,%d",
         a_rc[0], a_rc[1], a_rc[2]);
    bad += check("every squeezed open failed with -12", rc12 == ATTEMPTS);

    /* THE ORACLE: the gap must still take one WPROBE-sized block.
       Pre-fix three 3128-byte corpses ate 9384 of the 22144 bytes
       and 18048 contiguous no longer exist; post-fix the gap is
       whole. */
    h = malloc(WPROBE);
    wtof("TSTABUF: window re-allocation after %u opens: %s",
         (unsigned)ATTEMPTS, h ? "fits" : "DOES NOT FIT");
    bad += check("window survived the failing opens (pre-fix: leaked)",
                 h != NULL);
    if (h) free(h);

out:
    /* give the storage back */
    for (i = 0; i < n3; i++) free(p3[i]);
    for (i = 0; i < n2; i++) free(p2[i]);
    for (i = 0; i <= n1 && i < P1MAX; i++) if (p1[i]) free(p1[i]);

    /* the region must be whole: the same open succeeds with room */
    mode = 1;  recfm = 0;  lrecl = 27994;  blksize = 3120;
    asmbuf = NULL;
    h = __aopen("SYSUT1  ", &mode, &recfm, &lrecl, &blksize,
                &asmbuf, NULL);
    bad += check("__aopen works with the region whole", (int)h > 0);
    if ((int)h > 0) {
        __aclose(h);
    }

    printf("\n  exhaustion: %u x 256K, %u x 4K, %u x 64B\n", n1, n2, n3);
    printf("\nTSTABUF %s\n", bad ? "FAILED" : "PASSED");
    if (bad) wtof("TSTABUF FAILED (%d checks)", bad);
    else     wtof("TSTABUF PASSED");
    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-52s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
