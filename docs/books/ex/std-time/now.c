#include <stdio.h>
#include <time.h>

int main(void)
{
    time_t now = time(NULL);
    struct tm local;
    char buf[64];

    localtime_r(&now, &local);
    if (strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &local) > 0)
        printf("local time %s, %d seconds from UTC\n", buf, __tzget());
    printf("%s", ctime(&now));      /* "Sun Oct  4 14:05:09 2026\n" */
    return 0;
}
