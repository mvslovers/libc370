/* FOPEN.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <ctype.h>

extern int  __fpshr(FILE *fp);
extern int  __fpold(FILE *fp);
extern int  __fpnew(FILE *fp);

static int  tsoname(FILE *fp, const char *fn);
static int  dsalloc(FILE *fp);
static int  is_tmpname(const char *fn, const char *name);

/* fopen() is __fpopn() with every kind of name: the TSO streams and the
   allocation of new and temporary data sets.  The standard streams open
   through __fpopn() without them (size analysis F5). */
FILE *
fopen(const char *fn, const char *mode)
{
    return __fpopn(fn, mode, tsoname, dsalloc);
}

/* "*PUTLINE" and "*GETLINE": -1 when fn is neither */
__asm__("\n&FUNC    SETC 'tsoname'");
static int
tsoname(FILE *fp, const char *fn)
{
    if (is_tmpname(fn, "*PUTLINE")) {
        /* "*PUTLINE": through the TMP's PUTLINE, no DD (#463) */
        return __fpput(fp);
    }

    if (is_tmpname(fn, "*GETLINE")) {
        /* "*GETLINE": through the TMP's GETLINE, no DD (#467) */
        return __fpget(fp);
    }

    return -1;
}

/* allocate the data set fp->dataset names; 0 when it is allocated */
__asm__("\n&FUNC    SETC 'dsalloc'");
static int
dsalloc(FILE *fp)
{
    int     mustexist;

    if ((fp->flags & _FILE_FLAG_READ) && !(fp->flags & _FILE_FLAG_WRITE)) {
        /* READ, allocate dataset DISP=SHR */
        return __fpshr(fp);
    }

    /* WRITE, allocate dataset.  A '+' stream can write, so it is
       allocated as a writer - DISP=OLD for a data set (#189) */
    mustexist = (fp->flags & _FILE_FLAG_READ) && fp->mode[0] == 'r';

    if (fp->dataset[0]=='&' && !mustexist) {
        /* temp dataset being allocated, try alloc DISP=NEW on VIO */
        if (!__fptmp(fp)) return 0;
    }
    if (fp->member[0]) {
        /* allocate PDS member DISP=SHR */
        if (!__fpshr(fp)) return 0;
    }
    /* allocate dataset DISP=OLD */
    if (!__fpold(fp)) return 0;

    /* try allocating dataset DISP=NEW - but "r+" opens what exists */
    return mustexist ? 1 : __fpnew(fp);
}

/* is_tmpname() - fn is name, "*PUTLINE" (#463) or "*GETLINE" (#467), in any
   case.  The names are reserved: they never open a DD of that name. */
static int
is_tmpname(const char *fn, const char *name)
{
    int     i;

    for (i = 0; name[i]; i++) {
        if (toupper((unsigned char)fn[i]) != name[i]) return 0;
    }
    return fn[i] == '\0';
}
