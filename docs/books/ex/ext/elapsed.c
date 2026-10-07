/* ELAPSED: show the time of day, and how long a loop takes */
#include <stdio.h>
#include <ext/time64.h>

int main(void)
{
    time64_t    now;
    mtime64_t   t0, t1, ms;
    char        when[26];
    char        num[21];
    volatile unsigned i;

    time64(&now);
    if (ctime64_r(&now, when))
        printf("started %s", when);

    mtime64(&t0);
    for (i = 0; i < 1000000; i++)
        ;
    mtime64(&t1);

    __64_sub(&t1, &t0, &ms);            /* ms = t1 - t0 */
    __64_to_string(&ms, num, sizeof(num));
    printf("loop took %s ms\n", num);
    return 0;
}
