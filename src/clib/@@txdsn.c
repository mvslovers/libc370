/* @@TXDSN.C */
#include "svc99.h"
#include "clibary.h"

int
__txdsn(TXT99 ***txt99, const char *dataset)
{
    int     err     = 1;
    int     len;
    char    *p;
    char    *member;
    TXT99   *tu;
    char    dsn[80];

    if (dataset) {
        len = strlen(dataset);
        if (len >= sizeof(dsn)) goto quit;

        memcpy(dsn, dataset, len);
        dsn[len]=0;

        member = strchr(dsn, '(');
        if (member) {
            *member = 0;
            member++;
            p = strchr(member,')');
            if (p) *p = 0;
            len = strlen(member);

            tu = NewTXT99(DALMEMBR,1,len,member);
            if (!tu) goto quit;

            if (arrayadd(txt99, tu)) {
                free(tu);
                goto quit;
            }
            len = strlen(dsn);
        }

        tu = NewTXT99(DALDSNAM,1,len,dsn);
        if (!tu) goto quit;

        err = arrayadd(txt99, tu);
        if (err) free(tu);
    }

quit:
    return err;
}
