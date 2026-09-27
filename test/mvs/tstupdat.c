/*
 * tstupdat.c - libc370 #189: does the inherited UPDAT path work at all?
 *
 * THIS PROBE DECIDES NOTHING.  r+ in place (#189, slice 2) would rest on a
 * chain nobody has ever executed in libc370:
 *
 *   @@aopen   mode 2 = OPEN UPDAT on sequential DASD      (@@aopen.asm:153)
 *   @@aread   remembers the record it returned (KEPTREC)
 *   @@awrite  in update mode replaces exactly that record
 *             in the buffer                                (@@awrite.asm:33)
 *   @@atrout  writes the changed block back on the next
 *             read or at CLOSE                             (@@atrout.asm:46)
 *
 * It drives __aopen()/__aread()/__awrite()/__aclose() directly - no stdio -
 * against DD:UPD, FB 80, pre-loaded by the JCL with "L1".."L5" (BLKSIZE 800,
 * so all five share one block):
 *
 *   CASE A  read records 1..3, __awrite() "DONE 3" over record 3, read
 *           record 4 (the rewrite should happen on this read), close.
 *   CASE B  (fresh open) read records 1..2, __awrite() "DONE 2", close
 *           right away - the rewrite must happen at CLOSE.
 *
 * Then the data set is read back through stdio.
 *   expected A: L1 L2 DONE 3 L4 L5
 *   expected B: L1 DONE 2 DONE 3 L4 L5
 *
 * RC: 0 = measured, 8 = an open failed.  Every step also goes to the
 * console via wtof(), because an abend in the chain loses SYSOUT.
 *
 * BUILD (host):
 *
 *     make build
 *     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstupdat.c \
 *           -o TSTUPDAT -flinker-output=iebcopy
 *     ld370 --pack TSTUPDAT=TSTUPDAT.iebcopy -o probe -xmit \
 *           --dsn IBMUSER.LIBC370.T189SCR
 *
 * upload probe.xmit to IBMUSER.LIBC370.T189XMIT, run jcl/recvappn.jcl, then
 * jcl/tstupdat.jcl.
 *
 * MEASURED on mvsdev 2026-09-27, libc370 main at 00a7978, JOB00542, CC 0000:
 *
 *   A: [L1,L2,DONE 3,L4,L5]        the replace reached the disk
 *   B: [L1,DONE 2,DONE 3,L4,L5]    ...and CLOSE writes it back on its own
 *
 * BUT in A the read after the write answered rc=-1 (end of file) instead
 * of returning L4: the replace works, reading on after it does not.  r+ in
 * place has to fix that in @@aread/@@atrout or reposition after every
 * rewrite.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <clibwto.h>
#include <mvssupa.h>

static int bad = 0;

static void *
openupd(void)
{
    int     mode    = 2;            /* UPDAT                              */
    int     recfm   = 0;
    int     lrecl   = 80;
    int     blksize = 800;
    void    *asmbuf = 0;
    void    *dcb;

    dcb = __aopen("UPD     ", &mode, &recfm, &lrecl, &blksize, &asmbuf, 0);
    wtof("TSTUPDAT OPEN UPDAT rc/dcb=%d mode=%d lrecl=%d", (int)dcb, mode, lrecl);
    if ((int)dcb <= 0) bad = 1;
    return dcb;
}

static int
readrec(void *dcb, int n)
{
    unsigned char   *p = 0;
    size_t          len = 0;
    int             rc = __aread(dcb, &p, &len);

    wtof("TSTUPDAT READ %d rc=%d len=%d [%.6s]", n, rc, (int)len,
         rc == 0 ? (char *)p : "");
    return rc;
}

static int
writerec(void *dcb, const char *s)
{
    unsigned char   rec[80];
    unsigned char   *p = rec;
    size_t          len = sizeof(rec);
    int             rc;

    memset(rec, ' ', sizeof(rec));
    memcpy(rec, s, strlen(s));
    rc = __awrite(dcb, &p, &len);
    wtof("TSTUPDAT WRITE [%s] rc=%d", s, rc);
    return rc;
}

static void
show(const char *label)
{
    FILE    *fp = fopen("DD:UPD", "r");
    char    line[100];
    char    seen[120];
    size_t  n;

    seen[0] = 0;
    if (!fp) {
        wtof("TSTUPDAT %s READBACK OPEN FAILED errno=%d", label, errno);
        bad = 1;
        return;
    }
    while (fgets(line, sizeof(line), fp)) {
        n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == ' ')) line[--n] = 0;
        if (strlen(seen) + n + 2 < sizeof(seen)) {
            if (seen[0]) strcat(seen, ",");
            strcat(seen, line);
        }
    }
    fclose(fp);
    printf("%s: %s\n", label, seen);
    wtof("TSTUPDAT %s [%s]", label, seen);
}

int
main(int argc, char **argv)
{
    void    *dcb;

    /* A: replace record 3, the rewrite is driven by the next read */
    dcb = openupd();
    if ((int)dcb > 0) {
        readrec(dcb, 1);
        readrec(dcb, 2);
        readrec(dcb, 3);
        writerec(dcb, "DONE 3");
        readrec(dcb, 4);
        __aclose(dcb);
        wtof("TSTUPDAT A CLOSED");
    }
    show("A");

    /* B: replace record 2, then CLOSE at once */
    dcb = openupd();
    if ((int)dcb > 0) {
        readrec(dcb, 1);
        readrec(dcb, 2);
        writerec(dcb, "DONE 2");
        __aclose(dcb);
        wtof("TSTUPDAT B CLOSED");
    }
    show("B");

    printf("TSTUPDAT RC=%d\n", bad ? 8 : 0);
    return bad ? 8 : 0;
}
