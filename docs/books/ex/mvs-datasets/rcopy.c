#include <stdio.h>
#include <stdlib.h>
#include <mvs/rfile.h>

int main(void)
{
    RFILE  *in, *out;
    char   *rec;
    size_t len;
    int    rc = 0;

    if (ropen("dd:input", 0, &in)) {
        perror("ropen input");
        return 8;
    }
    if (ropen("dd:output", 1, &out)) {
        perror("ropen output");
        rclose(in);
        return 8;
    }

    rec = malloc(in->lrecl);
    if (!rec) {
        rclose(out);
        rclose(in);
        return 12;
    }

    while (rread(in, rec, &len) == 0) {
        if (rwrite(out, rec, len)) {
            perror("rwrite");
            rc = 8;
            break;
        }
    }

    free(rec);
    rclose(in);
    if (rclose(out)) {
        perror("rclose output");
        rc = 8;
    }
    return rc;
}
