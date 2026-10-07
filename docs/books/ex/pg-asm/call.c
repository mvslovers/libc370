extern int pd2int(const void *pd, int len) asm("PD2INT");

int price(const unsigned char *field)
{
    return pd2int(field, 4) + 1;
}
