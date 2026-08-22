/* REMOVE.C */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <mvssupa.h>
#include "cliblock.h"
#include "clibio.h"

int
remove(const char *filename)
{
    int ret;
    char buf[FILENAME_MAX + 50];
    char *p;
    const char *lp;
    const char *rp;

    /* A member deletion must not go through IDCAMS DELETE: its dynamic
       allocation demands the data set exclusively, and when this address
       space already holds an allocation of that DSN (a STEPLIB, an httpd
       SYSENV DD, a concurrently open file), dynamic allocation escalates
       the shared SYSDSN ENQ to exclusive with no way back down - the data
       set stays blocked for every other address space until the step ends
       (#127, mvslovers/mvsmf#342).  A directory update needs no exclusive:
       STOW delete under DISP=SHR is what ISPF does.  Anything that is not
       exactly dsn(member) - including the // filename forms fopen()
       accepts - keeps the IDCAMS path unchanged. */
    lp = strchr(filename, '(');
    if (filename[0] != '/' && lp) {
        rp = strchr(lp + 1, ')');
        if (rp && rp[1] == 0 && rp - lp > 1 && rp - lp <= 9 &&
            lp != filename && lp - filename <= 44) {
            /* The parenthesised part must be a PDS MEMBER name: first
               character alphabetic or national, the rest alphanumeric or
               national.  Relative GDG generations are spelled dsn(0),
               dsn(+1), dsn(-1) - those must keep going to IDCAMS as
               whole-data-set deletes, not become a STOW against the GDG
               base (#131 hardening of the #127 branch). */
            static const char first[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ@#$";
            static const char rest[]  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$";
            char dsn[45];
            char mem[9];
            int  i;
            int  ok = 1;

            for (i = 0; i < rp - lp - 1; i++) {
                mem[i] = (char)toupper((unsigned char)lp[1 + i]);
                if (!strchr(i == 0 ? first : rest, mem[i])) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                mem[i] = 0;
                for (i = 0; i < lp - filename; i++) {
                    dsn[i] = (char)toupper((unsigned char)filename[i]);
                }
                dsn[i] = 0;

                return __delmem(dsn, mem);
            }
        }
    }

    sprintf(buf, " DELETE %s", filename);
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
