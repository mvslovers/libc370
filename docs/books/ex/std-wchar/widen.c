#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

int main(void)
{
    wchar_t wide[16];
    char narrow[16];
    size_t n;

    n = mbstowcs(wide, "MVS 3.8j", 16);     /* one byte, one wchar_t */
    printf("%lu wide characters\n", (unsigned long)n);

    n = wcstombs(narrow, L"back", sizeof narrow);
    if (n != (size_t)-1)
        printf("%s, %lu bytes\n", narrow, (unsigned long)n);
    return 0;
}
