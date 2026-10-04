/*
 * tstnulin.c - libc370 #277 and #254 on MVS: stdin when the step has no
 * SYSIN, at a small REGION.
 *
 * #277: @@start opens stdin as dd:SYSIN, and when that DD is missing as
 * 'NULLFILE' - a DUMMY.  __fpopen() gave every non-record stream a C
 * buffer of the DCB's record or block size, and a DUMMY with no DCB
 * attributes comes back RECFM=U - so a 32 K calloc for a stream that never
 * transfers a byte, and at REGION=704K rexx370's IRXJCL ended CC 12 before
 * main() (MVSCE-LAB JOB01462).
 * #254: the message for it was "SYSIN DD not defined", in a dynamic SYSOUT.
 *
 * This program prints what stdin was opened as, reads it (end of file at
 * once for a DUMMY, the data for an instream SYSIN), and opens 'NULLFILE'
 * once more by name.  jcl/tstnulin.jcl runs it at a ladder of REGIONs
 * without SYSIN, for this tree's libc.a (TSTNUL) and the installed one
 * (TSTNULR): the lowest REGION each still reaches main() at is the
 * measurement, and the job log shows what @@start said where it did not.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstnulin.c -o TSTNUL -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstnulin.c -o TSTNULR -flinker-output=iebcopy
 *          ld370 --pack TSTNUL=TSTNUL.iebcopy TSTNULR=TSTNULR.iebcopy \
 *                -o tstnulin -xmit --dsn IBMUSER.LIBC370.NULSCR
 * Install: jcl/recvnul.jcl.   Run: jcl/tstnulin.jcl.
 *
 * mvsdev JOB01354, 2026-10-04 (RECEIVE JOB01353), REGION 304K-448K in 16K
 * steps: TSTNUL reaches main() and passes from 336K, stdin NULLFILE
 * LRECL/BLKSIZE 80; TSTNULR first reaches main() at 448K, stdin 32760/32760,
 * and has no room left for the second NULLFILE open there.  Below 336K the
 * new library says "@@START: stderr (SYSTERM) could not be opened: errno
 * 12, out of storage - raise REGION" in the job log; the old one printed
 * "SYSTERM DD not defined" to SYSPRINT.  JOB01349 (the first run, buffer
 * fix only) showed the 64 K of __aopen() buffers still standing.
 *
 * RC: 0 = main() reached and every check passed, 1 = a check failed;
 * 12 from @@start = main() never ran.
 */
#include <stdio.h>
#include <string.h>

static int bad = 0;

static void show(const char *what, FILE *fp)
{
    printf("  %-10s dataset '%s' recfm %02X lrecl %d blksize %d\n", what,
           fp->dataset, fp->recfm, fp->lrecl, fp->blksize);
}

int main(int argc, char **argv)
{
    char  line[100];
    int   n = 0;
    FILE *fp;

    printf("TSTNUL main() reached, PARM '%s'\n", argc > 1 ? argv[1] : "");
    show("stdin", stdin);
    while (fgets(line, sizeof(line), stdin)) {
        n++;
        if (n == 1) printf("  first line: %s", line);
    }
    printf("  stdin: %d line(s), feof %d, ferror %d\n", n, feof(stdin) != 0,
           ferror(stdin) != 0);
    if (ferror(stdin) || !feof(stdin)) bad++;
    if (argc > 1 && strcmp(argv[1], "DATA") == 0 && n != 2) {
        printf("  *** FAIL - want the 2 instream lines\n");
        bad++;
    }

    fp = fopen("'NULLFILE'", "r");
    if (!fp) {
        printf("  *** FAIL - fopen('NULLFILE') returned NULL\n");
        bad++;
    }
    else {
        show("NULLFILE", fp);
        if (fgets(line, sizeof(line), fp) || !feof(fp)) {
            printf("  *** FAIL - NULLFILE was not at end of file\n");
            bad++;
        }
        fclose(fp);
    }

    printf("TSTNUL %s\n", bad ? "FAILED" : "PASSED");
    return bad ? 1 : 0;
}
