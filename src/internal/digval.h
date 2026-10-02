#ifndef SRC_INTERNAL_DIGVAL_H
#define SRC_INTERNAL_DIGVAL_H

#include <ctype.h>

/* The value of digit c in bases up to 36, or 36 when c is no digit.  A
   table rather than c - 'A' + 10: the EBCDIC letters are not contiguous
   (A-I X'C1'-X'C9', J-R X'D1'-X'D9', S-Z X'E2'-X'E9').  Shared by the
   strto* family (#316). */
static __inline int __digval(int c)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int i;

    c = tolower(c);
    for (i = 0; i < 36; i++) {
        if (digits[i] == c) {
            return (i);
        }
    }
    return (36);
}

#endif
