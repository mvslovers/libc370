#include <stdio.h>

/* Copy SYSIN into a new data set whose name is the parameter,
   with fixed-length 80-byte records in blocks of 3120 bytes. */
int main(int argc, char **argv)
{
    char  name[48];
    char  line[82];
    FILE *out;
    long  n = 0;

    if (argc != 2) {
        fputs("NEWDS: give the data set name as the parameter\n", stderr);
        return 16;
    }
    snprintf(name, sizeof name, "'%s'", argv[1]);   /* fully qualified */

    out = fopen(name, "w,recfm=fb,lrecl=80,blksize=3120,space=trk(5,5),rlse");
    if (out == NULL) {
        fprintf(stderr, "NEWDS: cannot open %s\n", name);
        return 12;
    }

    while (fgets(line, sizeof line, stdin) != NULL) {
        fputs(line, out);
        n++;
    }

    /* the last block is written by the close: check it */
    if (fclose(out) == EOF) {
        perror(name);
        return 12;
    }
    printf("NEWDS: %ld records written to %s\n", n, name);
    return 0;
}
