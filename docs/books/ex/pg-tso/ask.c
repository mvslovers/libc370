/* ASK: read lines from the terminal until end of file */
#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[256];
    int  n = 0;

    printf("Enter names, /* to end:\n");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0')
            continue;           /* an empty line is not end of file */
        printf("Hello, %s\n", line);
        n++;
    }
    printf("%d name(s)\n", n);
    return 0;
}
