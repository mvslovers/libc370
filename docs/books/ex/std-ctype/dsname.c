#include <ctype.h>
#include <stdio.h>

/* Check one qualifier of a data set name: 1 to 8 characters,
   a letter or national character first, then letters, digits,
   national characters or hyphens.  The qualifier is changed to
   uppercase in place. */
static int good_qualifier(char *q)
{
    int n, c;

    for (n = 0; q[n] != '\0'; n++) {
        c = toupper((unsigned char)q[n]);
        q[n] = (char)c;
        if (isalpha(c) || c == '$' || c == '#' || c == '@')
            continue;
        if (n > 0 && (isdigit(c) || c == '-'))
            continue;
        return 0;
    }
    return n >= 1 && n <= 8;
}

int main(void)
{
    char a[] = "sys1", b[] = "1abc", c[] = "Payroll#";

    printf("%s %d\n", a, good_qualifier(a));    /* SYS1 1     */
    printf("%s %d\n", b, good_qualifier(b));    /* 1abc 0     */
    printf("%s %d\n", c, good_qualifier(c));    /* PAYROLL# 1 */
    return 0;
}
