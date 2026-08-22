/* RENAME.C */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <mvssupa.h>
#include "cliblock.h"
#include "clibio.h"

/* Split a name of exactly the form dsn(member) into its upper-cased parts.
   Returns 1 and fills dsn[45]/mem[9] on a match; 0 for anything else,
   including the // filename forms fopen() accepts.

   The parenthesised part must be a PDS MEMBER name: first character
   alphabetic or national, the rest alphanumeric or national.  That is not
   pedantry - relative GDG generations are spelled dsn(0), dsn(+1), dsn(-1),
   and those must keep going to IDCAMS as whole-data-set operations, not be
   mistaken for a directory update on the GDG base. */
static int
split_member(const char *name, char *dsn, char *mem)
{
    static const char first[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ@#$";
    static const char rest[]  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$";
    const char  *lp;
    const char  *rp;
    int         i;

    if (!name || name[0] == '/') return 0;
    lp = strchr(name, '(');
    if (!lp || lp == name || lp - name > 44) return 0;
    rp = strchr(lp + 1, ')');
    if (!rp || rp[1] != 0 || rp - lp < 2 || rp - lp > 9) return 0;

    for (i = 0; i < rp - lp - 1; i++) {
        mem[i] = (char)toupper((unsigned char)lp[1 + i]);
        if (!strchr(i == 0 ? first : rest, mem[i])) return 0;
    }
    mem[i] = 0;
    for (i = 0; i < lp - name; i++) {
        dsn[i] = (char)toupper((unsigned char)name[i]);
    }
    dsn[i] = 0;
    return 1;
}

int
rename(const char *old, const char *newnam)
{
    int ret;
    char buf[FILENAME_MAX + FILENAME_MAX + 50];
    char *p;
    char dsn_old[45];
    char mem_old[9];
    char dsn_new[45];
    char mem_new[9];

    /* Renaming a member must not go through IDCAMS ALTER: its dynamic
       allocation demands the data set exclusively, and when this address
       space already holds an allocation of that DSN, dynamic allocation
       escalates the shared SYSDSN ENQ to exclusive with no way back down -
       the data set stays blocked for every other address space until the
       step ends.  Measured for ALTER on 2026-08-22 with the same KEEP-DD
       A/B that settled remove() (#131; the delete half was #127 /
       mvslovers/mvsmf#342).  STOW change under DISP=SHR is what ISPF does,
       and __renmem() preserves the member's TTR and user data.  Only a
       rename WITHIN one PDS is a directory update; everything else -
       whole data sets, cross-data-set forms - keeps IDCAMS ALTER. */
    if (split_member(old, dsn_old, mem_old) &&
        split_member(newnam, dsn_new, mem_new) &&
        strcmp(dsn_old, dsn_new) == 0) {
        return __renmem(dsn_old, mem_old, mem_new);
    }

    sprintf(buf, " ALTER %s NEWNAME(%s)", old, newnam);
    p = buf;
    while (*p != '\0') {
       *p = toupper((unsigned char)*p);
       p++;
    }

#if 0
    lock(__idcams,0);
    ret = __idcams(strlen(buf), buf);
    unlock(__idcams,0);
#else
    lock(idcams,0);
    ret = idcams(buf);
    unlock(idcams,0);
#endif
    return (ret);
}
