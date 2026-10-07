#include <stdio.h>
#include <time.h>

int main(void)
{
    time_t start = time(NULL);
    time_t end;

    /* ... the work to be timed ... */

    end = time(NULL);
    /* time_t is unsigned: subtract the earlier from the later value */
    printf("%lu seconds\n", (unsigned long)(end - start));
    return 0;
}
