/* CLEARERR.C */
#include <stdio.h>

void
clearerr(FILE *fp)
{
    /* _FILE_FLAG_ENOSPC only qualifies _FILE_FLAG_ERROR, so it goes with
       it - a stream whose error is lifted has no pending errno (#149). */
    fp->flags &= 0xFFFF - (_FILE_FLAG_ERROR + _FILE_FLAG_EOF
                           + _FILE_FLAG_ENOSPC);
    return;
}
