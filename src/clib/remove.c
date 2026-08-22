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
            char dsn[45];
            char mem[9];
            int  i;

            for (i = 0; i < lp - filename; i++) {
                dsn[i] = toupper((unsigned char)filename[i]);
            }
            dsn[i] = 0;
            for (i = 0; i < rp - lp - 1; i++) {
                mem[i] = toupper((unsigned char)lp[1 + i]);
            }
            mem[i] = 0;

            return __delmem(dsn, mem);
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
