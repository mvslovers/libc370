/* @@DBLCVT.C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>

/*
   result has rsize bytes, and nothing is written past them (#222).
   What does not fit is cut off: the NUL always stays, an exponent
   before any fraction digit, digits before padding.
*/
void
__dblcvt(double num, char cnvtype, size_t nwidth, int nprecision,
         char *result, size_t rsize)
{
    double b,round;
    int i,j,exp,pdigits,format;
    char sign;
    size_t n,lim,dlim,pad;

/* append one character while there is room below dlim */
#define DBLCVT_PUT(c) do { if (n < dlim) result[n++] = (c); } while (0)

    if (rsize == 0) {
        return;
    }
    n = 0;
    lim = rsize - 1;

    /* save original data & set sign */
    if ( num < 0 ) {
        b = -num;
        sign = '-';
    }
    else {
        b = num;
        sign = ' ';
    }

    /*
      Now scale to get exponent
    */
    exp = 0;
    if( b > 1.0 ) {
        while ((b >= 10.0) && (exp < 120)) {
            ++exp;
            b=b / 10.0;
        }
    }
    else if ( b == 0.0 ) {
        exp=0;
    }
    /* 1.0 will get exp = 0 */
    else if ( b < 1.0 ) {
        while ((b < 1.0) && (exp > -120)) {
            --exp;
            b=b*10.0;
        }
    }

    if ((exp <= -120) || (exp >= 120)) {
        exp = 0;
        b = 0.0;
    }

    /*
      now decide how to print and save in FORMAT.
         -1 => we need leading digits
          0 => print in exp
         +1 => we have digits before dp.
    */
    switch (cnvtype) {
        case 'E':
        case 'e':
            format = 0;
            break;
        case 'f':
        case 'F':
            if ( exp >= 0 ) {
                format = 1;
            }
            else {
                format = -1;
            }
            break;
        default:
            /* Style e is used if the exponent from its
               conversion is less than -4 or greater than
               or equal to the precision.
            */
            if ( exp >= 0 ) {
                if ( nprecision > exp ) {
                    format=1;
                }
                else {
                    format=0;
                }
            }
            else {
                /*  if ( nprecision > (-(exp+1) ) ) { */
                if ( exp >= -4) {
                    format=-1;
                }
                else {
                    format=0;
                }
            }
            break;
    }

    /*
    Now round: add half a unit of the last digit printed, which is
    fractional digit j of b.  j only has to be bounded so the addend
    stays representable.  It must not be DBL_MANT_DIG - on S/370 that
    counts hex digits (14), and capping there added a fixed 5e-15 to
    every value printed with 14 or more digits (#209).  From digit
    DBLCVT_MAXRND on, the addend is below half an ulp of b in [1,10)
    (HFP: 16**-13, about 2.2e-16) and changes no digit.
    Zero is not rounded: no ulp absorbs the addend there, and it would
    print as a stray 5 (#209).
    */
#define DBLCVT_MAXRND 17
    switch (format) {
        case 0:    /* we are printing in standard form */
            j = nprecision;
            break;
        case 1:    /* we have a number > 1, round at digit exp+nprecision */
            j = exp + nprecision;
            break;
        default:   /* we have a number that starts 0.xxxx */
            /* j = -1: the first digit printed is the one before b's
               first, e.g. %.2f of 0.006 must round up to 0.01 */
            j = nprecision + exp;
            break;
    }
    if (j > DBLCVT_MAXRND) {
        j = DBLCVT_MAXRND;
    }

    if (j >= -1 && b != 0.0) {
        round = (j < 0) ? 5.0 : 0.5;
        i = 0;
        while (i++ < j) {
            round = round/10.0;
        }
        b = b + round;
        if (b >= 10.0) {
            b = b/10.0;
            exp = exp + 1;
        }
    }

    if (format == -1 && exp >= 0) {
        format = 1;
    }

    /*
       Now extract the requisite number of digits.  A positive number
       gets no sign position; the digits of an e-style number stop four
       bytes short, so its exponent always fits.
    */
    dlim = lim;
    if (format == 0) {
        dlim = (lim > 4) ? lim - 4 : 0;
    }
    if (sign == '-') {
        DBLCVT_PUT(sign);
    }

    if (format==-1) {
        /*
             Number < 1.0 so we need to print the "0."
             and the leading zeros...
        */
        DBLCVT_PUT('0');
        DBLCVT_PUT('.');
        while (++exp) {
            --nprecision;
            DBLCVT_PUT('0');
        }
        i=b;
        --nprecision;
        DBLCVT_PUT((char)('0' + i % 10));

        pdigits = nprecision;

        while (pdigits-- > 0 && n < dlim) {
            b = b - i;
            b = b * 10.0;
            i = b;
            DBLCVT_PUT((char)('0' + i % 10));
        }
    }
    /*
       Number >= 1.0 just print the first digit
    */
    else if (format==+1) {
        i = b;
        DBLCVT_PUT((char)('0' + i % 10));
        nprecision = nprecision + exp;
        pdigits = nprecision ;

        while (pdigits-- > 0 && n < dlim) {
            if ( ((nprecision-pdigits-1)==exp) ) {
                DBLCVT_PUT('.');
            }
            b = b - i;
            b = b * 10.0;
            i = b;
            DBLCVT_PUT((char)('0' + i % 10));
        }
    }
    /*
       printing in standard form
    */
    else {
        i = b;
        DBLCVT_PUT((char)('0' + i % 10));
        DBLCVT_PUT('.');

        pdigits = nprecision;

        while (pdigits-- > 0 && n < dlim) {
            b = b - i;
            b = b * 10.0;
            i = b;
            DBLCVT_PUT((char)('0' + i % 10));
        }
    }
    result[n] = '\0';

    if (format==0) { /* exp format - put exp on end */
        dlim = lim;
        DBLCVT_PUT('E');
        if ( exp < 0 ) {
            exp = -exp;
            DBLCVT_PUT('-');
        }
        else {
            DBLCVT_PUT('+');
        }
        DBLCVT_PUT((char)('0' + (exp/10) % 10));
        DBLCVT_PUT((char)('0' + exp % 10));
        result[n] = '\0';
    }
    else {
        /* get rid of trailing zeros for g specifier */
        if (cnvtype == 'G' || cnvtype == 'g') {
            char *p;

            p = strchr(result, '.');
            if (p != NULL) {
                p++;
                p = p + strlen(p) - 1;
                while (*p != '.' && *p == '0') {
                    *p = '\0';
                    p--;
                }
                if (*p == '.') {
                    *p = '\0';
                }
            }
            n = strlen(result);
        }
    }

    /* printf(" Final Answer = <%s> fprintf gives=%g\n",
                result,num); */
    /*
     do we need to pad - only as far as the buffer goes
    */
    if (nwidth > n) {
        pad = nwidth - n;
        if (pad > lim - n) {
            pad = lim - n;
        }
        memmove(result + pad, result, n + 1);
        while (pad > 0) {
            result[--pad] = ' ';
        }
    }
    return;
#undef DBLCVT_PUT
}
