/* @@WALKPD.C - __walkpd(): a PDS directory, member by member, to a callback
**
** The walk that __listpd() used to do inline (#80): no allocation, so a
** directory of any size costs one 256-byte block buffer.  The bounds are the
** ones #80's defect 2 settled -- believe the read, not the block's own length,
** and never read an entry that does not fit.
*/
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ext/strutil.h>        /* __patmat()                   */
#include "mvs/dslist.h"

int
__walkpd(const char *dataset, const char *filter, PDS_WALK fn, void *arg)
{
    FILE        *fp;
    int         count   = 0;
    unsigned    nread;
    unsigned    len;
    unsigned    pos;
    unsigned    size;
    char        buf[256];
    char        member[9];

    if (!dataset || !fn) {
        errno = EINVAL;
        return -1;
    }
    errno = 0;
    fp = fopen(dataset, "r,record");
    if (!fp) {
        /* fopen() does not always say why: a data set that does not exist
        ** leaves errno 0 (measured on MVS, JOB01074) */
        if (!errno) errno = EIO;
        return -1;
    }

    do {
        nread = fread(buf, 1, sizeof(buf), fp);
        if (nread < 2) break;       /* no block length to be read     */

        len = *(unsigned short *)buf;
        if (len > nread) len = nread;   /* believe the read, not the block */

        /* pos + 12 covers the 8 byte end-of-directory sentinel and the
        ** user data length in buf[pos+11]; the size test below covers the
        ** entry.  A block padded out behind its last entry stops here.
        */
        for (pos = 2; pos + 12 <= len; pos += size) {
            if (memcmp(&buf[pos], "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8) == 0)
                goto done;          /* logical end of directory       */

            size = 12 + ((buf[pos+11] & PDSLIST_IDC_UDATA) * 2);

            /* an entry whose user data runs past the block: stop trusting
            ** this block, keep what it yielded, read the next one */
            if (pos + size > len) break;

            if (filter) {
                /* the name is blank-padded to 8; cut it at the first blank
                ** by hand -- strtok() would end a caller's own walk (#301) */
                int i;
                memcpy(member, &buf[pos], 8);
                for (i = 0; i < 8 && member[i] != ' '; i++)
                    ;
                member[i] = 0;
                if (!__patmat(member, filter)) continue;
            }

            count++;
            if (fn(arg, (const PDSLIST *) &buf[pos]))
                goto done;          /* the caller has what it wants   */
        }
    } while (!feof(fp));

    if (ferror(fp)) {
        fclose(fp);
        errno = EIO;
        return -1;
    }
done:
    fclose(fp);
    return count;
}
