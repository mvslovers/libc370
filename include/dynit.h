#ifndef DYNIT_H
#define DYNIT_H
#include <sys/_cc370.h>
/* dynit.h - dynamic allocation through a structure: the interface of the
**           z/OS XL C/C++ run-time library, so that a program written for
**           it compiles here unchanged (#52).
**
** dyninit() clears a __dyn_t, the program fills in the fields it needs,
** dynalloc() turns them into SVC 99 text units and allocates, dynfree()
** unallocates.  The names, types and constants are z/OS's; the header sits
** at the top of include/ because that is where z/OS has it (naming rule 3).
**
** MVS 3.8j has no SMS, no UNIX file system and no request block extension.
** What a field asks for that 3.8j cannot do is either ignored or refused
** before SVC 99 is issued; the Library Reference has the table.  The layout
** of __dyn_t is not z/OS's: a program that compiles against it is portable,
** a binary image of the structure is not.
*/

#include <string.h>

/* the request block extension and the message parameter list of later MVS
   levels.  MVS 3.8j has neither, so the types stay incomplete: a pointer
   to one can be declared and set to NULL, an object cannot. */
typedef struct __S99rbx     __S99rbx_t;
typedef struct __S99emparms __S99emparms_t;

typedef struct __DYNstruct {
    char            *__ddname;      /* DDNAME; "????????": generated     */
    char            *__dsname;      /* data set name                     */
    char            __sysout;       /* SYSOUT class or __DEF_CLASS       */
    char            *__sysoutname;  /* SYSOUT program (writer) name      */
    char            *__member;      /* member of a PDS                   */
    char            __status;       /* __DISP_OLD, _MOD, _NEW, _SHR      */
    char            __normdisp;     /* normal disposition                */
    char            __conddisp;     /* conditional (abnormal) disp.      */
    char            *__unit;        /* unit name                         */
    char            *__volser;      /* volume serial                     */
    short           __dsorg;        /* __DSORG_...                       */
    char            __alcunit;      /* __CYL or __TRK                    */
    int             __primary;      /* primary space quantity            */
    int             __secondary;    /* secondary space quantity          */
    short           __recfm;        /* _F_, _V_, _FB_ + _A_, ...         */
    short           __blksize;      /* block size                        */
    unsigned short  __lrecl;        /* logical record length             */
    char            *__volrefds;    /* VOL=REF= data set                 */
    char            *__dcbrefds;    /* DCB= model data set               */
    char            *__dcbrefdd;    /* DCB= model DD                     */
    unsigned char   __misc_flags;   /* __CLOSE | __RELEASE | ...         */
    char            *__password;    /* data set password                 */
    char            **__miscitems;  /* further text units, last X'80'    */
    short           __infocode;     /* out: SVC 99 information code      */
    short           __errcode;      /* out: SVC 99 error code            */
    int             __dirblk;       /* directory blocks                  */
    int             __avgblk;       /* block length for block space      */
    char            *__storclass;   /* SMS storage class: ignored        */
    char            *__mgntclass;   /* SMS management class: ignored     */
    char            *__dataclass;   /* SMS data class: refused           */
    unsigned char   __recorg;       /* VSAM record organization: refused */
    short           __keylength;    /* key length                        */
    short           __keyoffset;    /* VSAM key offset: refused          */
    char            *__refdd;       /* REFDD=: refused                   */
    char            *__like;        /* LIKE=: refused                    */
    unsigned char   __dsntype;      /* DSNTYPE: only __DSNT_PDS          */
    char            *__pathname;    /* UNIX path: refused                */
    int             __pathopts;     /* path options: ignored             */
    int             __pathmode;     /* path mode: ignored                */
    unsigned char   __pathndisp;    /* path normal disp.: ignored        */
    unsigned char   __pathcdisp;    /* path cond. disp.: ignored         */
    __S99rbx_t      *__rbx;         /* request block extension: ignored  */
    __S99emparms_t  *__emsgparmlist;/* message parameters: ignored       */
    char            __rls;          /* VSAM RLS: refused                 */
    /* LIBC/370 extension: what dynalloc() got back from MVS */
    char            __retddn[9];    /* the DD name MVS generated         */
    char            __retdsn[45];   /* the data set name MVS generated   */
} __dyn_t;

/* dyninit() - clear *dynp for a new request; evaluates to 0 */
#define dyninit(dynp)   (memset((dynp), 0, sizeof(__dyn_t)), 0)

/* dynalloc() - allocate as *dyn_parms says (SVC 99, verb 1).  0 when
   allocated, -1 when the request did not fit the library's work area,
   -2 when a field holds something it cannot pass on (errno EINVAL), else
   the SVC 99 return code with __errcode and __infocode set */
int dynalloc(__dyn_t *dyn_parms);

/* dynfree() - unallocate by __ddname, or by __dsname (and __member), with
   __normdisp as the overriding disposition (SVC 99, verb 2).  Returns as
   dynalloc() */
int dynfree(__dyn_t *dyn_parms);

/* __sysout: the default class, SYSOUT=* */
#define __DEF_CLASS     '-'

/* __alcunit */
#define __CYL           '\x01'
#define __TRK           '\x02'

/* __status */
#define __DISP_OLD      0x01
#define __DISP_MOD      0x02
#define __DISP_NEW      0x04
#define __DISP_SHR      0x08

/* __normdisp, __conddisp */
#define __DISP_UNCATLG  0x01
#define __DISP_CATLG    0x02
#define __DISP_DELETE   0x04
#define __DISP_KEEP     0x08

/* __dsorg */
#define __DSORG_unknown 0x0000
#define __DSORG_VSAM    0x0008
#define __DSORG_GS      0x0080
#define __DSORG_PO      0x0200
#define __DSORG_POU     0x0300
#define __DSORG_DA      0x2000
#define __DSORG_DAU     0x2100
#define __DSORG_PS      0x4000
#define __DSORG_PSU     0x4100

/* __recfm, added together: _FB_ + _A_ is FBA */
#define _M_             0x02
#define _A_             0x04
#define _S_             0x08
#define _B_             0x10
#define _D_             0x20
#define _V_             0x40
#define _F_             0x80
#define _U_             0xC0
#define _FB_            0x90
#define _VB_            0x50
#define _FBS_           0x98
#define _VBS_           0x58

/* __misc_flags */
#define __CLOSE         0x01    /* free the DD when the file is closed  */
#define __RELEASE       0x02    /* release unused space at close        */
#define __PERM          0x04    /* permanently allocated attribute      */
#define __CONTIG        0x08    /* contiguous space                     */
#define __ROUND         0x10    /* whole cylinders for block space      */
#define __TERM          0x20    /* the TSO terminal                     */
#define __DUMMY_DSN     0x40    /* DUMMY                                */
#define __HOLDQ         0x80    /* SYSOUT to the hold queue             */

/* __recorg (VSAM; refused on MVS 3.8j) */
#define __KS            0x80
#define __ES            0x40
#define __RR            0x20
#define __LS            0x10

/* __dsntype (SMS; only __DSNT_PDS is accepted, as the PDS it always is) */
#define __DSNT_LIBRARY  0x80
#define __DSNT_PDS      0x40
#define __DSNT_HFS      0x20
#define __DSNT_PIPE     0x10
#define __DSNT_LARGE    0x08
#define __DSNT_EXTREQ   0x04
#define __DSNT_EXTPREF  0x02
#define __DSNT_BASIC    0x01

#endif /* DYNIT_H */
