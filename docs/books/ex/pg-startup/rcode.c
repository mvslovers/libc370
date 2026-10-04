#include <stdio.h>
#include <stdlib.h>

/* Runs at every normal end: return from main() or exit(). */
static void trailer(void)
{
    puts("REPORT ENDS");
}

int main(int argc, char **argv)
{
    char *end;
    long  limit;

    if (atexit(trailer) != 0)
        return 16;
    puts("REPORT BEGINS");

    if (argc < 2) {
        fputs("no limit given\n", stderr);
        exit(8);                        /* trailer() still runs */
    }
    limit = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || limit < 0) {
        fprintf(stderr, "%s is not a limit\n", argv[1]);
        return EXIT_FAILURE;            /* 12 on MVS */
    }

    printf("limit %ld\n", limit);
    return limit > 100 ? 4 : 0;         /* a warning, not an error */
}
