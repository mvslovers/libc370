#ifndef MVS_WSA_H
#define MVS_WSA_H
#include <sys/_cc370.h>

typedef struct clibwsa  CLIBWSA;    /* per process writable static area     */

struct clibwsa {
    char        wsaeye[8];          /* 00 Eye catcher for dumps "CLIBWSA "  */
#define CLIBWSA_EYE     "CLIBWSA "  /* ...                                  */
    void        *wsakey;            /* 08 key for this WSA                  */
    unsigned    wsasize;            /* 0C length of this WSA                */
    char        wsastart[0];        /* 10 start of WSA returned to caller   */
};

extern void     *__wsaget(void *key, unsigned len);

#endif
