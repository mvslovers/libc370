#include <stdio.h>
#include <string.h>
#include <time.h>

/* Write a short report into the member of DD OUTLIB that the
   parameter names. The library must exist; the member is
   created, or replaced if it exists. */
int main(int argc, char **argv)
{
    char       fname[24];
    char       stamp[32];
    FILE      *out;
    time_t     now = time(NULL);

    if (argc != 2 || strlen(argv[1]) < 1 || strlen(argv[1]) > 8) {
        fputs("MEMBER: give a member name of 1 to 8 characters\n", stderr);
        return 16;
    }
    snprintf(fname, sizeof fname, "DD:OUTLIB(%s)", argv[1]);

    out = fopen(fname, "w");
    if (out == NULL) {
        fprintf(stderr, "MEMBER: cannot open %s\n", fname);
        return 12;
    }
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M", localtime(&now));
    fprintf(out, "REPORT %s\n", argv[1]);
    fprintf(out, "WRITTEN %s\n", stamp);

    if (fclose(out) == EOF) {
        perror(fname);
        return 12;
    }
    return 0;
}
