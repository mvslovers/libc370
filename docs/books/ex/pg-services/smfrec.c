#include <stdio.h>
#include <string.h>
#include <mvs/apf.h>
#include <mvs/smf.h>

#define MYTYPE 210                      /* a user record type */

struct myrec {                          /* offset in the record      */
    SMF_HEADER  hdr;                    /* X'00'  18 bytes           */
    char        program[8];             /* X'12'                     */
    char        rsvd[2];                /* X'1A'  the alignment gap  */
    unsigned    records;                /* X'1C'                     */
    unsigned    errors;                 /* X'20'  36 bytes in all    */
};

int write_stats(unsigned records, unsigned errors)
{
    struct myrec rec;
    int          rc;

    if (!__isauth())                    /* smf_write() would abend */
        return -2;
    if (!smf_active())
        return -1;

    memset(&rec, 0, sizeof(rec));       /* zero first ...          */
    smf_init(&rec, sizeof(rec), MYTYPE);/* ... then the header     */
    memcpy(rec.program, "DEMO    ", 8);
    rec.records = records;
    rec.errors  = errors;

    rc = smf_write(&rec);
    if (rc)
        printf("SMF record not written, rc=%d\n", rc);
    return rc;
}

int main(void)
{
    return write_stats(1000, 3) ? 4 : 0;
}
