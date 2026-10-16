/* @@RTINIT - the runtime part of the C startup (#507).
**
** What @@START does before it parses the parameter and calls main(): load
** the environment variables from DD SYSENV (or ENVIRON), set the time zone,
** and let the standard streams open on first use.  @@START calls it at
** every program start; @@CCALL calls it when it builds a C environment for
** a caller that is not C (a COBOL main program), where there is no main()
** and no parameter to parse.
**
** The standard streams are no longer opened here (#564).  Opening stdout,
** stderr and stdin before main() cost an OPEN and a CLOSE each, and for
** stderr without a SYSTERM DD and stdin without a SYSIN DD a dynamic
** allocation besides - measured on MVS as most of the start-up's CPU, for
** streams a program may never touch.  GRTFLAG1_STDLAZY lets __gtin(),
** __gtout() and __gterr() open each one the first time it is used
** (@@stdlz.c).  It is set here, after __premain(): a stream the hook
** assigned is kept, and one it set or read before this point is not
** opened behind its back.
**
** Returns 0.
*/
#include <mvs/env.h>
#include <time.h>
#include "mvs/crt.h"

int
__rtinit(void)
{
    CLIBGRT *grt = __grtget();

    if (grt) grt->grtflag1 |= GRTFLAG1_STDLAZY;
    if (loadenv("dd:SYSENV")) {
        loadenv("dd:ENVIRON");
    }
    tzset();
    return 0;
}
