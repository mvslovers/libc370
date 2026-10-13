/*
 * tstpdx37.c - libc370 #524 on MVS: a member whose write failed is not
 * STOWed.
 *
 * ISSUE #524 (measured by the mvsMF side, mvsmf#406, on 2.10.2): when the
 * final block of a member failed with an x37, fclose() returned the error
 * (RC 12, ENOSPC), and @@ACLOSE then issued CLOSE anyway - which STOWs the
 * member.  A new member stayed in the directory with 0 records; an existing
 * one was replaced by what reached DASD, its old content lost.  Now a
 * failed write sets DCBCNSTO before CLOSE, and the close executor
 * (IGG0201Z) skips its STOW.
 *
 * DD-less: the PDS IBMUSER.LIBC370.PDX37 is allocated by
 * jcl/tstpdx37.jcl, small (TRK(2,0), one directory block, FB 80, BLKSIZE
 * 19040, mvsMF's shape) and deleted again.  Each step gets a fresh one.
 *
 *   (1) members M1, M2, ... of 47 records until one fails: the failed one
 *       is not in the directory (fopen for read fails), the ones before
 *       it are, with their 47 records
 *   (2) M1 overwritten in the full PDS: the write fails, and M1 still
 *       holds its 47 old records
 *   (3) a new member written until fwrite() itself fails, then fclose():
 *       not in the directory either
 *
 * Built twice: TSTPDX37 against this tree's libc.a, TSTPDX3R against
 * 2.10.2's - the red control (the failed member listed, M1 emptied).
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstpdx37.c -o TSTPDX37 -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -L <2.10.2 sysroot>/lib \
 *                test/mvs/tstpdx37.c -o TSTPDX3R -flinker-output=iebcopy
 *          ld370 --pack TSTPDX37=TSTPDX37.iebcopy \
 *                --pack TSTPDX3R=TSTPDX3R.iebcopy \
 *                -o tstpdx37 -xmit --dsn IBMUSER.LIBC370.PXSCR
 * Install: jcl/recvpdx3.jcl.   Run: jcl/tstpdx37.jcl.
 *
 * dev1 JOB02161, 2026-10-13 (RECEIVE JOB02160): GREEN CC 0000, 7/7;
 * RED (2.10.2) CC 0001, 4/7 - M9 left with 0 records, M1 emptied, BIG left
 * with 0 records.  M9 is the first to fail, as in mvsMF's measurement.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define PDS     "IBMUSER.LIBC370.PDX37"
#define NREC    47

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static void name(char *buf, const char *mem)
{
    sprintf(buf, "'%s(%s)'", PDS, mem);
}

/* write n records of "<mem> <i> <tag>"; the result of fclose(), or -2 when
   a write failed (then fclose() is still called) */
static int put(const char *mem, int n, const char *tag, int *wfail)
{
    char  dsn[64], rec[81];
    FILE *fp;
    int   i;

    *wfail = 0;
    name(dsn, mem);
    fp = fopen(dsn, "w");
    if (!fp) return -9;
    for (i = 0; i < n; i++) {
        sprintf(rec, "%-8s %05d %s", mem, i, tag);
        if (fputs(rec, fp) == EOF || fputc('\n', fp) == EOF) {
            *wfail = 1;
            break;
        }
    }
    return fclose(fp);
}

/* the number of records in a member, -1 when it cannot be opened (not in
   the directory); *first the first record */
static int count(const char *mem, char *first)
{
    char  dsn[64], rec[100];
    FILE *fp;
    int   n = 0;

    name(dsn, mem);
    fp = fopen(dsn, "r");
    if (!fp) return -1;
    while (fgets(rec, sizeof rec, fp)) {
        if (n == 0 && first) strcpy(first, rec);
        n++;
    }
    fclose(fp);
    return n;
}

int main(void)
{
    char    mem[9], msg[120], first[100];
    int     i, rc, wf, failed = 0, n;

    printf("=== tstpdx37: no STOW for a member whose write failed (#524) ===\n");

    /* (1) fill the PDS member by member */
    for (i = 1; i <= 40; i++) {
        sprintf(mem, "M%d", i);
        rc = put(mem, NREC, "ORIGINAL", &wf);
        if (rc != 0 || wf) {
            failed = i;
            printf("    M%d: fclose() %d, write failed %d, errno %d\n", i, rc, wf, errno);
            break;
        }
    }
    sprintf(msg, "the PDS filled up: M%d failed, M1..M%d written", failed, failed - 1);
    CHECK(failed > 1, msg);
    if (failed > 1) {
        n = count("M1", NULL);
        sprintf(msg, "M1 before the failure holds %d records (%d)", n, NREC);
        CHECK(n == NREC, msg);
        sprintf(mem, "M%d", failed);
        n = count(mem, NULL);
        sprintf(msg, "the failed %s is not in the directory (%d; -1 = absent)", mem, n);
        CHECK(n == -1, msg);
    }

    /* (2) overwrite M1 in the full PDS */
    rc = put("M1", NREC, "OVERWRITE", &wf);
    sprintf(msg, "overwriting M1 in the full PDS fails: fclose() %d, write failed %d", rc, wf);
    CHECK(rc != 0 || wf, msg);
    first[0] = 0;
    n = count("M1", first);
    sprintf(msg, "M1 still holds its %d old records (%d)", NREC, n);
    CHECK(n == NREC && strstr(first, "ORIGINAL") != NULL, msg);

    /* (3) a new member whose fwrite() fails before fclose() */
    rc = put("BIG", 5000, "BIG", &wf);
    sprintf(msg, "a 5000-record member fails while writing: write failed %d, fclose() %d", wf, rc);
    CHECK(wf || rc != 0, msg);
    n = count("BIG", NULL);
    sprintf(msg, "BIG is not in the directory (%d; -1 = absent)", n);
    CHECK(n == -1, msg);

    printf("tstpdx37 %d run, %d passed, %d failed\n", mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
