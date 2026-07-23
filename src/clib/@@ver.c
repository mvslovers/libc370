/* @@VER.C - libc370 build stamp (version + source commit)
**
** Exposes the exact libc370 that is linked into a module so a consumer can log
** it at startup and a deploy/relink mismatch cannot hide.  VERSION and
** LIBC370_REV are injected by sdk/mklibc.py at build time (VERSION from the
** VERSION file, LIBC370_REV from `git rev-parse --short HEAD`).  This TU is
** compiled fresh on every build so the commit tracks HEAD -- do not rely on
** the .s mtime cache here.
*/
#include "clibver.h"

#ifndef VERSION
#define VERSION "0.0.0-dev"
#endif
#ifndef LIBC370_REV
#define LIBC370_REV "unknown"
#endif

static const char stamp[] = "libc370 v" VERSION " (" LIBC370_REV ")";

__asm__("\n&FUNC    SETC 'libc370_version'");
const char *
libc370_version(void)
{
    return stamp;
}
