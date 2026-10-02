#ifndef CLIBAUTH_H
#define CLIBAUTH_H
#include <sys/_cc370.h>
#include <ibm/mvs/ihacde.h>

/* __autask() - make task APF authorized */
extern int __autask(void);

/* __uatask() - reverse of __autask(), reset APF authorization */
extern int __uatask(void);

/* __austep() - make STEPLIB dataset APF authorized */
extern int __austep(void);

/* __uastep() - reverse of __austep(), reset STEPLIB APF authorization */
extern int __uastep(void);

/* __isauth() - returns true if task is APF authorized, false if not */
extern int __isauth(void);

/* ---- from 1.x clibos.h -------------------------------------------------- */
/* __super() set supervisor mode and set PSW key, must be APF auth first */
/* returns error code if not APF authorized */
int __super(unsigned char pswkey, unsigned char *savekey);
#define PSWKEY0     0x00    /* key 0  0000 .... */
#define PSWKEY1     0x10    /* key 1  0001 .... */
#define PSWKEY2     0x20    /* key 2  0010 .... */
#define PSWKEY3     0x30    /* key 3  0011 .... */
#define PSWKEY4     0x40    /* key 4  0100 .... */
#define PSWKEY5     0x50    /* key 5  0101 .... */
#define PSWKEY6     0x60    /* key 6  0110 .... */
#define PSWKEY7     0x70    /* key 7  0111 .... */
#define PSWKEY8     0x80    /* key 8  1000 .... */
#define PSWKEY9     0x90    /* key 9  1001 .... */
#define PSWKEY10    0xA0    /* key 10 1010 .... */
#define PSWKEY11    0xB0    /* key 11 1011 .... */
#define PSWKEY12    0xC0    /* key 12 1100 .... */
#define PSWKEY13    0xD0    /* key 13 1101 .... */
#define PSWKEY14    0xE0    /* key 14 1110 .... */
#define PSWKEY15    0xF0    /* key 15 1111 .... */
#define PSWKEYNONE  0xFF    /* don't change PSW key */

/* __pswkey() get current PSW key, must be APF auth first */
/* returns error code if not APF authorized */
int __pswkey(unsigned char *savekey);

/* __prob() set problem mode (non-supervisor) and set PSW key, must be APF auth first */
/* returns error code if not APF authorized */
int __prob(unsigned char pswkey, unsigned char *savekey);

/* __isauth() returns true if APF authorized AC(1), false if not APF authorized */
int __isauth(void);

/* __issup() returns true if in supervisor mode, false if not in supervisor mode */
int __issup(void);

/* __sudo() - switch to super state, call function, return to previous state, return func return code as int */
int __sudo(void *func, ...);
int super_do(void *func, ...) asm("@@SUDO");

/* __sukydo() - switch to super state, switch to pswkey, call function, return to previous key and state, return func return code as int */
int __sukydo(unsigned char pswkey, void *func, ...);
int super_key_do(unsigned char pswkey, void *func, ...) asm("@@SUKYDO");

/* __steplb() - returns DCB address or NULL for STEPLIB DD */
void *__steplb(void) asm("@@STEPLB");

/* clib_apf_setup() - make this task and steplib APF authorized */
int clib_apf_setup(const char *pgm)                         asm("@@APFSET");

/* clib_auth_cde() - make CDE entry APF authorized AC(1) */
int clib_auth_cde(CDE *cde)                                 asm("@@AUTCDE");

/* clib_auth_name() - find CDE for program name and make APF authorized AC(1) */
int clib_auth_name(const char *name)                        asm("@@AUTNAM");

#endif

