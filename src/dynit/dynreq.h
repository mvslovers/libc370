#ifndef SRC_DYNIT_DYNREQ_H
#define SRC_DYNIT_DYNREQ_H
/* dynreq.h - the SVC 99 request that dynalloc() and dynfree() build (#52).
**
** One structure on the caller's stack holds the request block, the list of
** text unit pointers and the text units themselves: no storage is obtained,
** so nothing can leak and a shortage cannot arise.  A request that does not
** fit (only possible with many __miscitems) fails with -1, z/OS's code for
** "not enough storage to process all the text units".
**
** The adders never fail on their own: they set err, and __dynrqx() returns
** it before it issues anything.  The request block comes first, so the
** host test's stand-in for __svc99() finds the whole request from it.
*/
#include <dynit.h>
#include "ibm/mvs/iefzb4d0.h"

#define DYNRQ_TU    48              /* text unit pointers                */
#define DYNRQ_BUF   640             /* bytes for the library's own units */

typedef struct dynrq {
    RB99            rb;             /* first: what __svc99() is given    */
    int             err;            /* 0, -1 no room, -2 bad field       */
    unsigned        count;          /* pointers in tu[]                  */
    unsigned        used;           /* bytes of buf[] in use             */
    unsigned char   *last;          /* tu[count-1] before it is marked   */
    unsigned char   *tu[DYNRQ_TU];
    unsigned char   buf[DYNRQ_BUF];
} DYNRQ;

/* __dynrqb() - add a text unit with one value of n bytes (1 to 4), v
   written big-endian; n 0 adds a unit without a value (DALDUMMY) */
void __dynrqb(DYNRQ *rq, unsigned key, unsigned long v, unsigned n);

/* __dynrqs() - add a text unit with the string s, cut to max characters
   and in upper case.  NULL or "" adds nothing.  Returns the unit or NULL. */
unsigned char *__dynrqs(DYNRQ *rq, unsigned key, const char *s, unsigned max);

/* __dynrqr() - add a text unit for MVS to return n bytes in (DALRTDDN),
   blank filled.  Returns the unit or NULL. */
unsigned char *__dynrqr(DYNRQ *rq, unsigned key, unsigned n);

/* __dynrqm() - add the caller's own text units, a list whose last pointer
   has its high-order bit set (z/OS's __miscitems) */
void __dynrqm(DYNRQ *rq, char **items);

/* __dynrqc() - copy the value of returned unit tu to dst (size bytes with
   the terminating 0), trailing blanks removed */
void __dynrqc(char *dst, unsigned size, const unsigned char *tu);

/* __dynrqx() - issue the request: verb S99VRBAL or S99VRBUN, flag1 the
   first flag byte.  Stores the error and information codes in dyn. */
int __dynrqx(DYNRQ *rq, int verb, int flag1, __dyn_t *dyn);

#endif /* SRC_DYNIT_DYNREQ_H */
