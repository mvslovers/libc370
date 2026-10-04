#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int tries;

    for (tries = 1; tries <= 3; tries++) {
        printf("attempt %d\n", tries);
        sleep(5);               /* wait five seconds between attempts */
    }
    usleep(250000);             /* and a quarter of a second at the end */
    return 0;
}
