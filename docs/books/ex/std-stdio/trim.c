#include <stdio.h>
#include <string.h>

/* Copy DD INPUT to stdout, one line at a time, without the
   blanks that pad each fixed-length record. */
int main(void)
{
    FILE   *in;
    char    line[258];          /* LRECL up to 256, '\n', NUL */
    size_t  len;

    in = fopen("DD:INPUT", "r");
    if (in == NULL) {
        fputs("cannot open DD INPUT\n", stderr);
        return 8;
    }

    while (fgets(line, sizeof line, in) != NULL) {
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            len--;
        while (len > 0 && line[len - 1] == ' ')
            len--;
        line[len] = '\0';
        puts(line);
    }

    if (ferror(in)) {
        perror("INPUT");
        fclose(in);
        return 8;
    }
    fclose(in);
    return 0;
}
