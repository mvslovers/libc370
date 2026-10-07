/*
 * tstputl.c - libc370 #463 on MVS: fopen("*PUTLINE", "w"), a stream that
 * writes through the TMP's PUTLINE.
 *
 * Under a batch TMP (PGM=IKJEFT01) PUTLINE writes through the TMP's own
 * SYSTSPRT, in order with the TMP's messages.  @@aopen finds the TMP's
 * ECT and UPT through the LWA, so it works for TSO CALL as well as for a
 * command processor; without a TMP the open fails with ENODEV.
 *
 * Built three ways from this source:
 *   TSTPUT   writes three lines, the second 300 bytes long ("X...XEND",
 *            more than one PUTLINE line), through fopen("*PUTLINE")
 *   TSTPUTP  -DPREMAIN: __premain() points stdout at "*PUTLINE", and
 *            main() writes with printf()
 *   TSTPUTR  TSTPUT against the installed library, the red control
 * With PARM 'NOTMP' (plain batch, no TMP) TSTPUT expects the open to fail
 * with ENODEV.
 *
 * What lands in SYSTSPRT is the measurement: jcl/tstputl.jcl CALLs the
 * modules and runs TSTPUT as a command, and the job's SYSTSPRT shows
 * each line between the TMP's READY prompts.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstputl.c -o TSTPUT -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk -DPREMAIN \
 *                test/mvs/tstputl.c -o TSTPUTP -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstputl.c -o TSTPUTR -flinker-output=iebcopy
 *          ld370 --pack TSTPUT=TSTPUT.iebcopy TSTPUTP=TSTPUTP.iebcopy \
 *                TSTPUTR=TSTPUTR.iebcopy -o tstputl -xmit \
 *                --dsn IBMUSER.LIBC370.PUTSCR
 * Install: jcl/recvput.jcl.   Run: jcl/tstputl.jcl.
 *
 * mvsdev JOB01607, 2026-10-07 (RECEIVE JOB01606): SYSTSPRT holds TSTPUT's
 * three lines after the CALL and again after the command, the 300-byte
 * line whole (297 X and END, wrapped by TSO at 132/120), and TSTPUTP's
 * printf() line; TSTPUTR's went to a SYSOUT of their own.  NOTMP: NULL,
 * errno 19 (ENODEV), RC 0; RED: non-NULL, RC 1.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mvs/crt.h>

#ifdef PREMAIN
static int premain_ok;

int __premain(char *parm, char *pgmname, void **pgmr1)
{
    FILE *fp = fopen("*PUTLINE", "w");

    (void)parm;
    (void)pgmname;
    (void)pgmr1;
    if (fp) {
        stdout = fp;
        premain_ok = 1;
    }
    return 0;
}

int main(void)
{
    printf("TSTPUTP: printf through stdout = *PUTLINE (premain %d)\n",
           premain_ok);
    return premain_ok ? 0 : 1;
}

#else

int main(int argc, char **argv)
{
    FILE    *fp;
    char    line[301];
    int     notmp = argc > 1 && strcmp(argv[1], "NOTMP") == 0;
    int     failed = 0;

    errno = 0;
    fp = fopen("*PUTLINE", "w");
    if (notmp) {
        printf("TSTPUT NOTMP: fopen %s, errno %d\n", fp ? "non-NULL" : "NULL",
               errno);
        if (fp) fclose(fp);
        return (!fp && errno == ENODEV) ? 0 : 1;
    }

    printf("TSTPUT: fopen(\"*PUTLINE\") %s, errno %d\n",
           fp ? "non-NULL" : "NULL", errno);
    if (!fp) return 1;

    memset(line, 'X', 297);
    strcpy(line + 297, "END");
    if (fputs("TSTPUT line 1 of 3\n", fp) < 0) failed++;
    if (fprintf(fp, "%s\n", line) != 301) failed++;
    if (fputs("TSTPUT line 3 of 3\n", fp) < 0) failed++;
    if (fclose(fp) != 0) failed++;
    printf("TSTPUT: %d write error(s)\n", failed);
    return failed ? 1 : 0;
}
#endif
