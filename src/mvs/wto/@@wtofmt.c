/* @@WTOFMT.C - console message text without the printf family (#564).
** See src/internal/wtofmt.h.
*/
#include "src/internal/wtofmt.h"

void
__wfstr(char *buf, int *i, int max, const char *s)
{
    while (*s && *i < max - 1) buf[(*i)++] = *s++;
    buf[*i] = 0;
}

void
__wfhex(char *buf, int *i, int max, unsigned v, int digits)
{
    static const char hx[] = "0123456789ABCDEF";

    while (digits-- > 0 && *i < max - 1)
        buf[(*i)++] = hx[(v >> (digits * 4)) & 15];
    buf[*i] = 0;
}

void
__wfdec(char *buf, int *i, int max, unsigned v)
{
    char    t[11];
    int     n = 0;

    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n > 0 && *i < max - 1) buf[(*i)++] = t[--n];
    buf[*i] = 0;
}
