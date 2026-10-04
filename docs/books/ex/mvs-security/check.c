#include <stdio.h>
#include <mvs/racf.h>

int main(int argc, char **argv)
{
    int  racrc;
    int  rc;
    ACEE *acee;

    if (argc < 3) return 12;

    /* the program must run APF-authorized for racf_login() */
    acee = racf_login(argv[1], argv[2], NULL, &racrc);
    if (!acee) {
        printf("logon for %s refused, RACINIT rc=%d\n", argv[1], racrc);
        return 8;
    }

    rc = racf_auth(acee, "FACILITY", "DEMO.RESOURCE", RACF_ATTR_READ);
    if (rc <= 4)            /* 0 permitted, 4 not protected */
        printf("%s may use DEMO.RESOURCE (rc=%d)\n", argv[1], rc);
    else
        printf("%s may not use DEMO.RESOURCE (rc=%d)\n", argv[1], rc);

    racf_logout(&acee);
    return 0;
}
