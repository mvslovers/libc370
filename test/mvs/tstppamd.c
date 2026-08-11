/*
 * tstppamd.c - libc370 #93 middle module for the tstppafr probe (T3).
 *
 * LINKed by TSTPPAFR under __linkds(); LINKs TSTPPAIN itself via
 * __link() - deliberately NOT ESTAE-protected, so on the abend leg
 * (ambient subpool 7, inherited by TSTPPAIN through the PPA chain)
 * the inner S0C1 percolates straight to the OUTER module's try().
 * Neither this module's @@EXITA nor TSTPPAIN's ever runs: TWO
 * stack+PPA blocks are abandoned at 8(TCBFSAB), chained through
 * PPASAVE, and ___try must release the whole chain back to the
 * outer snapshot, not just the innermost frame.
 *
 * On a normal leg (ambient != 7) the inner module returns 0, both
 * @@EXITAs run, and this module passes the result through.
 *
 * wtof() only, no stdio: on the abend leg the trailing SYSPRINT
 * block would be lost anyway (doc/consumer-notes.md).
 *
 * BUILD (host): see test/mvs/tstppafr.c - all three members travel in
 * one ld370 --pack.  RUN: jcl/tstppafr.jcl.
 */
#include <cliblink.h>
#include <clibwto.h>

int main(void)
{
    int     prc = -1;
    int     rc;

    rc = __link("TSTPPAIN", 0, 0, &prc);    /* no ESTAE by design */

    wtof("TSTPPAMD: __link rc=%d prc=%d", rc, prc);
    return rc < 0 ? 8 : rc;
}
