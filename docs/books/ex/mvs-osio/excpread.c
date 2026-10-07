/* Read block 0 of DD DISK with EXCP. */
#include <stdio.h>
#include <stdlib.h>
#include <mvs/osio.h>
#include <mvs/dasd.h>

int main(void)
{
    DCB      *dcb = osxdcb("DISK", NULL);
    char     sense[2];
    char     *buf;
    unsigned blkstrk;
    int      rc;

    if (!dcb) return 12;
    if (osxopen(dcb, 0) != 0) {
        osxclose(dcb, 1);
        return 8;
    }

    /* blocks per track on a 3350, no keys */
    blkstrk = trkcalc(DEV3350, 0, dcb->dcbblksi);
    buf = malloc(dcb->dcbblksi);
    if (!buf || blkstrk == 0) {
        osxclose(dcb, 1);
        return 12;
    }

    rc = osxread(dcb, blkstrk, 0, buf, sense);
    if (rc)
        printf("read failed, code %02X sense %02X%02X\n",
               rc, sense[0] & 0xFF, sense[1] & 0xFF);

    free(buf);
    osxclose(dcb, 1);
    return rc ? 8 : 0;
}
