#include <stdio.h>

/* Copy DD INPUT to DD OUTPUT record by record. Both are
   RECFM=VB; each record carries its record descriptor word,
   whose first two bytes give its length. */
int main(void)
{
    unsigned char rec[32760];           /* the longest V record */
    FILE    *in, *out;
    unsigned len;
    long     n = 0;
    int      rc = 0;

    in  = fopen("DD:INPUT",  "rb,record");
    out = fopen("DD:OUTPUT", "wb,record");
    if (in == NULL || out == NULL) {
        fputs("VBCOPY: cannot open INPUT or OUTPUT\n", stderr);
        return 16;
    }

    while (fread(rec, sizeof rec, 1, in) == 1) {
        len = (rec[0] << 8) | rec[1];       /* RDW included */
        if (fwrite(rec, len, 1, out) != 1) {
            perror("VBCOPY: DD OUTPUT");
            rc = 12;
            break;
        }
        n++;
    }
    if (ferror(in)) {
        perror("VBCOPY: DD INPUT");
        rc = 12;
    }

    fclose(in);
    if (fclose(out) == EOF) {
        perror("VBCOPY: DD OUTPUT");
        rc = 12;
    }
    printf("VBCOPY: %ld records\n", n);
    return rc;
}
