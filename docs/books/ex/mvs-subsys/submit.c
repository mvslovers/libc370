#include <stdio.h>
#include <mvs/jes2.h>

static const char *jcl[] = {
    "//HELLO    JOB (1),'SUBMIT',CLASS=A,MSGCLASS=X",
    "//STEP1    EXEC PGM=IEFBR14",
    NULL
};

int main(void)
{
    VSFILE        *rdr;
    unsigned char  jobid[8];
    int            i, rc;

    if (jesiropn(&rdr) != 0) return 8;

    for (i = 0; jcl[i]; i++)
        jesirput(rdr, (char *)jcl[i]);

    rc = jesircl2(rdr, jobid);
    printf("submitted as %.8s, rc %d\n", jobid, rc);
    return rc;
}
