#include <mvs/wto.h>
#include <mvs/crt.h>
#include <mvs/recovery.h>
#include "src/internal/wtofmt.h"

static void try_traceback(SAVEAREA *sa);

__asm__("\n&FUNC    SETC 'wto_traceback'");
void wto_traceback(SAVEAREA *sa)
{
    try(try_traceback, sa);
}

__asm__("\n&FUNC    SETC 'try_traceback'");
static void try_traceback(SAVEAREA *sa)
{
    CLIBCRT     *crt    = __crtget();
    SAVEAREA    *prev;
    SAVEAREA    *next;
    unsigned    offset;
    unsigned    retto;
    unsigned    frame;
    int         i;
    void        *thisep;
    void        *prevep;
    char        thisname[256];
    char        prevname[256];

    if (!sa) {
        /* caller didn't provide a save area to start at */
        sa = sa_prev(0);    /* this save area has "try_traceback" ep in R15 */
#if 1
        sa = sa_prev(sa);   /* this save area has "call" ep in R15 */
        sa = sa_prev(sa);   /* this save area has "@@@TRY" ep in R15 */
        sa = sa_prev(sa);   /* this save area has "wto_traceback" ep in R15 */
#endif
    }

    /* without the printf family (#564): malloc() reports a shortage
       through here, so every program linked printf() for it */
    {
        char    title[] = "Save area trace back";

        wto(title);
    }

    for(i=0, prev = (SAVEAREA*) sa->prev;i < 256 && sa != (SAVEAREA*)crt->crtsave; i++, sa = prev, prev = (SAVEAREA*) sa->prev) {
        next = sa_next(sa);
        prevep = (void*) prev->r15;
        sa_get_epname(prev, prevname);
        thisep = (void*) sa->r15;
        sa_get_epname(sa, thisname);
        frame = (unsigned)next->nab - (unsigned)next;

        retto = (sa->r14 & 0x0FFFFFFF); /* strip out BALR bits in R14 */
        offset = retto - prev->r15;
        {
            char    msg[126];
            int     k = 0;

            wtofmt_str(msg, &k, sizeof(msg), "  \"");
            wtofmt_str(msg, &k, sizeof(msg), thisname);
            wtofmt_str(msg, &k, sizeof(msg), "\" ep=");
            wtofmt_hex(msg, &k, sizeof(msg), (unsigned)thisep, 6);
            wtofmt_str(msg, &k, sizeof(msg), " frame=");
            wtofmt_dec(msg, &k, sizeof(msg), frame);
            wtofmt_str(msg, &k, sizeof(msg), " bytes");
            wto(msg);
            k = 0;
            wtofmt_str(msg, &k, sizeof(msg), "    returns to=");
            wtofmt_hex(msg, &k, sizeof(msg), retto, 6);
            wtofmt_str(msg, &k, sizeof(msg), " \"");
            wtofmt_str(msg, &k, sizeof(msg), prevname);
            wtofmt_str(msg, &k, sizeof(msg), "\" ep=");
            wtofmt_hex(msg, &k, sizeof(msg), (unsigned)prevep, 6);
            wtofmt_str(msg, &k, sizeof(msg), "+");
            /* %X: no leading zeros */
            {
                int d = 1;
                while (d < 8 && (offset >> (d * 4))) d++;
                wtofmt_hex(msg, &k, sizeof(msg), offset, d);
            }
            wto(msg);
        }
    }
}

#if 0
int main(int argc, char **argv)
{
    wto_traceback(0);

    return 0;
}
#endif
