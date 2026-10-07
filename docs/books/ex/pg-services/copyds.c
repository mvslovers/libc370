#include <stdio.h>
#include <mvs/dynalloc.h>

/* Copy a sequential data set to a new one with the same DCB
   attributes. Both DDs are allocated and freed by the program. */
int main(int argc, char **argv)
{
    char    line[256];
    FILE    *in, *out;
    int     rc;

    if (argc < 3) {
        printf("usage: COPYDS from-dsn to-dsn\n");
        return 8;
    }

    /* 1. the input: an existing data set, shared */
    rc = __dsalcf(NULL, "DD=INPUT;DSN=%s;DISP=SHR", argv[1]);
    if (rc) {
        printf("cannot allocate %s, rc=%d\n", argv[1], rc);
        return 8;
    }

    /* 2. the output: new, cataloged, DCB taken from the input */
    rc = __dsalcf(NULL, "DD=OUTPUT;DSN=%s;DISP=(NEW,CATLG,DELETE);"
                        "DCBDSN=%s;UNIT=SYSDA;SPACE=TRK(5,5)",
                  argv[2], argv[1]);
    if (rc) {
        printf("cannot create %s, rc=%d\n", argv[2], rc);
        __dsfree("INPUT");
        return 8;
    }

    /* 3. the DDs are ordinary DD statements now */
    rc = 8;
    in  = fopen("DD:INPUT", "r");
    out = fopen("DD:OUTPUT", "w");
    if (in && out) {
        while (fgets(line, sizeof(line), in))
            fputs(line, out);
        rc = ferror(in) ? 8 : 0;
    }
    if (in && fclose(in))
        rc = 8;
    if (out && fclose(out))         /* the last block is written here */
        rc = 8;

    /* 4. free both; the dispositions take effect now */
    __dsfree("INPUT");
    __dsfree("OUTPUT");
    return rc;
}
