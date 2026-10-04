#include <stddef.h>
#include <string.h>              /* memmove()                     */
#include <strings.h>             /* strcasecmp()                  */
#include <mvs/wto.h>             /* wtof()                        */
#include <mvs/link.h>            /* __link()                      */

int run_step(const char *pgm, const char *opt,
             char *save, const char *rec, size_t len)
{
    memmove(save, rec, len);     /* was bcopy(rec, save, len)     */
    if (strcasecmp(opt, "TRACE") == 0)
        wtof("RUNSTEP: calling %s", pgm);
    return __link(pgm, NULL, NULL, NULL);
}
