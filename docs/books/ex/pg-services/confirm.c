#include <string.h>
#include <mvs/wto.h>

/* Ask the operator before a step that cannot be undone. */
int confirm(const char *dsn)
{
    char reply[4];

    memset(reply, 0, sizeof(reply));    /* the reply is not terminated */
    wtorf(reply, sizeof(reply) - 1,     /* one byte left for the null */
          "DEMO010A REPLY YES TO SCRATCH %s, NO TO KEEP IT", dsn);
    return strcmp(reply, "YES") == 0;
}
