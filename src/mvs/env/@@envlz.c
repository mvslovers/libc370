/* @@ENVLZ.C - the SYSENV environment, loaded on first use (#564)
**
** The start-up loaded DD SYSENV (or ENVIRON) before main(), which linked
** fopen() and the stream layer into every program, environment or not.
** Now __rtinit() sets GRTFLAG2_ENVLAZY and the environment functions -
** getenv(), getenvi(), setenv(), putenv(), unsetenv() - call __envlz()
** first; it loads the DD once.
**
** - Before __rtinit() (in __premain) nothing is loaded, as before.
** - ENVTRY is set before the DD is read, so the putenv() calls
**   do not load again; ENVDONE when it has finished.
** - The DD is loaded in the task's PSW key (#693).  Loaded inside a
**   caller's key-0 window, the variables and their array were key-0
**   storage: a setenv() in the program's own key ended S0C4 storing a
**   new one and S30A freeing an old one (2.16.0-2.24.0).
** - A lock of its own, not &grt->grtenv: setenv() takes that one and
**   releases it unconditionally, which would end ours too.  A second task
**   that comes while the first loads waits for it and then finds the
**   environment complete.
*/
#include <mvs/env.h>
#include "mvs/crt.h"
#include "mvs/lock.h"
#include "src/internal/envld.h"
#include "src/internal/tskey.h"

void
__envlz(void)
{
    CLIBGRT *grt    = __grtget();
    int     owned;
    unsigned st;

    if (!grt || !(grt->grtflag2 & GRTFLAG2_ENVLAZY)) return;
    if (grt->grtflag2 & GRTFLAG2_ENVDONE) return;

    owned = (lock(&grt->grtflag2, 0) == 0);   /* 8: this task holds it */
    if (!(grt->grtflag2 & GRTFLAG2_ENVTRY)) {
        grt->grtflag2 |= GRTFLAG2_ENVTRY;
        st = tskey();               /* the variables in the task's key */
        if (__envdd("SYSENV  ")) {   /* without fopen() (F3) */
            __envdd("ENVIRON ");
        }
        tsrestore(st);
        grt->grtflag2 |= GRTFLAG2_ENVDONE;
    }
    if (owned) unlock(&grt->grtflag2, 0);
}
