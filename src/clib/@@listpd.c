/* @@LISTPD.C - create PDSLIST array */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "clibary.h"        /* dynamic array prototypes     */
#include "cliblist.h"       /* __listpd()                   */
#include "clibstr.h"        /* __patmat()                   */

PDSLIST **
__listpd(const char *dataset, const char *filter)
{
    int         rc      = 0;
    FILE        *fp     = 0;
    PDSLIST     **array = 0;
    PDSLIST     *pdslist;
    unsigned    nread;
    unsigned    len;
    unsigned    pos;
    unsigned    size;
    char        buf[256];
    char        member[12];

    fp = fopen(dataset, "r,record");
    if (!fp) goto quit;

    do {
        nread = fread(buf, 1, sizeof(buf), fp);
        if (nread < 2) goto quit;   /* no block length to be read     */

        len = *(unsigned short *)buf;
        if (len > nread) len = nread;   /* believe the read, not the block */

        /* pos + 12 covers the 8 byte end-of-directory sentinel and the
        ** user data length in buf[pos+11]; the size test below covers the
        ** copy.  A block padded out behind its last entry stops here.
        */
        for(pos = 2; pos + 12 <= len; pos += size) {
            if (memcmp(&buf[pos], "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8)==0) {
                /* logical end of directory */
                goto quit;
            }
            size = 12;  /* member,ttr,c = 8+3+1 */
            size += ((buf[pos+11] & 0x1F) * 2); /* + size of user data */

            /* an entry whose user data runs past the block: stop trusting
            ** this block, keep what it already yielded, read the next one.
            ** Signalling the shortfall to the caller is defect 3 of #80.
            */
            if (pos + size > len) break;

            if (filter) {
                memcpy(member, &buf[pos], 8);
                member[8] = 0;
                strtok(member, " ");
                if (!__patmat(member, filter)) continue;
            }

            pdslist = calloc(1, size);
            if (!pdslist) goto quit;

            memcpy(pdslist, &buf[pos], size);
            rc = arrayadd(&array, pdslist);
            if (rc) {
                free(pdslist);
                goto quit;
            }
        }
    } while(!feof(fp));

quit:
    if (fp) fclose(fp);
    return array;
}
