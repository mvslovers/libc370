/* @@75RECV.C
** Receive data from a socket
*/
#include "__75.h"
#include "socket.h"
#include "errno.h"

/* recv() */
__asm__("\n&FUNC    SETC 'recv'");
extern int
recv(int ss, void *vbuf, int len, int flags)
{
    int     rc  = 0;    /* len <= 0 never enters the loop below */
    int     chunk;
    char    *buf;
    int     read;
    PL75    pl;

    /* sanity check the parms */
    if (ss < 0 || !vbuf) {
        rc = -1;
        errno = EPERM;
        goto quit;
    }
 
    for(read = 0; read < len; read += rc) {
        buf = ((char *)vbuf) + read;

        /* Cap every X'75' at 256 bytes -- the segment size the instruction
         * copies in, and the only bound that is safe (#154).
         *
         * X'75' is restartable: a page translation exception on the guest
         * buffer is nullifying, so MVS resolves the page and the instruction
         * runs again from the top.  The guest side resumes correctly -- R1
         * holds the bytes remaining and the base register was advanced
         * before the exception.  The host side has nothing to resume from:
         * upstream x75.c recomputes its pointer from map32[R2] on every
         * entry, and R2 is a slot index that never advances.  The remaining
         * bytes are therefore copied from the START of the host buffer to
         * the already advanced guest address, and the tail of the read is a
         * replay of its head.
         *
         * A single segment is atomic against that exception -- vstorec()
         * resolves both page addresses through MADDRL before either memcpy
         * -- so a copy of 256 bytes or less either faults having moved
         * nothing, where resuming from the start is correct, or it
         * completes.  The defect needs at least one COMPLETED segment before
         * the fault, which is why every larger cap looked like a fix and
         * then failed: 4096 here since cd43a70, then 2048 in mvsMF, which
         * failed five days later.
         *
         * The comment this replaces named a dyn75/Hercules buffer-size limit
         * that does not exist.  The symptom it recorded -- "the buf is
         * overwritten from the start" -- was real, and is the replay above.
         *
         * This does not wait on the emulator, and it does not become
         * removable once the emulator is fixed: no return value, status bit
         * or function code lets a guest tell a patched host from an
         * unpatched one, and the same build has to keep working on both.
         */
        chunk = len - read;
        if (chunk > 256) chunk = 256;
        
        __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list"
            : : "r" (&pl));
        pl.r6   = (unsigned) buf;
        pl.r7   = (unsigned) 11;    /* function code for recv() */
        pl.r8   = (unsigned) ss;    /* socket number            */
        pl.r9   = (unsigned) chunk; /* from 1 to 256 bytes      */

        __75(&pl);

        rc = (int) pl.r4;
        if (rc==-2) {
            /* special case, we have to wait for some reason??? */
            __asm__("STIMER WAIT,BINTVL==F'8'   0.08 seconds" : : : "0", "1");
            rc = 0;
            continue;
        }

        if (rc <= 0) break;
    }
    
    if (rc==-1) {
        /* set the errno value for this error */
        pl.r1   = (unsigned) 0;
        pl.r7   = (unsigned) 2;     /* function code for get error */
        __75(&pl);
        errno = (int) pl.r4;
    }

    /* if we read some data, return the length we read */
    if (read) rc = read;

quit:
    return rc;
}
