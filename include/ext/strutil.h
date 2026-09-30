#ifndef EXT_STRUTIL_H
#define EXT_STRUTIL_H
/* ext/strutil.h - libc370's own string helpers: padded copies,
** pattern match, a clearing memset.
**
** libc370 2.0 (#256, #250).
*/

#include <stddef.h>

/* copy source string to target string with pad character fill */
char *strcpyp(char *target, int tlen, const void *source, int pad );

/* copy source to target with pad character fill */
void *memcpyp(void *target, int tlen, void *source, int slen, int pad);

/* compare "str" against pattern "pat", returns true if match */
int __patmat( const char *str, const char *pat );

static __inline void *memclr(void *s, size_t n)
{
    __asm__ __volatile__("\n*** MEMCLR ***\n"
"         LR    14,%0           => target (s)\n"
"         LR    15,%1           => length (n)\n"
"         SLR   0,0             => source (NULL)\n"
"         SLR   1,1             zero fill\n"
"         MVCL  14,0            Set target to fill character"
    : : "r"(s), "r"(n) : "0", "1", "14", "15");
    return s;
}

#endif /* EXT_STRUTIL_H */
