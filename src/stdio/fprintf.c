/* FPRINTF.C */
#include <stdio.h>

/* Through vfprintf(), like printf() (#385).  It used to format into an
   8 KB stack buffer and fwrite() that: a result of 8192 characters or
   more was cut, its last byte written as X'00', and the count was
   fwrite()'s. */
int
fprintf(FILE *stream, const char *format, ...)
{
    va_list arg;
    int ret;

    va_start(arg, format);
    ret = vfprintf(stream, format, arg);
    va_end(arg);
    return (ret);
}
