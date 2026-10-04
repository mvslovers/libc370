#ifndef SRC_INTERNAL_TOK_H
#define SRC_INTERNAL_TOK_H

#include <string.h>

/* strtok() with the position in *old instead of the CRT.  strtok() keeps
   one position per task, so a library function that tokenised with it
   moved the position of a caller looping with strtok() over its own string
   - the caller's next strtok(NULL, ...) continued inside the library's
   buffer (#301 for jesjob(); the same for fopen()'s DCB keywords,
   __listds(), __listvl() and the dynalloc text-unit builders).  This is
   strtok.c's algorithm, line for line, on a caller-owned position. */
static __inline char *__tok(char *s1, const char *s2, char **old)
{
    char    *p;
    size_t  len;
    size_t  remain;

    if (s1 != NULL) *old = s1;
    if (*old == NULL) return (NULL);

    p = *old;
    len = strspn(p, s2);
    remain = strlen(p);
    if (remain <= len) {
        *old = NULL;
        return (NULL);
    }

    p += len;
    len = strcspn(p, s2);
    remain = strlen(p);
    if (remain <= len) {
        *old = NULL;
        return (p);
    }

    *(p + len) = '\0';
    *old = p + len + 1;
    return (p);
}

#endif
