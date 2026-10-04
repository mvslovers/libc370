#include <stdio.h>
#include <string.h>
#include <mvs/vsam.h>

#define KEYLEN  8                       /* key: the first 8 bytes */

/* Add 1 to the counter in columns 10-14 of the record with KEY. */
int bump(const char *keyval)
{
    VSFILE  *vs;
    char    rec[80];
    char    key[KEYLEN];
    int     len, n, rc;

    rc = vsopen("COUNTERS", VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_UPD, &vs);
    if (rc) {
        printf("cannot open DD COUNTERS, errno %d\n", rc);
        return 8;
    }

    memset(key, ' ', sizeof(key));      /* keys are blank-padded */
    memcpy(key, keyval, strlen(keyval) < KEYLEN ? strlen(keyval) : KEYLEN);

    len = vsread(vs, rec, sizeof(rec), key, sizeof(key));
    if (len < 0) {
        /* a missing key is an error (reason 16), not end of data */
        printf("key %.8s: rc %d, reason %d\n", key, vs->rc, vs->rsn);
        vsclose(vs);
        return 4;
    }
    if (len < 14) {
        printf("record too short (%d)\n", len);
        vsclose(vs);
        return 8;
    }

    sscanf(rec + 9, "%5d", &n);
    sprintf(rec + 9, "%05d", n + 1);    /* overwrites column 15 ...  */
    rec[14] = ' ';                      /* ... so restore it         */

    rc = vsupdate(vs, rec, len);        /* rewrites the record read  */
    if (rc)
        printf("update failed: rc %d, reason %d\n", vs->rc, vs->rsn);

    vsclose(vs);
    return rc ? 8 : 0;
}

int main(int argc, char **argv)
{
    return argc > 1 ? bump(argv[1]) : 8;
}
