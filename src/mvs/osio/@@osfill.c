/* @@OSFILL.C - the DD name and the exit list of a new DCB, for osbdcb()
** and osxdcb(), which shared this code as two copies (#412)
*/
#include <stdlib.h>
#include <ctype.h>
#include "mvs/osio.h"

extern DCB *__osfill(DCB *dcb, const char *ddname, EXITLIST *exitlist)
    asm("@@OSFILL");

/* returns dcb, or NULL - with dcb freed - when the exit list cannot be
   allocated: no DCB rather than one without the exits asked for, which
   came back without dcbexlst and no word of it (#412) */
DCB *
__osfill(DCB *dcb, const char *ddname, EXITLIST *exitlist)
{
    unsigned    *list;
    int         i;

    if (ddname) {
        if (ddname[2]==':') ddname += 3;
        for (i=0; i < 8 && ddname[i]; i++) {
            dcb->dcbddnam[i] = toupper(ddname[i]);
        }
    }

    if (!exitlist) return dcb;

    list = calloc(1, sizeof(unsigned)*10);  /* up to 10 exits */
    if (!list) {
        free(dcb);
        return NULL;
    }

    dcb->dcbpriv  = (void*)list;    /* save address for cleanup */
    dcb->dcbexlst = (void*)list;
    for(i=0; i < 10; i++, exitlist++) {
        list[i] = exitlist->exit_code << 24 |
            ((unsigned)exitlist->exit_addr & 0x00FFFFFF);
        if (exitlist->exit_code & 0x80) break;
    }
    if (i==10) i--;
    list[i] |= 0x80000000;  /* tag end of exit list */

    return dcb;
}
