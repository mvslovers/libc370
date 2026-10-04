/* asc.h - translate text between EBCDIC and ASCII for the network */
#ifndef ASC_H
#define ASC_H

typedef struct asctab {
    unsigned char toasc[256];   /* EBCDIC byte -> ASCII byte */
    unsigned char toebc[256];   /* ASCII byte  -> EBCDIC byte */
} ASCTAB;

void asc_init(ASCTAB *t)                          asm("ASCINIT");
void asc_out(const ASCTAB *t, char *buf, int len) asm("ASCOUT");
void asc_in(const ASCTAB *t, char *buf, int len)  asm("ASCIN");

#endif
