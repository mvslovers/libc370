/* Read a KSDS: one record by key, then the rest in key order. */
#include <stdio.h>
#include <errno.h>
#include <mvs/vsam.h>

int main(void)
{
    VSFILE  *vs;
    char    rec[256];
    char    key[8] = "00001000";
    int     len, rc;

    rc = vsopen("KSDS", VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_IN, &vs);
    if (rc) {
        printf("vsopen failed, errno %d\n", rc);
        return 8;
    }

    len = vsread(vs, rec, sizeof(rec), key, sizeof(key));
    if (len < 0) {
        printf("key not found: rc %d, reason %d\n", vs->rc, vs->rsn);
        vsclear(vs);
    }

    /* test vseof() and vserror() too: the return value of vsread()
       can report the end of the data one call late */
    for (;;) {
        len = vsread(vs, rec, sizeof(rec), NULL, 0);
        if (len < 0 || vseof(vs) || vserror(vs))
            break;
        printf("%.*s\n", len, rec);
    }
    if (len == -2 || vserror(vs))
        printf("read error: rc %d, reason %d\n", vs->rc, vs->rsn);

    vsclose(vs);
    return 0;
}
