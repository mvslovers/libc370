#include <mvs/subsys.h>
#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/wto.h>
#include <mvs/apf.h>

__asm__("\n&FUNC    SETC 'ssct_remove'");
int ssct_remove(SSCT *ssct)
{
    SSCT            *inst;
    int             rc          = 4;                    /* assume not found         */
    unsigned char   prevkey;

    /* NULL is not installed.  It matched the last SSCT, whose next is
       NULL, and the unlink below then stored 0 at absolute address 4, in
       key 0 (#407) */
    if (!ssct) return rc;

    /* the walk starts at the first subsystem, which is the system's own
       (MSTR) and is nobody's successor: it is never removed, and
       ssct_install() never puts an SSCT in front of it */
    inst = ssct_find(0);

    /* save current PSW key */
    __asm__("IPK\t0             get psw key in R2\n\t"
            "LR\t%0,2           save psw key in register" : "=r"(prevkey) : : "2", "memory");

    if (prevkey != PSWKEY0) {
        /* set PSW key 0 */
        __asm__("SLR\t2,2               PSW key 0 value\n\t"
                "SPKA\t0(2)             save in psw" : : : "2", "memory");
    }

    /* search installed subsystems */
    while(inst) {
        /* is this subsystem 'next' value our subsystem pointer? */
        if (inst->ssctscta == ssct) {
            /* yes, remove ssct from installed next */
            inst->ssctscta = ssct->ssctscta;    /* installed->next = our subsystem->next */
            ssct->ssctscta = (SSCT*)0;          /* our subsystem->next = NULL            */
            rc = 0;                             /* indicate success                      */
            break;
        }
        inst = inst->ssctscta;
    }

    if (prevkey != PSWKEY0) {
        /* reset PSW key to callers key */
        __asm__("LR\t2,%0               get prev psw key\n\t"
                "SPKA\t0(2)             save in psw" : : "r"(prevkey) : "2", "memory");
    }

    return rc;
}
