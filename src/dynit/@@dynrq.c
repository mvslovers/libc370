/* @@DYNRQ.C - the SVC 99 request of dynalloc() and dynfree() (#52) */
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <dynit.h>
#include "mvs/dynalloc.h"
#include "src/dynit/dynreq.h"

/* room for a text unit of len value bytes (a 6-byte header, or 4 without a
   value), kept on a halfword boundary.  NULL, and err -1, when it does not
   fit. */
static unsigned char *
room(DYNRQ *rq, unsigned len)
{
    unsigned        need = (len ? 6 + len : 4);
    unsigned char   *tu;

    need = (need + 1) & ~1U;
    if (rq->err) return NULL;
    if (rq->count >= DYNRQ_TU || rq->used + need > DYNRQ_BUF) {
        rq->err = -1;
        return NULL;
    }
    tu = rq->buf + rq->used;
    rq->used += need;
    rq->tu[rq->count++] = tu;
    return tu;
}

/* key, number of values, and the length of the one value there is */
static unsigned char *
unit(DYNRQ *rq, unsigned key, unsigned len)
{
    unsigned char   *tu = room(rq, len);

    if (!tu) return NULL;
    tu[0] = (unsigned char)(key >> 8);
    tu[1] = (unsigned char)key;
    tu[2] = 0;
    tu[3] = (unsigned char)(len ? 1 : 0);
    if (len) {
        tu[4] = (unsigned char)(len >> 8);
        tu[5] = (unsigned char)len;
    }
    return tu;
}

void
__dynrqb(DYNRQ *rq, unsigned key, unsigned long v, unsigned n)
{
    unsigned char   *tu = unit(rq, key, n);
    unsigned        i;

    if (!tu) return;
    for (i = n; i > 0; i--) {
        tu[6 + i - 1] = (unsigned char)(v & 0xFF);
        v >>= 8;
    }
}

unsigned char *
__dynrqs(DYNRQ *rq, unsigned key, const char *s, unsigned max)
{
    unsigned        len;
    unsigned        i;
    unsigned char   *tu;

    if (!s || !*s) return NULL;
    len = strlen(s);
    if (len > max) len = max;       /* z/OS cuts a value that is too long */
    tu = unit(rq, key, len);
    if (!tu) return NULL;
    for (i = 0; i < len; i++)
        tu[6 + i] = (unsigned char)toupper((unsigned char)s[i]);
    return tu;
}

unsigned char *
__dynrqr(DYNRQ *rq, unsigned key, unsigned n)
{
    unsigned char   *tu = unit(rq, key, n);

    if (tu) memset(tu + 6, ' ', n);
    return tu;
}

void
__dynrqm(DYNRQ *rq, char **items)
{
    unsigned char   *p;

    if (!items) return;
    for (; *items; items++) {
        p = (unsigned char *)((unsigned long)*items & ~0x80000000UL);
        if (rq->err) return;
        if (rq->count >= DYNRQ_TU) {
            rq->err = -1;
            return;
        }
        rq->tu[rq->count++] = p;
        if ((unsigned long)*items & 0x80000000UL) return;
    }
}

void
__dynrqc(char *dst, unsigned size, const unsigned char *tu)
{
    unsigned        len;

    if (!tu || !size) return;
    len = ((unsigned)tu[4] << 8) | tu[5];
    if (len > size - 1) len = size - 1;
    memcpy(dst, tu + 6, len);
    while (len > 0 && dst[len - 1] == ' ') len--;
    dst[len] = 0;
}

int
__dynrqx(DYNRQ *rq, int verb, int flag1, __dyn_t *dyn)
{
    int     rc;

    if (!rq->err && rq->count == 0) rq->err = -2;
    if (rq->err) {
        errno = (rq->err == -1) ? ENOMEM : EINVAL;
        return rq->err;
    }

    /* the last pointer carries the high-order bit */
    rq->last = rq->tu[rq->count - 1];
    rq->tu[rq->count - 1] =
        (unsigned char *)((unsigned long)rq->last | 0x80000000UL);

    rq->rb.len      = sizeof(RB99);
    rq->rb.request  = (unsigned char)verb;
    rq->rb.flag1    = (unsigned char)flag1;
    rq->rb.txtptr   = rq->tu;

    rc = __svc99(&rq->rb);
    dyn->__errcode  = rq->rb.error;
    dyn->__infocode = rq->rb.info;
    return rc;
}
