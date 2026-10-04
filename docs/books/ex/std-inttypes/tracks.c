#include <stdio.h>
#include <inttypes.h>

int main(int argc, char **argv)
{
    uint32_t tracks = 4000;         /* tracks of a data set */
    intmax_t n;
    char *end;

    if (argc > 1) {
        n = strtoimax(argv[1], &end, 10);
        if (*end != '\0' || n <= 0 || n > UINT32_MAX) {
            fprintf(stderr, "bad track count: %s\n", argv[1]);
            return 8;
        }
        tracks = (uint32_t)n;
    }

    uint64_t bytes = (uint64_t)tracks * UINT64_C(19069);  /* 3350 track */
    printf("%" PRIu32 " tracks hold %" PRIu64 " bytes\n", tracks, bytes);
    return 0;
}
