#include <string.h>
#include <mvs/wto.h>

/* Ask the operator before a step that cannot be undone. */
int confirm(const char *dsn)
{
    char reply[4];

    wtorf(reply, sizeof(reply),         /* up to 3 characters and a null */
          "DEMO010A REPLY YES TO SCRATCH %s, NO TO KEEP IT", dsn);
    return strcmp(reply, "YES") == 0;
}
