/*
 * tstfrecl.c - libc370 #559: fread() on a record stream returns the number
 *              of elements read, as ISO C has it.
 *
 * Writes records of known lengths to three temporary data sets (DDs from
 * jcl/tstfrecl.jcl, attributes from the mode, #554), then reads them back:
 *
 *   (a) RECFM=U, records of 30 and 100 bytes, fread(buf, 1, 32760):
 *       30 and 100 - before #559 32760 both times
 *   (b) RECFM=VB, a record of 30 data bytes (34 with its RDW), size 1: 34
 *   (c) RECFM=FB 80, size 1: 80; size 80, nmemb 1: 1
 *   (d) a record longer than the buffer, FB 80 into 50 bytes: 50
 *   (e) size 0: 0
 *   (f) fseek(SEEK_END) on the U stream still reaches the end: fread 0, EOF
 *
 * RC 0 = every check passed, 8 = a check failed.  Built twice: TSTFRECL
 * against this tree's libc.a (green), TSTFRECR against the installed one.
 */
#include <stdio.h>
#include <string.h>
#include <mvs/wto.h>

static int  bad = 0;
static char buf[32760];

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTFRECL %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static void put(FILE *fp, size_t len)
{
    memset(buf, 'D', len);
    fwrite(buf, 1, len, fp);
}

int main(void)
{
    FILE    *fp;
    size_t  n1, n2;

    printf("=== tstfrecl: #559 - fread() count on a record stream ===\n");

    /* write */
    fp = fopen("DD:RECU", "wb,record,recfm=u,blksize=200");
    if (!fp) { check(0, "open DD:RECU for output"); return 8; }
    put(fp, 30);
    put(fp, 100);
    fclose(fp);

    fp = fopen("DD:RECV", "wb,record,recfm=vb,lrecl=104,blksize=1000");
    if (!fp) { check(0, "open DD:RECV for output"); return 8; }
    memset(buf, 'D', 34);
    buf[0] = 0; buf[1] = 34; buf[2] = 0; buf[3] = 0;    /* RDW: 34 */
    fwrite(buf, 1, 34, fp);
    fclose(fp);

    fp = fopen("DD:RECF", "wb,record,recfm=fb,lrecl=80,blksize=800");
    if (!fp) { check(0, "open DD:RECF for output"); return 8; }
    put(fp, 80); put(fp, 80); put(fp, 80); put(fp, 80);
    fclose(fp);

    /* (a) U */
    fp = fopen("DD:RECU", "rb,record");
    n1 = fread(buf, 1, sizeof(buf), fp);
    n2 = fread(buf, 1, sizeof(buf), fp);
    printf("      U: %u %u\n", (unsigned)n1, (unsigned)n2);
    wtof("TSTFRECL U %u %u", (unsigned)n1, (unsigned)n2);
    check(n1 == 30 && n2 == 100, "(a) RECFM=U, size 1: 30 and 100");
    /* (f) */
    fseek(fp, 0, SEEK_END);
    n1 = fread(buf, 1, sizeof(buf), fp);
    check(n1 == 0 && feof(fp), "(f) fseek(SEEK_END) reaches the end");
    fclose(fp);

    /* (b) V */
    fp = fopen("DD:RECV", "rb,record");
    n1 = fread(buf, 1, sizeof(buf), fp);
    printf("      V: %u (RDW %u)\n", (unsigned)n1,
           (unsigned)((unsigned char)buf[0] << 8 | (unsigned char)buf[1]));
    wtof("TSTFRECL V %u", (unsigned)n1);
    check(n1 == 34, "(b) RECFM=VB, size 1: 34 with the RDW");
    fclose(fp);

    /* (c) (d) (e) F */
    fp = fopen("DD:RECF", "rb,record");
    n1 = fread(buf, 1, sizeof(buf), fp);
    n2 = fread(buf, 80, 1, fp);
    printf("      F: size 1 %u, size 80 %u\n", (unsigned)n1, (unsigned)n2);
    wtof("TSTFRECL F %u %u", (unsigned)n1, (unsigned)n2);
    check(n1 == 80, "(c) FB 80, size 1: 80");
    check(n2 == 1,  "(c) FB 80, size 80 nmemb 1: 1");
    n1 = fread(buf, 1, 50, fp);
    check(n1 == 50, "(d) FB 80 into 50 bytes: 50");
    n1 = fread(buf, 0, 10, fp);
    check(n1 == 0,  "(e) size 0: 0");
    fclose(fp);

    printf("=== tstfrecl: %d check(s) failed ===\n", bad);
    wtof("TSTFRECL VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
