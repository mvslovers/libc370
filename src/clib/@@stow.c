/* @@STOW.C - STOW (SVC 21) wrapper for PDS directory maintenance */
#include <clibos.h>

/*
 * __stow() - update a partitioned data set directory entry.
 *
 * The DCB must be open for OUTPUT or UPDAT against a DSORG=PO data set.
 * The directory action is selected by 'func':
 *
 *   'A' add      - area = 8-byte name + 3-byte TTR + 1-byte count + user data
 *   'R' replace  - area = same layout as add
 *   'D' delete   - area = 8-byte name
 *   'C' change   - area = 8-byte old name + 8-byte new name (16 bytes);
 *                  repoints the directory entry, preserving the member's
 *                  TTR and user data (ISPF statistics)
 *
 * Returns the STOW return code in register 15 (0 on success, non-zero
 * otherwise - see the STOW documentation), or -1 for an unknown function.
 *
 * The SVC 21 linkage is built by hand because as370 has no STOW opcode and
 * there is no stow macro in the macro library: the STOW mnemonic assembled
 * to NOTHING, so every call was a silent no-op that returned the func letter
 * left in R15 (#32).  BLDL, by contrast, as370 does know.
 *
 * The linkage below is transcribed from SYS1.MACLIB(STOW) and its inner
 * macro IHBINNRA on MVS 3.8j, not from memory.  IHBINNRA loads
 *
 *      R1 = DCB address        (LR 1,&A - the first operand)
 *      R0 = area address       (LR 0,&B - the second operand)
 *
 * and the function is encoded by NEGATING those registers - there is no
 * function code byte:
 *
 *      A   R0 = +area   R1 = +dcb      (no negation)
 *      R   R0 = +area   R1 = -dcb      LCR 1,1
 *      D   R0 = -area   R1 = +dcb      LCR 0,0
 *      C   R0 = -area   R1 = -dcb      LCR 1,1 + LCR 0,0
 *
 * followed by SVC 21, with the return code in R15.
 */
int
__stow(void *dcb, void *area, int func)
{
    int rc = -1;

    switch (func) {
    case 'A': case 'a':
        __asm__("LR\tR1,%1\n\t"
                "LR\tR0,%2\n\t"
                "SVC\t21\n\t"
                "LR\t%0,R15"
                : "=r"(rc) : "r"(dcb), "r"(area) : "0", "1", "14", "15");
        break;
    case 'R': case 'r':
        __asm__("LR\tR1,%1\n\t"
                "LR\tR0,%2\n\t"
                "LCR\tR1,R1\n\t"
                "SVC\t21\n\t"
                "LR\t%0,R15"
                : "=r"(rc) : "r"(dcb), "r"(area) : "0", "1", "14", "15");
        break;
    case 'D': case 'd':
        __asm__("LR\tR1,%1\n\t"
                "LR\tR0,%2\n\t"
                "LCR\tR0,R0\n\t"
                "SVC\t21\n\t"
                "LR\t%0,R15"
                : "=r"(rc) : "r"(dcb), "r"(area) : "0", "1", "14", "15");
        break;
    case 'C': case 'c':
        __asm__("LR\tR1,%1\n\t"
                "LR\tR0,%2\n\t"
                "LCR\tR1,R1\n\t"
                "LCR\tR0,R0\n\t"
                "SVC\t21\n\t"
                "LR\t%0,R15"
                : "=r"(rc) : "r"(dcb), "r"(area) : "0", "1", "14", "15");
        break;
    }

    return rc;
}
