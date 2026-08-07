#ifndef RACHECK_H
#define RACHECK_H

typedef struct racheck  RACHECK;
typedef struct raclass  RACLASS;
typedef struct raentity RAENTITY;

struct racheck {
    char    len;                    /* 00 length of racheck struct (plist)  */
    char    instdata[3];            /* 01 AL3(address of installation data) */
/* The three flag bytes and the ATTR byte below were checked against the
** RACHECK macro itself -- sysmac/racheck.macro, which is the primary source
** and the only one -- on 2026-08-07 for #63.  Each DC there names its bits
** high-order first, so BIT0 is 0x80:
**
**     flag1  DC B'&BIT0&BIT1&BIT2&BIT3&BIT4&BIT5&BIT6&BIT7'   (:182, :586)
**     attr   DC B'&BIT0.000&BIT4&BIT5&BIT6.0'                 (:231)
**     flag2  DC B'0&BIT1&BIT2&BIT3&BIT4&BIT5.00'              (:265)
**
** One constant was wrong: LOG_NONE was 0x10, which is DSTYPE=V.  The ATTR
** values were all correct.  flag2 had no constants at all and now has the
** three the macro can set.
*/
    char    flag1;                  /* 04 flags                             */
#define RACHECK_FLAG1_RACFIND   0x80/* ... RACFIND coded            (BIT0)  */
#define RACHECK_FLAG1_RACFIND_Y 0x40/* ... RACFIND=YES              (BIT1)  */
#define RACHECK_FLAG1_DSTYPE_V  0x10/* ... DSTYPE=V, a VSAM dataset (BIT3)  */
#define RACHECK_FLAG1_31BIT     0x08/* ... 31-bit plist (RACROUTE)  (BIT4)  */
#define RACHECK_FLAG1_LOG_NOFAIL 0x04/*... LOG=NOFAIL or =NOSTAT    (BIT5)  */
#define RACHECK_FLAG1_LOG_NONE  0x02/* ... LOG=NONE or =NOSTAT      (BIT6)  */
#define RACHECK_FLAG1_ENTITY_CSA 0x01/*... ENTITY(...,CSA)          (BIT7)  */
    char    entity[3];              /* 05 AL3(entity name)                  */
    char    attr;                   /* 08 attr flags                        */
#define RACHECK_ATTR_READ       0x02/* ... READ, also the default   (BIT6)  */
#define RACHECK_ATTR_UPDATE     0x04/* ... UPDATE                   (BIT5)  */
#define RACHECK_ATTR_CONTROL    0x08/* ... CONTROL                  (BIT4)  */
#define RACHECK_ATTR_ALTER      0x80/* ... ALTER                    (BIT0)  */

    char    aclass[3];              /* 09 AL3(class name)                   */

    char    flag2;                  /* 0C flags                             */
#define RACHECK_FLAG2_DSTYPE_M  0x40/* ... DSTYPE=M                 (BIT1)  */
#define RACHECK_FLAG2_PROFILE   0x20/* ... PROFILE= given           (BIT2)  */
#define RACHECK_FLAG2_GENERIC   0x04/* ... GENERIC=YES (not ASIS)   (BIT5)  */
    char    volser[3];              /* 0D AL3(volser name)                  */

    void    *oldvolser;             /* 10 old volser addr                   */
    void    *appl;                  /* 14 appl                              */
    void    *acee;                  /* 18 ACEE                              */
    void    *owner;                 /* 1C owner                             */
    void    *unused[4];             /* 20 unused                            */
    void    *access1;               /* 30 access value address              */
    void    *access2;               /* 34 2nd access address                */
};

struct raclass {
	char    len;					/* 00 length of class name				*/
	char    name[8];				/* 01 class name						*/
};

struct raentity {
	char	name[40];				/* 00 entity name						*/
};

#endif
