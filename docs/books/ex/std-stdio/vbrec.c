#include <stdio.h>

/* List the length of every record of a RECFM=VB data set.
   In record mode each fread() returns one record, and on V
   the record starts with its record descriptor word. */
int main(void)
{
    FILE           *f;
    unsigned char   rec[32760];
    unsigned        len;
    long            n = 0;
    int             rc;

    f = fopen("DD:INPUT", "rb,record");
    if (f == NULL)
        return 8;

    while (fread(rec, sizeof rec, 1, f) == 1) {
        len = (rec[0] << 8) | rec[1];       /* includes the RDW */
        printf("record %ld: %u bytes of data\n", ++n, len - 4);
    }

    rc = ferror(f) ? 8 : 0;
    fclose(f);
    return rc;
}
