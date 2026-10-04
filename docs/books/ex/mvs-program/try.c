#include <stdio.h>
#include <mvs/recovery.h>

static int parse(const char *text, int *value)
{
    *value = 0;
    while (*text >= '0' && *text <= '9')
        *value = *value * 10 + (*text++ - '0');
    return 0;
}

int main(void)
{
    int value;
    int rc;

    rc = try(parse, "1234", &value);
    if (rc < 0) {
        printf("ESTAE could not be created, rc=%d\n", -rc);
        return 12;
    }
    if (rc > 0xFFF) {
        printf("parse abended S%03X\n", (rc >> 12) & 0xFFF);
        return 8;
    }
    if (rc > 0) {
        printf("parse abended U%04d\n", rc);
        return 8;
    }

    printf("value=%d\n", value);
    return 0;
}
