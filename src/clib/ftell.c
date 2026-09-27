/* FTELL.C */
#include <stdio.h>

long int
ftell(FILE *fp)
{
    /* "a+" is at the end, and its size is counted the first time anyone
       asks (#189) - fseek() does the counting */
    if (fp->flags & _FILE_FLAG_POSEND) fseek(fp, 0L, SEEK_END);
    return fp->filepos;
}
