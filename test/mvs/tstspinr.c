/*
 * tstspinr.c - libc370 #89 inner module for the tstsplnk probe.
 *
 * LINKed by TSTSPLNK (test/mvs/tstsplnk.c) under __linkds().  It has
 * no parm: its behaviour is keyed on the ambient heap subpool it
 * INHERITS through @@CRT0's PPAHEAPS validation - which is itself the
 * mechanism under test.
 *
 *   __getsp() == 5   T3 leg: malloc a chunk (lands in subpool 5, the
 *                    outer module reclaims it with FREEMAIN SP=5) and
 *                    return the ambient value as the program rc, so
 *                    the outer module can verify the inheritance.
 *   __getsp() == 6   T4 leg: malloc a chunk, then abend deliberately
 *                    (S0C1) under the outer module's __linkds ESTAE -
 *                    the httpd#154 crashing-CGI scenario in miniature.
 *   anything else    report and return the value (a T6-style direct
 *                    run from JCL takes this path: ambient 0, rc 0).
 *
 * wtof() only, no stdio: on the abend leg the trailing SYSPRINT block
 * would be lost anyway (doc/consumer-notes.md).
 *
 * BUILD (host): see test/mvs/tstsplnk.c - both members travel in one
 * ld370 --pack.  RUN: jcl/tstsplnk.jcl.
 */
#include <stdlib.h>
#include <string.h>
#include <clibos.h>
#include <clibwto.h>

#define CHUNK   (1024 * 1024)

int main(void)
{
    unsigned char   sp = __getsp();
    void            *p;

    p = malloc(CHUNK);              /* lands in the ambient subpool */
    if (p) memset(p, 0xEE, 64);
    wtof("TSTSPINR: ambient sp=%u, malloc(1M)=%s",
         (unsigned)sp, p ? "ok" : "NULL");

    if (sp == 6) {
        /* T4: die the way a broken CGI dies, holding storage */
        __asm__("DC\tH'0'");        /* S0C1 under __linkds's ESTAE */
    }

    return (int)sp;                 /* T3: report what was inherited */
}
