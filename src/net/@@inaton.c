/* @@INATON.C - inet_aton(): an IPv4 address in BSD's forms (#51)
**
** "a.b.c.d", "a.b.c" (c: 16 bits), "a.b" (b: 24 bits) or "a" (32 bits), each
** part decimal, octal (leading 0) or hex (leading 0x).  The string ends after
** the last part, or at a blank.  No sign, no empty part, no part too large.
**
** The digits are read here rather than by strtoul(): '0'-'9', 'a'-'f' and
** 'A'-'F' are contiguous in EBCDIC as in ASCII, and a caller converting an
** address does not want strtoul() and its locale baggage in its load module.
*/
#include <arpa/inet.h>

static int digit(char c, int base)
{
    int d;

    if (c >= '0' && c <= '9')                    d = c - '0';
    else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else                                         return -1;
    return d < base ? d : -1;
}

/* one part; a pointer past it, or NULL when cp starts none or it passes 32 bits */
static const char *number(const char *cp, unsigned long *val)
{
    unsigned long   v       = 0;
    int             base    = 10;
    int             any     = 0;
    int             d;

    if (*cp < '0' || *cp > '9')
        return 0;                       /* no sign, no blank, no empty part */
    if (*cp == '0') {
        base = 8; cp++; any = 1;
        if (*cp == 'x' || *cp == 'X') {
            base = 16; cp++; any = 0;   /* "0x" needs a digit after it */
        }
    }
    while ((d = digit(*cp, base)) >= 0) {
        if (v > (0xFFFFFFFFUL - (unsigned long) d) / (unsigned long) base)
            return 0;
        v = v * base + d;
        cp++; any = 1;
    }
    if (!any)
        return 0;
    *val = v;
    return cp;
}

int inet_aton(const char *cp, struct in_addr *inp)
{
    /* the last part fills what the parts before it leave */
    static const unsigned long last[4] = {
        0xFFFFFFFFUL, 0x00FFFFFFUL, 0x0000FFFFUL, 0x000000FFUL };
    unsigned long   part[4];
    in_addr_t       addr;
    int             n, i;

    if (!cp)
        return 0;
    for (n = 0; ; n++) {
        cp = number(cp, &part[n]);
        if (!cp)
            return 0;
        if (*cp != '.')
            break;
        if (n == 3)
            return 0;                   /* a fifth part */
        cp++;
    }
    if (*cp && *cp != ' ' && *cp != '\t' && *cp != '\n')
        return 0;
    for (i = 0; i < n; i++)
        if (part[i] > 0xFF)
            return 0;
    if (part[n] > last[n])
        return 0;

    addr = part[n];
    for (i = 0; i < n; i++)
        addr |= part[i] << (24 - 8 * i);
    if (inp)
        inp->s_addr = addr;
    return 1;
}
