/* Copy DD INPUT to DD OUTPUT with BSAM, one block at a time.
   Assumes RECFM=F: every block is BLKSIZE bytes long. */
#include <stdio.h>
#include <stdlib.h>
#include <mvs/osio.h>

int main(void)
{
    DCB     *in  = osbdcb("DD:INPUT", NULL);
    DCB     *out = osbdcb("OUTPUT", NULL);
    DECB    decb;
    char    *buf;
    int     rc, blocks;

    if (!in || !out) return 12;
    if (osbopen(in, 0, "read") != 0) return 8;
    if (osbopen(out, 0, "write") != 0) {
        osbclose(in, NULL, 1, 0);
        return 8;
    }

    buf = malloc(in->dcbblksi);
    if (!buf) return 12;

    for (blocks = 0; ; blocks++) {
        osread(&decb, in, buf, in->dcbblksi);
        if ((rc = oscheck(&decb)) != 0) break;   /* end of data or error */
        oswrite(&decb, out, buf, in->dcbblksi);
        if ((rc = oscheck(&decb)) != 0) break;
    }

    free(buf);
    osbclose(out, NULL, 1, 0);
    osbclose(in, NULL, 1, 0);
    printf("%d blocks copied, last CHECK %08X\n", blocks, rc);
    return 0;
}
