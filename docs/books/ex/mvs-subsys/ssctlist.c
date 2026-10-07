#include <stdio.h>
#include <mvs/subsys.h>

int main(void)
{
    SSCT *ssct;

    for (ssct = ssct_find(NULL); ssct; ssct = ssct->ssctscta)
        printf("%.4s  SSVT %08X\n",
               ssct->ssctsnam, (unsigned)ssct->ssctssvt);
    return 0;
}
