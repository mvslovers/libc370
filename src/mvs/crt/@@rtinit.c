/* @@RTINIT - the runtime part of the C startup (#507).
**
** What @@START does before it parses the parameter and calls main(), and
** what @@CCALL does when it builds a C environment for a caller that is not
** C (a COBOL main program), where there is no main() and no parameter.
**
** Since #564 it opens and loads nothing.  The standard streams open on first
** use (@@stdlz.c), the SYSENV (or ENVIRON) environment on the first call of
** an environment function (@@envlz.c), and the TZ variable is applied the
** first time a time function needs the offset (__tzget()).  Opening the
** streams before main() was most of the start-up's processor time, and
** loadenv() and tzset() linked fopen() and printf() into every program.
** Here only the flags are set that let them happen; they are set after
** __premain(), so what the hook does is not overridden, and the hook still
** runs before any of it.
**
** Returns 0.
*/
#include "mvs/crt.h"

int
__rtinit(void)
{
    CLIBGRT *grt = __grtget();

    if (grt) {
        grt->grtflag1 |= GRTFLAG1_STDLAZY;
        grt->grtflag2 |= GRTFLAG2_ENVLAZY;
    }
    return 0;
}
