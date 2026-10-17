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

   alt is printf's # flag: %e and %f keep the decimal point with
   precision 0, %g keeps its trailing zeros (#578).  __dblcvt() is
   __dblcvx() without it.
*/
void
__dblcvx(double num, char cnvtype, size_t nwidth, int nprecision, int alt,
         char *result, size_t rsize)
{
    double b,round;
    int i,j,exp,pdigits,format;
    char sign;
    char style;     /* 'e' or 'f': what %g resolves to (#578) */
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
      %g, C99 7.19.6.1: the precision P counts significant digits (0 is
      taken as 1).  With X the exponent the e style would have, after
      rounding to P digits, it is %f with precision P-1-X when
      P > X >= -4, else %e with precision P-1.  It used to take P as
      the number of decimals, decide on the unrounded exponent and keep
      P decimals in the e style: %.3g of 123.456 printed 123.456 (#578).
    */
#define DBLCVT_MAXRND 17
    style = (cnvtype == 'e' || cnvtype == 'E') ? 'e' : 'f';
    if (cnvtype == 'g' || cnvtype == 'G') {
        int p = (nprecision > 0) ? nprecision : 1;
        int x = exp;

        if (b != 0.0) {
            round = 0.5;
            for (i = 0; i < p - 1 && i < DBLCVT_MAXRND; i++) {
                round = round / 10.0;
            }
            if (b + round >= 10.0) {
                x++;
            }
        }
        if (x < p && x >= -4) {
            style = 'f';
            nprecision = p - 1 - x;
        }
        else {
            style = 'e';
            nprecision = p - 1;
        }
    }

    /*
      now decide how to print and save in FORMAT.
         -1 => we need leading digits
          0 => print in exp
         +1 => we have digits before dp.
    */
    if (style == 'e') {
        format = 0;
    }
    else if ( exp >= 0 ) {
        format = 1;
    }
    else {
        format = -1;
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
    switch (format) {
        case 0:    /* we are printing in standard form */
            j = nprecision;
            break;
        case 1:    /* we have a number > 1, round at digit exp+nprecision */
            j = exp + nprecision;
            break;
        default:   /* we have a number that starts 0.xxxx */
            j = nprecision + exp;
            break;
    }
    if (j > DBLCVT_MAXRND) {
        j = DBLCVT_MAXRND;
    }

    if (j >= 0 && b != 0.0) {
        round = 0.5;
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
    else if (j < 0) {
        /* %f of a number below 1 whose first digit lies past the last
           one printed: it rounds to one unit of the last place when the
           digit after it, b's first, is 5 or more (j == -1), else to
           zero.  This added 5 to b whatever its first digit, so %.3f of
           0.000123 printed 0.0006, and j < -1 printed digits past the
           precision: %.1f of 0.001 gave 0.001 (#578) */
        if (j == -1 && b >= 5.0) {
            b = 1.0;
            exp = exp + 1;
        }
        else {
            b = 0.0;
            exp = 0;
            format = 1;
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
        if (alt && exp == 0 && nprecision == 0) {
            DBLCVT_PUT('.');            /* %#.0f keeps its point */
        }
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
        if (alt && exp > 0 && nprecision == exp) {
            DBLCVT_PUT('.');            /* %#.0f of 10 and up */
        }
    }
    /*
       printing in standard form
    */
    else {
        i = b;
        DBLCVT_PUT((char)('0' + i % 10));
        if (nprecision > 0 || alt) {
            /* no point with precision 0, C99 7.19.6.1: %.0e of 1 is
               1e+00, it printed 1.E+00 (#578) */
            DBLCVT_PUT('.');
        }

        pdigits = nprecision;

        while (pdigits-- > 0 && n < dlim) {
            b = b - i;
            b = b * 10.0;
            i = b;
            DBLCVT_PUT((char)('0' + i % 10));
        }
    }
    result[n] = '\0';

    /* %g drops the trailing zeros of the fraction, and the point with
       them, in either style - before the exponent (#578); not with # */
    if ((cnvtype == 'G' || cnvtype == 'g') && !alt) {
        char *p = strchr(result, '.');

        if (p != NULL) {
            char *q = result + strlen(result) - 1;

            while (q > p && *q == '0') {
                *q-- = '\0';
            }
            if (q == p) {
                *q = '\0';
            }
        }
        n = strlen(result);
    }

    if (format==0) { /* exp format - put exp on end */
        dlim = lim;
        /* e for %e and %g, E for %E and %G (#221) */
        DBLCVT_PUT((cnvtype == 'e' || cnvtype == 'g') ? 'e' : 'E');
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

void
__dblcvt(double num, char cnvtype, size_t nwidth, int nprecision,
         char *result, size_t rsize)
{
    __dblcvx(num, cnvtype, nwidth, nprecision, 0, result, rsize);
}
