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
** One thing it does create: the empty array of open files, grt->grtfile
** (#693).  Created by the first fopen() instead, it took the PSW key of
** that call, and a program whose first fopen() came inside a key-0 window
** (__super(PSWKEY0, ...), __loadhi()) ended S0C4 at its next fopen() in
** its own key, storing into a key-0 array (2.15.0-2.24.0).  Up to 2.14.0
** the start-up's own fopen() of stdout created it, in the program's key.
** It costs arraynew() and one calloc(); calloc() is linked anyway.
**
** Returns 0.
*/
#include "mvs/crt.h"
#include "ext/array.h"

int
__rtinit(void)
{
    CLIBGRT *grt = __grtget();

    if (grt) {
        grt->grtflag1 |= GRTFLAG1_STDLAZY;
        grt->grtflag2 |= GRTFLAG2_ENVLAZY;
        /* __premain() may have opened a file already */
        if (!grt->grtfile) grt->grtfile = arraynew(0);
    }
    return 0;
}
