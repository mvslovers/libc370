#include <string.h>
#include <mvs/wto.h>

int main(void)
{
    char reply[8];

    wtorf(reply, sizeof(reply),
          "DEMO010A REPLY GO OR END FOR JOB STEP %s", "STEP1");
    if (strncmp(reply, "GO", 2) != 0) {
        wtof("DEMO011I ENDED BY OPERATOR, REPLY WAS '%s'", reply);
        return 4;
    }
    return 0;
}
