/* @@IDCREC.C - IDCAMS input records from a command string (#590)
**
** idcams() handed IDCAMS its whole command as one input record, and
** IDCAMS reads columns 2 to 72 of a record - from its I/O exit as from
** SYSIN.  rename() builds " ALTER old NEWNAME(new)", which passes column
** 72 once the two names are long enough, and the new name was cut there:
** the data set was renamed to a name the caller had not asked for
** (measured on MVS 3.8j: a 33-character name catalogued with 32).
**
** So the command is given to IDCAMS record by record:
** - a '\n' ends a record, so a caller can write several lines;
** - a line longer than 72 columns is broken at the last blank before
**   column 71 that is not inside quotes, with IDCAMS's continuation, a
**   '-' as the last character, and the rest goes on in the next record
**   after a leading blank (column 1 is not read);
** - a line that has no such blank cannot be broken, and __idcchk()
**   reports it before IDCAMS is called rather than letting IDCAMS see a
**   cut-off command.
*/
#include <string.h>
#include "src/internal/idcrec.h"

int
__idcrec(IDCREC *c, char *rec)
{
    const char  *s = c->next;
    const char  *nl;
    int         len;
    int         pre = c->cont ? 1 : 0;
    int         b;
    int         i;
    int         quoted;

    if (!s || !*s) return -1;

    for (len = 0; s[len] && s[len] != '\n'; len++)
        ;
    nl = s[len] ? s + len : 0;

    if (pre + len <= IDCREC_MAX) {
        /* the rest of the line fits */
        if (pre) rec[0] = ' ';
        memcpy(rec + pre, s, len);
        rec[pre + len] = 0;
        c->next = nl ? nl + 1 : s + len;
        c->cont = 0;
        return pre + len;
    }

    /* break at the last blank outside quotes that leaves room for "-"
       in column 72 at the latest: the record is s[0..b] and a '-' */
    b = -1;
    quoted = 0;
    for (i = 0; i < len && pre + i + 2 <= IDCREC_MAX; i++) {
        if (s[i] == '\'') quoted = !quoted;
        else if (s[i] == ' ' && !quoted && pre + i > 0) b = i;
    }
    if (b < 0) return -2;

    if (pre) rec[0] = ' ';
    memcpy(rec + pre, s, b + 1);
    rec[pre + b + 1] = '-';
    rec[pre + b + 2] = 0;
    c->next = s + b + 1;
    while (*c->next == ' ') c->next++;
    c->cont = 1;
    return pre + b + 2;
}

int
__idcchk(const char *cmds)
{
    IDCREC  c;
    char    rec[IDCREC_MAX + 1];
    int     n;

    c.next = cmds;
    c.cont = 0;
    while ((n = __idcrec(&c, rec)) >= 0)
        ;
    return n == -2 ? -2 : 0;
}
