#ifndef STRINGS_H
#define STRINGS_H
/* strings.h - POSIX <strings.h>: case-insensitive comparison.
**
** libc370 2.0 (#256, #250).
*/

#include <stddef.h>

/* case-insensitive comparison.  Both fold through tolower(), so they
   are EBCDIC-correct.  1.x also declared stricmp()/strncmpi(), the
   same code under MS-style names; they are gone in 2.0 (#250). */
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

#endif /* STRINGS_H */
