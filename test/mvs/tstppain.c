/*
 * tstppain.c - libc370 #93 inner module for the tstppafr probe.
 *
 * LINKed by TSTPPAFR directly (T1) or through TSTPPAMD (T3), from the
 * same STEPLIB.  Behaviour is keyed on the ambient heap subpool it
 * inherits through @@CRT0's validated PPA chain (#89):
 *
 *   __getsp() == 7   abend S0C1 under the outer module's try() - the
 *                    caught-abend path whose stack reclaim is under
 *                    test.  Deliberately allocates NOTHING: the only
 *                    storage this run costs is the @@CRT0 stack+PPA
 *                    block, so the probe measures exactly the #93
 *                    leak and nothing else.
 *   anything else    return 0 normally (T5: @@EXITA frees the block
 *                    and pops the chain; ___try must not free again).
 *
 * wtof() only, no stdio: on the abend leg the trailing SYSPRINT block
 * would be lost anyway (doc/consumer-notes.md).
 *
 * BUILD (host): see test/mvs/tstppafr.c - all three members travel in
 * one ld370 --pack.  RUN: jcl/tstppafr.jcl.
 */
#include <clibos.h>
#include <clibwto.h>

int main(void)
{
    unsigned char   sp = __getsp();

    wtof("TSTPPAIN: ambient sp=%u%s", (unsigned)sp,
         sp == 7 ? ", abending S0C1" : ", returning 0");

    if (sp == 7) {
        __asm__("DC\tH'0'");        /* S0C1 under the outer try() */
    }

    return 0;
}
