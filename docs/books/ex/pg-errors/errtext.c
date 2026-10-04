#include <errno.h>
#include <stdio.h>
#include <string.h>

/* strerror() has no text for some values and returns NULL. */
static const char *errtext(int err)
{
    const char *text = strerror(err);

    return text != NULL ? text : "no message for this value";
}

int main(void)
{
    FILE *out;
    int   err;
    int   i;

    out = fopen("DD:OUTPUT", "w");
    if (out == NULL) {
        /* errno is not reliable after a failed fopen() */
        fputs("ERRTEXT: cannot open DD OUTPUT\n", stderr);
        return 16;
    }

    for (i = 1; i <= 1000; i++)
        fprintf(out, "RECORD %04d\n", i);
    if (ferror(out)) {
        err = errno;
        fprintf(stderr, "ERRTEXT: write failed: %s (errno %d)\n",
                errtext(err), err);
    }

    if (fclose(out) == EOF) {
        err = errno;
        fprintf(stderr, "ERRTEXT: close failed: %s (errno %d)\n",
                errtext(err), err);
        return 12;
    }
    return 0;
}
