#ifndef MVS_CRT_H
#define MVS_CRT_H
/* mvs/crt.h - the runtime anchors: CLIBGRT (process), CLIBCRT (task), CLIBPPA (program).
**
** libc370 2.0 merges clibgrt.h, clibcrt.h and clibppa.h into this header (#256).
** Their declarations are unchanged and appear in dependency order.
*/

/* ---- 1.x clibgrt.h --------------------------------------------------- */
#include "clibwsa.h"                /* process level writable static areas  */

typedef struct clibgrt  CLIBGRT;    /* per process runtime work area        */

#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
#if (defined(__OS2__) || defined(__32BIT__) || defined(__MVS__) \
    || defined(__CMS__) || defined(__VSE__))
typedef unsigned long size_t;
#elif (defined(__MSDOS__) || defined(__DOS__) || defined(__POWERC) \
    || defined(__WIN32__) || defined(__gnu_linux__))
typedef unsigned int size_t;
#endif
#endif

/* This structure holds data unique for the process (main)
** regardless of the number of task/threads.  The @@CRT0.ASM
** code allocates the CLIBGRT and anchors it in the task level
** CRTGRT field in the CLIBCRT structure.
**
** All of the task/thread level structures refer to the same
** process level CLIBGRT area via the CRTGRT value which is
** propogated from parent task to child task at startup of
** each child task.
*/
struct clibgrt {
    char        grteye[8];          /* 00 Eye catcher for dumps "CLIBGRT "  */
    short       grtclibl;           /* 08 length of CLIBGRT area            */
    char        grtflag1;           /* 0A flags                             */
#define GRTFLAG1_SOCKINIT   0x80    /* ... sockets initialized              */
#define GRTFLAG1_TSO        0x40    /* ... TSO environment                  */

    char        unused1;            /* 0B unused                            */
    unsigned    grttmpnm;           /* 0C tmpnam() counter                  */
    void        **grtexit;          /* 10 atexit() array of functions       */
    void        **grtexita;         /* 14 on_exit() array of args           */
    void        **grtfile;          /* 18 open FILE handle array            */
    void        **grtsock;          /* 1C open sockets array                */
    void        **grtenv;           /* 20 environment variables array       */
    void        *grtin;             /* 24 stdin file handle                 */
    void        *grtout;            /* 28 stdout file handle                */
    void        *grterr;            /* 2C stderr file handle                */
    void        *grtcom;            /* 30 console communication             */
    void        *grtapp1;           /* 34 for use by applications           */
    void        *grtapp2;           /* 38 for use by applications           */
    void        *grtapp3;           /* 3C for use by applications           */
    void        *grtcthrd;          /* 40 CTHDTASK array                    */
    void        **grtwsa;           /* 44 writable static array             */
    void        **grtdevtb;         /* 48 device array                      */
    void        **grtptrs;          /* 4C pointers passed in via argv       */
                                    /* ... the words at R1: up to the VL    */
                                    /* ... bit, or the 4 words of a CPPL.   */
                                    /* ... Any other list gets 10 words,    */
                                    /* ... valid only within the caller's   */
                                    /* ... own list (#218).                 */
};                                  /* 50 (80 bytes)                        */

extern CLIBGRT  *__grtget(void);
extern int      __grtres(void);
extern int      __grtset(void);

/* ---- 1.x clibcrt.h --------------------------------------------------- */
typedef struct clibcrt  CLIBCRT;    /* per thread runtime work area         */

#include "libc370/array.h"                /* dynamic array                        */

/* This structure holds data unique to each task/thread (TCB)
**
** When a task is created, a CLIBCRT is allocated and saved
** in the task CLIBPPA->PPACRT dynamic array. This occurs for both the main
** task via the startup code in @@CRT0.ASM and in the startup code in
** CTHREAD entry point.
**
** The CLIBPPA is found by calling the @@PPAGET entry point in @@CRT0.
**
** The CRTGRT field below is a pointer to the process (main) structure
** that holds data unique to the process regardless of how many
** task/threads are in use (global data).
*/
struct clibcrt {
    char        crteye[8];          /* 00 Eye catcher for dumps "CLIBCRT "  */
    void        *crttcb;            /* 08 Owning TCB                        */
    void        *crtsave;           /* 0C first save area address           */
    void        *crtacee;           /* 10 ACEE for this task/thread         */
    unsigned    crtseed;            /* 14 seed for rand()/srand()           */
    char        crtctime[28];       /* 18 result for asctime()/ctime()      */
    int         crttzoff;           /* 34 time zone offset                  */
    void        *crtstime;          /* 38 STIMER exit plist                 */
    unsigned    crtuserl;           /* 3C length of CRTUSER area            */
    unsigned    crthoste[10];       /* 40 hostent areas for DYN75           */
    char        crthostn[80];       /* 68 host name for DYN75               */
    int         crtestct;           /* B8 ESTAE stack count                 */
    unsigned    crtestpl[10*2];     /* BC ESTAE parameter list              */
    char        crtopts;            /* 10C copy of original JFCBOPTS byte   */
#define CRTOPTS_AUTH        0x01    /* ... JFCBAUTH bit for APF authorized  */
    char        crtauth;            /* 10D authorization flags              */
#define CRTAUTH_ON          0x80    /* ... task auth via __autask()         */
#define CRTAUTH_STEPLIB     0x40    /* ... steplib auth via __austep()      */
	char 		crtflag;			/* 10E processing flag(s)				*/
#define CRTFLAG_TSO			0x80	/* ... TSO environment					*/
#define CRTFLAG_TSOB		0x40	/* ... TSO background environment		*/
#define CRTFLAG_TMRFAIL		0x20	/* ... STIMER failure reported (#94)	*/
#define CRTFLAG_TIN			0x08	/* ... STDIN is terminal				*/
#define CRTFLAG_TOUT		0x04	/* ... STDOUT is terminal				*/
#define CRTFLAG_TERR		0x02	/* ... STDERR is terminal				*/
    char        unused;             /* 10F unused                           */
    int         crterrno;           /* 110 error number                     */
    char        *crtstrtk;          /* 114 used by strtok() for "old" ptr   */
    CLIBGRT     *crtgrt;            /* 118 process level C runtime anchor   */
    char        crttmpnm[12];       /* 11C tmpnam() buffer                  */
    char        crttms[4*9];        /* 128 struct tm for gmtime()           */
    void        **crtpush;          /* 14C push/pop function array          */
    void        **crtargs;          /* 150 push/pop arg array               */
    void        **crtmutx;          /* 154 mutex cleanup array              */
    void		*crtapp1;			/* 158 application use #1				*/
    void		*crtapp2;			/* 15C application use #2				*/
    void 		*crtufs;			/* 160 Unix "like" File System			*/
    unsigned    unused2;			/* 164 unused/available					*/
    char        crtntoa[16];        /* 168 "nnn.nnn.nnn.nnn" xxxx_ntoa()    */
    unsigned    crttryrc;			/* 178 return/abend code from try()     */
    unsigned    crtavail[3];		/* 17C unused/available					*/
};                                  /* 188 (392 bytes)                      */

extern CLIBCRT  *__crtget(void);
extern int      __crtset(void);
extern int      __crtres(void);

/* runtime exit: run the atexit()/on_exit() functions, close open files and
   release the runtime's storage, then terminate the task via @@EXITA.  This is
   what exit() calls.  Deliberately not declared __attribute__((noreturn)) --
   control does not come back, but the definition in @@exit.c ends in a plain
   return, so the attribute would be a promise the code does not make. */
extern void     __exit(int status);

/* ---- 1.x clibppa.h --------------------------------------------------- */
typedef struct clibppa  CLIBPPA;

struct clibppa {
    char    ppaeye[4];          /* 00 Eye Catcher                       */
#define PPAEYE  "@PPA"          /* ...                                  */
    void    *ppaprev;           /* 04 Previous Save Area                */
    void    *ppanext;           /* 08 Next Save Area                    */
    CLIBCRT **ppacrt;           /* 0C C Runtime Library Anchor (Task)   */
    CLIBGRT *ppagrt;            /* 10 C Runtime Global Anchor (AS)      */
    void    *ppasave;           /* 14 saved "next" from TCBFSA          */
    void    *ppatiot;           /* 18 TIOT address                      */
    void    *ppapscb;           /* 1C PSCB address                      */
    char 	ppaflag;			/* 20 Processing flag(s)				*/
#define PPAFLAG_TSOFG	0x80	/* ... TSO environment					*/
#define PPAFLAG_TSOBG	0x40	/* ... TSO background environment		*/
#define PPAFLAG_TIN		0x08	/* ... STDIN is terminal				*/
#define PPAFLAG_TOUT	0x04	/* ... STDOUT is terminal				*/
#define PPAFLAG_TERR	0x02	/* ... STDERR is terminal				*/
    char    ppasubpl;			/* 21 save area subpool number          */
	char 	ppaheaps;			/* 22 heap (malloc) subpool number		*/
	char 	unused[1];			/* 23 unused/available					*/
	unsigned ppastkln;			/* 24 stack area length 				*/
	void	*ppaexita;			/* 28 EXITA entry point address			*/
	void    *ppacppl;			/* 2C TSO CPPL							*/
};								/* 30 (48 bbytes)						*/

CLIBPPA * __PPAGET(void);
CLIBPPA * __ppaget(void);

/* __ppahrv() - free what a dead program's own exit path would have
   freed: its open FILEs (fclose, DCBs are still open under this TCB),
   its CLIBGRT with the registration arrays (atexit functions are NOT
   run), and its CLIBCRTs.  Called by try()'s abend-path walk for each
   validated abandoned PPA before the stack+PPA block is freed (#96).
   Everything is validated before it is trusted; what does not
   validate is left alone. */
void __ppahrv(CLIBPPA *ppa);

#endif /* MVS_CRT_H */
