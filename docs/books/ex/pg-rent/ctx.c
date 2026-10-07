#include <stdio.h>
#include <string.h>

struct ctx {                     /* everything the run changes    */
    unsigned lines;
    unsigned errors;
    char     last[81];
};

static void count_line(struct ctx *cx, const char *line)
{
    cx->lines++;
    if (strncmp(line, "ERROR", 5) == 0) {
        cx->errors++;
        strncpy(cx->last, line, sizeof cx->last - 1);
    }
}

int main(void)
{
    struct ctx cx;               /* on the stack of this call     */
    char       buf[256];

    memset(&cx, 0, sizeof cx);
    while (fgets(buf, sizeof buf, stdin))
        count_line(&cx, buf);

    printf("%u lines, %u errors\n", cx.lines, cx.errors);
    if (cx.errors)
        printf("last: %s", cx.last);
    return cx.errors ? 4 : 0;
}
