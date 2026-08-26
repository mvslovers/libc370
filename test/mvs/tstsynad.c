/*
 * tstsynad.c - libc370 #147 item 3: an uncorrectable I/O error must
 * become ferror(), not ABEND S001.
 *
 * ISSUE #147: the DCBs @@AOPEN builds carry no SYNAD exit, so the
 * first genuine I/O error - bad track, wrong-length record, device
 * error - reaches CHECK/GET error processing with "NO ERROR HANDLING
 * (SYNAD) EXIT SPECIFIED" and kills the whole address space with
 * S001.  That is what turned ftpd#117's corrupted PUT into a dead
 * STC.  The fix plants a SYNAD on the BSAM path (@@ATROUT's WRITE +
 * CHECK, @@AREAD's READ + CHECK), records the error in the per-FILE
 * work area, and surfaces it as an error return the C layer turns
 * into _FILE_FLAG_ERROR + errno EIO.
 *
 * The deterministic error vector: round 1 writes a normal FB/3120
 * dataset.  Round 2 reads the SAME dataset back through a second DD
 * whose DCB override says BLKSIZE=80 - the first READ meets a
 * 3120-byte block with an 80-byte buffer, a wrong-length I/O error.
 * Today that is ABEND S001; the round runs under try() so the RED
 * probe reports the abend code instead of dying of it.  GREEN:
 * fgets() returns NULL with zero lines delivered, ferror() is set,
 * feof() is NOT, errno is EIO, fclose() survives, and the program -
 * this is the point - runs on.
 *
 * If the RED run shows neither an abend nor an error (BSAM quietly
 * accepting the long block), the vector is dead and the probe says so
 * loudly - that is a measurement too, and the search for a better
 * vector starts at the IOB flags this probe just exercised.
 *
 * Verdicts go out via wtof() as well as printf() (the #145 probe
 * lesson: an S001 on a stdio DCB can take buffered SYSPRINT with it).
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstsynad.c -flinker-output=iebcopy -o TSTSYNAD
 *     ld370 --pack TSTSYNAD.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstsynad.jcl (OUT80 = the dataset, BADIN = the same
 * dataset with the lying DCB override).  Built by hand - libc370 is
 * the cc370 sysroot, not an mbt project.
 *
 * RC: 0 = all checks passed, 8 = at least one did not.
 */
#include <stdio.h>
#include <errno.h>
#include <cliblock.h>
#include <clibwto.h>
#include <clibtry.h>

#define NRECS   20

static int  bad = 0;

static int check(const char *what, int ok)
{
    printf("  %-52s %s\n", what, ok ? "ok" : "*** FAIL");
    if (!ok) bad = 1;
    return ok;
}

/* ---- round 2 under try(): the read that hits the wrong-length ------- */

typedef struct rdcase RDCASE;
struct rdcase {
    int     opened;         /* fopen succeeded                          */
    int     lines;          /* lines fgets delivered                    */
    int     ferr;           /* ferror() after the NULL                  */
    int     fend;           /* feof() after the NULL                    */
    int     err;            /* errno after the NULL                     */
    int     closed;         /* fclose returned                          */
};

static int readcase(void *p)
{
    RDCASE  *r = (RDCASE *)p;
    FILE    *fp;
    char    line[100];

    fp = fopen("dd:BADIN", "r");
    if (!fp) return 0;
    r->opened = 1;

    errno = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        r->lines++;
        if (r->lines > NRECS + 2) break;    /* runaway guard            */
    }
    r->ferr = ferror(fp) ? 1 : 0;
    r->fend = feof(fp) ? 1 : 0;
    r->err  = errno;

    fclose(fp);
    r->closed = 1;
    return 0;
}

int main(void)
{
    FILE        *fp;
    static RDCASE r;
    int         i, t;

    printf("TSTSYNAD - libc370 #147 item 3: I/O error -> ferror, not S001\n\n");
    wtof("TSTSYNAD start, main TCB=%08X", ((unsigned *)0)[0x21C / 4]);

    /* ---- (1) write the dataset the reader will stumble over --------- */
    printf("(1) write %d FB/3120 records to dd:OUT80:\n", NRECS);
    fp = fopen("dd:OUT80", "w");
    if (!check("(1) fopen dd:OUT80 for write", fp != NULL)) goto verdict;
    for (i = 0; i < NRECS; i++) {
        fprintf(fp, "RECORD-%02d: the quick brown fox jumps over it\n", i);
    }
    fclose(fp);
    check("(1) written and closed", 1);
    wtof("TSTSYNAD r1: %d records written", NRECS);

    /* ---- (2) read it back through the lying DCB override ------------ */
    printf("\n(2) read it via dd:BADIN (DCB says BLKSIZE=80):\n");
    t = try(readcase, &r);
    wtof("TSTSYNAD r2: try=%03X opened=%d lines=%d ferr=%d feof=%d "
         "errno=%d closed=%d",
         t, r.opened, r.lines, r.ferr, r.fend, r.err, r.closed);

    check("(2) fopen dd:BADIN succeeded", r.opened == 1);
    check("(2) no abend (RED: S001 here)", t == 0);
    check("(2) zero lines delivered", r.lines == 0);
    check("(2) ferror() is set", r.ferr == 1);
    check("(2) feof() is NOT set", r.fend == 0);
    check("(2) errno is EIO", r.err == EIO);
    check("(2) fclose() survived", r.closed == 1);

    if (t == 0 && r.lines > 0 && r.ferr == 0) {
        /* the third outcome: BSAM swallowed the long block - vector dead */
        wtof("TSTSYNAD r2: VECTOR DEAD - no abend, no error, %d lines",
             r.lines);
    }

    /* ---- (3) the address space is still alive ----------------------- */
    printf("\n(3) still running after the broken stream:\n");
    check("(3) main() still in control", 1);

verdict:
    printf("\nTSTSYNAD %s\n", bad ? "FAILED" : "PASSED");
    wtof("TSTSYNAD %s", bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
