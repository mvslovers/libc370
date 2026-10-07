/*
 * tstgetl.c - libc370 #467 on MVS: fopen("*GETLINE", "r"), a stream that
 * reads through the TMP's GETLINE - SYSTSIN in a batch TMP.
 *
 * The measurements jcl/tstgetl.jcl takes, under a batch TMP:
 *   (1) TSTGET reads two lines; the TMP must not run them as commands,
 *       and runs the command after them
 *   (2) TSTGET 'ALL' reads to the end of SYSTSIN: how many lines, and
 *       whether feof() is set; then the TMP must end cleanly
 *   (4) the record shape: each line is printed between [ ] with its
 *       length, so leading and trailing blanks show
 *   NOTMP: plain batch, the open must fail with ENODEV
 * and in every run "*GETLINE" for writing and "*PUTLINE" for reading must
 * fail with EINVAL.
 *
 * Built twice from this source: TSTGET against this tree's libc.a,
 * TSTGETR against the installed one, the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstgetl.c -o TSTGET -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstgetl.c -o TSTGETR -flinker-output=iebcopy
 *          ld370 --pack TSTGET=TSTGET.iebcopy TSTGETR=TSTGETR.iebcopy \
 *                -o tstgetl -xmit --dsn IBMUSER.LIBC370.GETSCR
 * Install: jcl/recvget.jcl.   Run: jcl/tstgetl.jcl.
 *
 * mvsdev JOB01626, 2026-10-07 (RECEIVE JOB01625): TMP CC 0000 - TSTGET
 * read both lines (26 and 43 bytes, the leading blanks kept, the FB 80
 * padding gone), the TMP ran neither as a command and ran LISTALC; 'ALL'
 * read the last 2 lines, then feof 1, and the TMP ended CC 0000.  NOTMP:
 * NULL, errno 19.  RED: TMPR read 0 lines with ferror 1 and the TMP ran
 * both lines as commands (IKJ56500I); NOTMPR opened, RC 1.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int failed;

static void expect_einval(const char *name, const char *mode)
{
    FILE *fp;

    errno = 0;
    fp = fopen(name, mode);
    printf("  fopen(\"%s\", \"%s\"): %s, errno %d\n", name, mode,
           fp ? "non-NULL" : "NULL", errno);
    if (fp) {
        fclose(fp);
        failed++;
    }
    else if (errno != EINVAL) failed++;
}

int main(int argc, char **argv)
{
    const char  *arg = argc > 1 ? argv[1] : "";
    FILE        *fp;
    char        line[1100];
    int         n = 0, want, all = strcmp(arg, "ALL") == 0;

    printf("TSTGET %s\n", arg);
    errno = 0;
    fp = fopen("*GETLINE", "r");
    printf("  fopen(\"*GETLINE\", \"r\"): %s, errno %d\n",
           fp ? "non-NULL" : "NULL", errno);

    if (strcmp(arg, "NOTMP") == 0) {
        if (fp) {
            fclose(fp);
            return 1;
        }
        return errno == ENODEV ? 0 : 1;
    }
    if (!fp) return 1;

    want = all ? 1000 : 2;
    while (n < want && fgets(line, sizeof(line), fp)) {
        size_t len = strcspn(line, "\n");
        line[len] = '\0';
        n++;
        printf("  line %d, %u bytes: [%s]\n", n, (unsigned)len, line);
    }
    printf("  %d line(s); feof %d, ferror %d\n", n, feof(fp) != 0,
           ferror(fp) != 0);
    if (all ? !feof(fp) : n != 2) failed++;
    if (ferror(fp)) failed++;
    if (fclose(fp) != 0) failed++;

    expect_einval("*GETLINE", "w");
    expect_einval("*PUTLINE", "r");

    printf("  %s\n", failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
