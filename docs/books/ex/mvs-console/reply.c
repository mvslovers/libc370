#include <string.h>
#include <mvs/wto.h>

int main(void)
{
    char reply[8];

    memset(reply, 0, sizeof(reply));   /* the reply is not terminated */
    wtorf(reply, sizeof(reply) - 1,
          "DEMO010A REPLY GO OR END FOR JOB STEP %s", "STEP1");
    if (strncmp(reply, "GO", 2) != 0) {
        wtof("DEMO011I ENDED BY OPERATOR, REPLY WAS '%s'", reply);
        return 4;
    }
    return 0;
}
