/*
 * tstppbig.c - libc370 #96 T7b: TSTPPAIN with a 128K initialized pad,
 *              so the load module is ~128K bigger.  If the per-abend
 *              residue tstcrtlk measures scales with the module size,
 *              the residue IS abandoned module copies in the job pack
 *              area.  Same trigger as TSTPPAIN: ambient heap subpool 7
 *              (inherited through the PPA chain, #89) means abend S0C1,
 *              anything else returns 0.
 *
 * BUILD (host): see test/mvs/tstcrtlk.c - travels in the same
 * ld370 --pack.  RUN: jcl/tstcrtlk.jcl.
 */
#include <clibos.h>
#include <clibwto.h>

/* initialized so it lands in the load module as data, not in BSS */
static char pad[131072] = { 0xEE };

int main(void)
{
    unsigned char   sp = __getsp();

    wtof("TSTPPBIG: ambient sp=%u pad[0]=%u%s", (unsigned)sp,
         (unsigned)(unsigned char)pad[0],
         sp == 7 ? ", abending S0C1" : ", returning 0");

    if (sp == 7) {
        __asm__("DC\tH'0'");        /* S0C1 under the outer try() */
    }

    return 0;
}
