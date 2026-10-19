/* STRSTR.C */
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/* strstr by Frank Adam */
/* modified by Paul Edwards */

#ifdef strstr
#undef strstr
#endif
__PDPCLIB_API__ char *strstr(const char *s1, const char *s2)
{
    const char *p = s1, *p1, *p2 = s2;

    /* C99: an empty s2 matches at s1 (#393).  strchr(s1, *s1) is s1, as a
       char * without a cast that drops the const */
    if (*s2 == '\0') return strchr(s1, *s1);


    while (*p) {

        if (*p == *s2) {
            p1 = p;
            p2 = s2;
            while ((*p2 != '\0') && (*p1 == *p2)) {
                p1++;
                p2++;
            }

            if (*p2 == '\0') {
                return (char *)p;
            }
        }
        p++;
    }

    return NULL;
}
