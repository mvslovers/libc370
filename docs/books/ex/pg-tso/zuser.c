/* ZUSER: read the ISPF variable ZUSER into a C variable */
#include <stdio.h>
#include <string.h>
#include <mvs/ispf.h>

int main(void)
{
    char user[9];
    int  len = 8;
    int  rc;

    memset(user, 0, sizeof(user));

    /* tie the dialog variable ZUSER to the C array user */
    rc = isplink(ISP_VDEFINE, "(ZUSER)", user, "CHAR    ",
                 ISP_LAST(&len));
    if (rc != 0) {
        fprintf(stderr, "VDEFINE rc=%d\n", rc);
        return rc;
    }

    /* copy its value from the shared pool into user */
    rc = isplink(ISP_VGET, ISP_LAST("(ZUSER)"));
    isplink(ISP_VDELETE, ISP_LAST("(ZUSER)"));

    if (rc != 0) {
        fprintf(stderr, "VGET rc=%d\n", rc);
        return rc;
    }
    printf("ISPF user is %s\n", user);
    return 0;
}
