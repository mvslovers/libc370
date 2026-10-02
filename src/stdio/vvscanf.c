/* VVSCANF.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#define inch() ((fp == NULL) ? \
    (ch = (unsigned char)*s++) : (ch = getc(fp)))

/* vvscanf - the guts of the input scanning */
/* several mods by Dave Edwards */
int
vvscanf(const char *format, va_list arg, FILE *fp, const char *s)
{
    int ch;
    int fin = 0;
    int cnt = 0;
    /* The va_arg targets below are set and used under the same !skipvar
    ** test, startpos/startp under the same fp != NULL test, and the three
    ** mod/skip flags at the '%' that opens each format item -- so none is
    ** read unset. The compiler cannot follow any of those pairings, so
    ** they are initialized here to keep -Wuninitialized quiet (#102). */
    char *cptr = NULL;
    void *vptr = NULL;
    double *dptr = NULL;
    float *fptr = NULL;
    long startpos = 0;
    const char *startp = NULL;
    int skipvar = 0; /* nonzero if we are skipping this variable */
    /* the length modifier (#318): 0 none, 'H' hh, 'h' h, 'l' l, and 'q'
       for the 64-bit ll, j and L.  z and t are 'l': size_t and ptrdiff_t
       are long-sized under cc370. */
    int size = 0;
    int informatitem;  /* nonzero if % format item started */
           /* informatitem is 1 if we have processed "%l" but not the
              type letter (s,d,e,f,g,...) yet. */

    if (fp != NULL) {
        startpos = ftell(fp);
    }
    else {
        startp = s;
    }

    inch();
    informatitem = 0;   /* init */
    /* don't bail out at end of data as there may be a %n */
    /*if ((fp != NULL && ch == EOF) || (fp == NULL && ch == 0)) return EOF; */
                               /* initially at EOF or end of string */
    while (!fin) {
        if (*format == '\0') {
            fin = 1;
        }
        else if (*format == '%' || informatitem) {
            if(*format=='%') {  /* starting a format item */
                format++;
                size = 0;
                skipvar = 0;
                if (*format == '*') {
                    skipvar = 1;
                    format++;
                }
            }

            if (*format == '%') {  /* %% */
                if (ch != '%') return (cnt);
                inch();
                informatitem=0;
            }
            else if (*format == 'l') {
                /* Type modifier: l (long), a second one ll (long long) */
                size = (size == 'l') ? 'q' : 'l';
                informatitem=1;
            }
            else if (*format == 'h') {
                /* Type modifier: h (short), a second one hh (char) */
                size = (size == 'h') ? 'H' : 'h';
                informatitem=1;
            }
            else if (*format == 'j' || *format == 'L') {
                /* intmax_t, and L: long long (L is long double for e/f/g) */
                size = 'q';
                informatitem=1;
            }
            else if (*format == 'z' || *format == 't') {
                /* size_t, ptrdiff_t: both long-sized under cc370 */
                size = 'l';
                informatitem=1;
            }
            else {   /* process a type character: */
                informatitem=0;   /* end of format item */
                if (*format == 's') {
                    if (!skipvar) {
                        cptr = va_arg(arg, char *);
                    }

                    /* Skip leading whitespace: */
                    while (ch>=0 && isspace(ch)) inch();
                    if ((fp != NULL && ch == EOF) || (fp == NULL && ch == 0)) {
                                         /* at EOF or end of string */
                        if (!skipvar) {
                            *cptr = 0;   /* give a null string */
                        }
                        continue;
                    }
                    else {
                        for(;;) {
                            if (isspace(ch)) break;
                            if ((fp != NULL && ch == EOF)
                                || (fp == NULL && ch == 0)) {
                                break;
                            }

                            if (!skipvar) {
                                *cptr++ = (char)ch;
                            }
                            inch();
                        }

                        if (!skipvar) {
                            *cptr = '\0';
                        }
                        cnt++;
                    }
                }
                else if (*format == '[') {
                    int reverse = 0;
                    int found;
                    const char *first;
                    const char *last;
                    size_t size;
                    size_t mcnt = 0;

                    if (!skipvar) {
                        cptr = va_arg(arg, char *);
                    }

                    format++;
                    if (*format == '^') {
                        reverse = 1;
                        format++;
                    }

                    if (*format == '\0') break;

                    first = format;
                    format++;
                    last = strchr(format, ']');
                    if (last == NULL) return (cnt);

                    size = (size_t)(last - first);
                    while (1) {
                        /* note that C90 doesn't require special
                           processing for '-' so it hasn't been
                           added */
                        found = (memchr(first, ch, size) != NULL);
                        if (found && reverse) break;
                        if (!found && !reverse) break;
                        if (!skipvar) {
                            *cptr++ = (char)ch;
                        }
                        mcnt++;
                        inch();
                        /* if at EOF or end of string, bug out */
                        if ((fp != NULL && ch == EOF)
                            || (fp == NULL && ch == 0)) {
                            break;
                        }
                    }

                    if (mcnt > 0) {
                        if (!skipvar) {
                            *cptr++ = '\0';
                        }
                        cnt++;
                    }
                    else {
                        break;
                    }
                    format = last + 1;
                }
                else if (*format == 'c') {
                    if (!skipvar) {
                        cptr = va_arg(arg, char *);
                    }

                    if ((fp != NULL && ch == EOF)
                        || (fp == NULL && ch == 0)) {
                        /* do nothing */
                    }
                    else {
                        if (!skipvar) {
                            *cptr = ch;
                        }
                        cnt++;
                        inch();
                    }
                }
                else if (*format == 'n') {
                    long n;

                    vptr = va_arg(arg, void *);
                    if (fp != NULL) {
                        n = ftell(fp) - startpos;
                    }
                    else {
                        /* we need a -1 because s will point to
                           the character after the NUL */
                        n = (long)(s - startp - 1);
                    }
                    switch (size) {
                    case 'H': *(signed char *)vptr = (signed char)n; break;
                    case 'h': *(short *)vptr = (short)n; break;
                    case 'l': *(long *)vptr = n; break;
                    case 'q': *(long long *)vptr = n; break;
                    default:  *(int *)vptr = (int)n; break;
                    }
                }
                else if (*format == 'd' || *format == 'u'
                         || *format == 'x' || *format == 'o'
                         || *format == 'p'
                         || *format == 'i') {
                    int neg = 0;
                    unsigned long long x = 0;
                    int undecided = 0;
                    int base = 10;
                    int mcnt = 0;

                    if (*format == 'x') base = 16;
                    else if (*format == 'p') base = 16;
                    else if (*format == 'o') base = 8;
                    else if (*format == 'i') base = 0;
                    if (!skipvar) {
                        vptr = va_arg(arg, void *);
                    }

                    /* Skip leading whitespace: */
                    while (ch>=0 && isspace(ch)) inch();
                    if (ch == '-') {
                        neg = 1;
                        inch();
                    }
                    else if(ch == '+') inch();

                    /* this logic is the same as strtoul so if you
                       change this, change that one too */

                    if (base == 0) {
                        undecided = 1;
                    }

                    while (!((fp != NULL && ch == EOF)
                             || (fp == NULL && ch == 0))) {
                        if (isdigit((unsigned char)ch)) {
                            if (base == 0) {
                                if (ch == '0') {
                                    base = 8;
                                }
                                else {
                                    base = 10;
                                    undecided = 0;
                                }
                            }
                            x = x * base + (ch - '0');
                            inch();
                        }
                        else if (isalpha((unsigned char)ch)) {
                            if ((ch == 'X') || (ch == 'x')) {
                                if ((base == 0) || ((base == 8) && undecided)) {
                                    base = 16;
                                    undecided = 0;
                                    inch();
                                }
                                else if (base == 16) {
                                    /* hex values are allowed to have an
                                       optional 0x */
                                    inch();
                                }
                                else {
                                    break;
                                }
                            }
                            else if (base <= 10) {
                                break;
                            }
                            else {
                                x = x * base +
                                    (toupper((unsigned char)ch) - 'A') + 10;
                                inch();
                            }
                        }
                        else {
                            break;
                        }
                        mcnt++;
                    }

                    /* end of strtoul logic */

                    /* If we didn't get any characters, don't go any
                       further */
                    if (mcnt == 0) {
                        break;
                    }


                    if (!skipvar) {
                        /* the sign applies to the unsigned conversions
                           too, as in strtoull; each store truncates to
                           the size the length modifier names */
                        if (neg) {
                            x = 0 - x;
                        }
                        if ((*format == 'd') || (*format == 'i')) {
                            switch (size) {
                            case 'H': *(signed char *)vptr = (signed char)x;
                                      break;
                            case 'h': *(short *)vptr = (short)x; break;
                            case 'l': *(long *)vptr = (long)x; break;
                            case 'q': *(long long *)vptr = (long long)x;
                                      break;
                            default:  *(int *)vptr = (int)x; break;
                            }
                        }
                        else {
                            switch (size) {
                            case 'H': *(unsigned char *)vptr =
                                          (unsigned char)x;
                                      break;
                            case 'h': *(unsigned short *)vptr =
                                          (unsigned short)x;
                                      break;
                            case 'l': *(unsigned long *)vptr =
                                          (unsigned long)x;
                                      break;
                            case 'q': *(unsigned long long *)vptr = x;
                                      break;
                            default:  *(unsigned int *)vptr =
                                          (unsigned int)x;
                                      break;
                            }
                        }
                    }
                    cnt++;
                }
                else if (*format=='e' || *format=='f' || *format=='g' ||
                         *format=='E' || *format=='G') {
                    /* Floating-point (double or float) input item */
                    int negsw1,negsw2,dotsw,expsw,ndigs1,ndigs2,nfdigs;
                    int ntrailzer,expnum,expsignsw;
                    double fpval,pow10;

                    if (!skipvar) {
                        /* l, and L for long double, which is double */
                        if (size == 'l' || size == 'q')
                            dptr = va_arg(arg, double *);
                        else fptr = va_arg(arg, float *);
                    }
                    negsw1=0;   /* init */
                    negsw2=0;
                    dotsw=0;
                    expsw=0;
                    ndigs1=0;
                    ndigs2=0;
                    nfdigs=0;
                    ntrailzer=0;  /* # of trailing 0's unaccounted for */
                    expnum=0;
                    expsignsw=0;  /* nonzero means done +/- on exponent */
                    fpval=0.0;
                    /* Skip leading whitespace: */
                    while (ch>=0 && isspace(ch)) inch();
                    if (ch=='-') {
                        negsw1=1;
                        inch();
                    }
                    else if (ch=='+') inch();

                    while (ch>0) {
                        if (ch=='.' && dotsw==0 && expsw==0) dotsw=1;
                        else if (isdigit(ch)) {
                            if(expsw) {
                                ndigs2++;
                                expnum=expnum*10+(ch-'0');
                            }
                            else {
                                /* To avoid overflow or loss of precision,
                                   skip leading and trailing zeros unless
                                   really needed. (Automatic for leading
                                   0's, since 0.0*10.0 is 0.0) */
                                ndigs1++;
                                if (dotsw) nfdigs++;
                                if (ch=='0' && fpval!=0.) {
                                    /* Possible trailing 0 */
                                    ntrailzer++;
                                }
                                else {
                                    /* Account for any preceding zeros */
                                    while (ntrailzer>0) {
                                        fpval*=10.;
                                        ntrailzer--;
                                    }
                                    fpval=fpval*10.0+(ch-'0');
                                }
                            }
                        }
                        else if ((ch=='e' || ch=='E') && expsw==0) expsw=1;
                        else if ((ch=='+' || ch=='-') && expsw==1
                                 && ndigs2==0 && expsignsw==0) {
                            expsignsw=1;
                            if (ch=='-') negsw2=1;
                        }
                        else break;   /* bad char: end of input item */
                        inch();
                    }

                    /* don't finish at end of input there may be a %n */
                    /*if ((fp != NULL && ch == EOF)
                        || (fp == NULL && ch == 0)) fin=1;*/
                    /* Check for a valid fl-pt value: */
                    if (ndigs1==0 || (expsw && ndigs2==0)) return(cnt);
                    /* Complete the fl-pt value: */
                    if (negsw2) expnum=-expnum;
                    expnum+=ntrailzer-nfdigs;
                    if (expnum!=0 && fpval!=0.0) {
                        negsw2=0;
                        if (expnum<0) {
                            expnum=-expnum;
                            negsw2=1;
                        }
                        /* Multiply or divide by 10.0**expnum, using
                           bits of expnum (fast method) */
                        pow10=10.0;
                        for (;;) {
                            if (expnum & 1) {   /* low-order bit */
                                if (negsw2) fpval/=pow10;
                                else fpval*=pow10;
                            }
                            expnum>>=1;   /* shift right 1 bit */
                            if (expnum==0) break;
                            pow10*=pow10;   /* 10.**n where n is power of 2 */
                        }
                    }

                    if (negsw1) fpval=-fpval;
                    if (!skipvar) {
                        /* l modifier: assign to double */
                        if (size == 'l' || size == 'q') *dptr=fpval;
                        else *fptr=(float)fpval;
                    }
                    cnt++;
                }
            }
        }
        else if (isspace((unsigned char)(*format))) {
            /* Whitespace char in format string: skip next whitespace
               chars in input data. This supports input of multiple
               data items. */
            while (ch>=0 && isspace(ch)) {
                inch();
            }
        }
        else { /* some other character in the format string */
            if (ch != *format) return (cnt);
            inch();
        }
        format++;
        /* don't bail out at end of string as there may be a %n */
        /*if ((fp != NULL && ch == EOF) || (fp == NULL && ch == 0)) fin = 1;*/
            /* EOF */
    }

    if (fp != NULL) ungetc(ch, fp);

    return (cnt);
}
