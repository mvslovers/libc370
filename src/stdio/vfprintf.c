/* VFPRINTF.C */
#include "src/internal/fileio.h"
#include <stdio.h>

int
vfprintf(FILE *stream, const char *format, va_list arg)
{
    int ret;

    /* the only caller that names the stream layer (size analysis F2) */
    ret = vvprintf(format, arg, stream, NULL, __fputc, __fputs);

    return (ret);
}
