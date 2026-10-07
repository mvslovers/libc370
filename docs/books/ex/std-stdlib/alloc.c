#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    size_t n = 1000;
    char **lines;
    char **more;

    lines = calloc(n, sizeof *lines);       /* zeroed array of pointers */
    if (lines == NULL) {
        fprintf(stderr, "no storage for %lu lines\n", (unsigned long)n);
        return EXIT_FAILURE;                /* condition code 12 */
    }

    more = realloc(lines, 2 * n * sizeof *lines);
    if (more == NULL) {                     /* the old block is still valid */
        free(lines);
        return EXIT_FAILURE;
    }
    lines = more;
    memset(lines + n, 0, n * sizeof *lines);

    free(lines);
    return EXIT_SUCCESS;
}
