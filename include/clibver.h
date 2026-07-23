#ifndef CLIBVER_H
#define CLIBVER_H

/* libc370_version() - the exact libc370 linked into this module.
**
** Returns a stable, statically-allocated string of the form
**     "libc370 v1.0.0 (6b676dc)"
** (version from the VERSION file, commit from `git rev-parse --short HEAD`,
** with "-dirty" appended when built from a modified tree).  Consumers should
** log it once at startup so the runtime actually linked in is recorded on the
** console -- a deploy/relink mismatch (sysroot says X, STC runs Y) then cannot
** hide.  The string is baked in at build time; see sdk/mklibc.py.
*/
extern const char *libc370_version(void);

#endif
