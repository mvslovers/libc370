#include <ctype.h>
#include <stdio.h>

/* Count the letters of a string twice: with a range test,
   which is wrong in EBCDIC, and with isalpha(). */
int main(void)
{
    const char *s = "ab~z";
    int         range = 0;
    int         alpha = 0;

    for (; *s != '\0'; s++) {
        if (*s >= 'a' && *s <= 'z')     /* also true for '~' */
            range++;
        if (isalpha((unsigned char)*s))
            alpha++;
    }
    printf("range test %d, isalpha %d\n", range, alpha);
    return 0;
}
