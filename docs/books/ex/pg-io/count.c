#include <stdio.h>
#include <string.h>

/* Count the lines of DD INPUT and find the longest one,
   not counting the blanks that pad a fixed-length record. */
int main(void)
{
    FILE   *in;
    char    line[258];          /* LRECL up to 256, '\n', NUL */
    size_t  len;
    size_t  longest = 0;
    long    lines = 0;

    in = fopen("DD:INPUT", "r");
    if (in == NULL) {
        fputs("COUNT: cannot open DD INPUT\n", stderr);
        return 16;
    }

    while (fgets(line, sizeof line, in) != NULL) {
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            len--;              /* a whole record was read */
        while (len > 0 && line[len - 1] == ' ')
            len--;
        if (len > longest)
            longest = len;
        lines++;
    }

    if (ferror(in)) {
        perror("COUNT: DD INPUT");
        fclose(in);
        return 12;
    }
    fclose(in);

    printf("%ld lines, the longest has %lu characters\n",
           lines, (unsigned long)longest);
    return 0;
}
