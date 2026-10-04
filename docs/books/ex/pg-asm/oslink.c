/* OLDSUB is an assembler routine written for the OS convention:  */
/* R1 points to a list of addresses, the last one flagged.        */
extern int oldsub(void *rec, void *len) asm("OLDSUB");

#define VL(p) ((void *)((unsigned)(p) | 0x80000000))

int put_record(char *rec, int len)
{
    return oldsub(rec, VL(&len));    /* the addresses, not values */
}
