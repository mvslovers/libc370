#include <stdio.h>
#include <string.h>
#include <mvs/smf.h>

struct myrec {
    SMF_HEADER  hdr;                /* the standard 18-byte header  */
    char        jobname[8];         /* data of record type 200      */
    unsigned    count;
};

int main(void)
{
    struct myrec rec;
    int          rc;

    if (!smf_active()) {
        printf("SMF is not recording\n");
        return 4;
    }

    memset(&rec, 0, sizeof(rec));
    smf_init(&rec, sizeof(rec), 200);
    memcpy(rec.jobname, "MYJOB   ", 8);
    rec.count = 42;

    rc = smf_write(&rec);           /* needs APF authorization      */
    if (rc) {
        printf("smf_write rc=%d\n", rc);
        return 8;
    }
    return 0;
}
