#include <stdio.h>
#include <errno.h>

/* Write a report into a new, cataloged data set:
   RECFM=FB, LRECL=80, 5 tracks primary and 5 secondary,
   unused space released at close. */
int main(void)
{
    FILE   *f;
    int     i;

    f = fopen("'MYUSER.REPORT.LIST'",
              "w,recfm=fb,lrecl=80,blksize=3120,space=trk(5,5),rlse");
    if (f == NULL) {
        printf("open failed, errno %d\n", errno);
        return 8;
    }

    for (i = 1; i <= 10; i++)
        fprintf(f, "LINE %03d\n", i);

    /* the last block is written here: check the result */
    if (fclose(f) == EOF) {
        perror("MYUSER.REPORT.LIST");
        return 8;
    }
    return 0;
}
