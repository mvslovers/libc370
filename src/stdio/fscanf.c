/* FSCANF.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdarg.h>

int
fscanf(FILE *stream, const char *format, ...)
{
    va_list arg;
    int ret;

    va_start(arg, format);
    ret = vvscanf(format, arg, stream, NULL);
    va_end(arg);
    return (ret);
}
