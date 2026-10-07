/* PANEL: display an ISPF panel through the ISPEXEC command */
#include <stdio.h>
#include <string.h>
#include <mvs/crt.h>
#include <mvs/tso.h>

int main(int argc, char **argv)
{
    CLIBPPA *ppa = __ppaget();
    char     req[256];
    int      rc;

    if (argc < 2 || strlen(argv[1]) > 8) {
        fprintf(stderr, "usage: PANEL name\n");
        return 4;
    }
    if (!ppa || !ppa->ppacppl) {
        fprintf(stderr, "PANEL must be run as a TSO command\n");
        return 8;
    }

    /* Build the request in our own buffer and pass it to tsocmd(),
       which does not format it again. */
    snprintf(req, sizeof(req), "DISPLAY PANEL(%s)", argv[1]);
    rc = tsocmd("ISPEXEC", req);

    if (rc == 0)
        printf("ENTER pressed\n");
    else if (rc == 8)
        printf("END or RETURN pressed\n");
    else
        printf("ISPEXEC return code %d\n", rc);
    return rc == 0 || rc == 8 ? 0 : rc;
}
