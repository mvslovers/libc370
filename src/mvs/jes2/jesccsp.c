/* JESCCSP.C - the paper movement of one SYSOUT record (#760).
 *
 * A file of its own so that only a program calling it links it: every user of
 * jesprint() links jesprb.c.  Includes no libc370 header, for the same reason
 * as jesprb.c: test/host/tstjesprb.c runs it on the host.
 */
#include "src/mvs/jes2/jesprb.h"

/* The paper movement of one record, see mvs/jes2.h.
 *
 * The ASA codes are compared as characters, so this is right in EBCDIC on the
 * target and in ASCII under the host test.  A machine code is a bit pattern in
 * either: the low three bits say write (001) or immediate (011), the high five
 * what the paper does: 00000 nothing, 00001 to 00011 space 1 to 3, 10001 to
 * 11100 skip to channel 1 to 12.                                             */
void
jesccsp(unsigned cc, int *before, int *after)
{
    int     c   = (int)JESCC_BYTE(cc);
    int     n   = 1;

    if ((cc & JESCC_HAS) && (cc & JESCC_ASA)) {
        *after = 0;
        if (c == ' ')                   n = 1;
        else if (c == '0')              n = 2;
        else if (c == '-')              n = 3;
        else if (c == '+')              n = 0;
        else if (c >= '1' && c <= '9')  n = -(c - '0');
        else if (c == 'A')              n = -10;
        else if (c == 'B')              n = -11;
        else if (c == 'C')              n = -12;
        *before = n;
        return;
    }

    *before = 0;
    *after  = 1;

    if ((cc & JESCC_HAS) && (cc & JESCC_MACH)
        && ((c & 7) == 1 || (c & 7) == 3)) {
        int op = c >> 3;

        if (op <= 3)                        n = op;
        else if (op >= 0x11 && op <= 0x1C)  n = -(op - 0x10);
        else return;                        /* no carriage code */

        if ((c & 7) == 1) {
            *after  = n;
        } else {
            *before = n;
            *after  = 0;
        }
    }
}
