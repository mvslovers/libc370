/*
 * tstplen.c - libc370 httpd#275 on MVS: a parameter whose length halfword
 * is 1..3 with a zero third byte is not taken for a TSO command buffer.
 *
 * __start() treated any parameter with a nonzero length and a zero third
 * byte as TSO-shaped and then copied length - 4 bytes from behind the
 * 4-byte prefix.  A length of 1..3 made that copy length negative, and
 * memcpy() took it as a huge size.  Now the TSO shape needs a length of at
 * least 4.
 *
 * Built as two programs from this source: the driver (TSTPLEN, default)
 * LINKs a target through __linkds() with R1 -> { A(X'0002',X'00','X') }
 * and reports what came back; the target (-DTARGET) prints its argc and
 * returns 7 when argv[0] is its program name (3 otherwise).  TSTPLNT is the target against this tree's libc.a, TSTPLNR
 * against the installed one (2.4.0, before the fix) - the red control.
 * The old code's copy did not abend only by luck: memcpy() is an MVCL,
 * and with a length near 16 MB source and target overlap, so MVCL sets
 * CC 3 and moves nothing.  What shows is argv[0], taken from the TSO path.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstplen.c -o TSTPLEN -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk -DTARGET \
 *                test/mvs/tstplen.c -o TSTPLNT -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -DTARGET \
 *                test/mvs/tstplen.c -o TSTPLNR -flinker-output=iebcopy
 *          ld370 --pack TSTPLEN=TSTPLEN.iebcopy TSTPLNT=TSTPLNT.iebcopy \
 *                TSTPLNR=TSTPLNR.iebcopy -o tstplen -xmit \
 *                --dsn IBMUSER.LIBC370.PLNSCR
 * Install: jcl/recvpln.jcl.   Run: jcl/tstplen.jcl (PARM= names the target).
 *
 * mvsdev JOB01408, 2026-10-05 (RECEIVE JOB01407): GREEN CC 0, argv[0]
 * TSTPLNT; RED (installed 2.4.0) CC 1, argv[0] empty.
 *
 * RC: 0 = the target ran and returned 7, 1 = it did not.
 */
#include <stdio.h>

#include <string.h>

#ifdef TARGET
int main(int argc, char **argv)
{
    printf("target: argc=%d argv[0]=%s\n", argc, argv[0] ? argv[0] : "(null)");
    /* parsed as a PARM, argv[0] is the program name; taken for a TSO
       buffer, it was the empty start of the copied text */
    return (argc == 1 && argv[0] && strncmp(argv[0], "TSTPLN", 6) == 0)
           ? 7 : 3;
}
#else
#include <mvs/link.h>

int main(int argc, char **argv)
{
    static unsigned char parm[4] = { 0x00, 0x02, 0x00, 'X' };
    void *plist[1];
    int  rc = -99;
    int  lrc;
    const char *target = argc > 1 ? argv[1] : "TSTPLNT";

    plist[0] = (void *)((unsigned)parm | 0x80000000);
    lrc = __linkds(target, NULL, plist, &rc);
    printf("driver: __linkds(%s) = %d, program rc = %d\n", target, lrc, rc);
    printf("%s\n", rc == 7 ? "PASS" : "FAIL");
    return rc == 7 ? 0 : 1;
}
#endif
