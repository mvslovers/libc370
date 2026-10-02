#ifndef MVS_PDS_H
#define MVS_PDS_H
#include <sys/_cc370.h>
/* mvs/pds.h - PDS directory: BLDL and STOW.
**
** libc370 2.0 splits this out of clibos.h (#256).
*/

typedef struct bldl         BLDL;       /* BLDL list                    */
typedef struct de12         DE12;       /* minimum dir entry            */
typedef struct de14         DE14;       /* a little more dir entry info */
typedef struct de76         DE76;       /* dir entry with user data     */

struct de12 {
    char            name[8];            /* name, space filled           */
    char            ttr[3];             /* ttr                          */
    char            k;                  /* concatenation number         */
} __attribute__((packed));

struct de14 {
    char            name[8];            /* name, space filled           */
    char            ttr[3];             /* ttr                          */
    char            k;                  /* concatenation number         */
    char            z;                  /* where found                  */
#define DE_PRIVATE  0                   /* ... private library          */
#define DE_LINK     1                   /* ... link library             */
#define DE_JOB      2                   /* ... job, task or step library*/
/* > 2 job, step or library of parents task n, where n = z - 2 */
    char            c;                  /* type, ttrn's and udata length*/
#define DE_ALIAS    0x80                /* name is an alias             */
#define DE_TTRNS    0x60                /* ttrns, 0x20=1, 0x40=2, 0x60=3*/
#define DE_UDATA    0x1F                /* #udata in half words         */
} __attribute__((packed));

struct de76 {
    char            name[8];            /* name, space filled           */
    char            ttr[3];             /* ttr                          */
    char            k;                  /* concatenation number         */
    char            z;                  /* where found                  */
    char            c;                  /* type, ttrn's and udata length*/
    char            udata[62];          /* user data                    */
} __attribute__((packed));

struct bldl {
    short           ff;                 /* number of DEnn structs       */
    short           ll;                 /* length of each DEnn struct   */
    union {
        DE12        de12[1];            /* 12 byte dir entry            */
        DE14        de14[1];            /* 14 byte dir entry            */
        DE76        de76[1];            /* 76 byte dir entry            */
    };
} __attribute__((packed));

/* __bldl() search for member in dcb or default link libraries when dcb is NULL */
int __bldl(BLDL *bldl, void *dcb);

/* __stow() maintain a PDS directory entry via STOW on an open DSORG=PO DCB.
 * func is 'A' add, 'R' replace, 'D' delete or 'C' change (rename).
 * Returns the STOW return code (0 on success) or -1 for an unknown function. */
int __stow(void *dcb, void *area, int func);

/* ---- from 1.x clibio.h -------------------------------------------------- */
/* __renmem() rename PDS member oldmem to newmem in dsn via STOW change.
 * Returns 0 on success, a positive STOW return code (8=old not found,
 * 4=new already exists, ...), or a negative value for allocation/open
 * failures.  Unlike rename(), which uses IDCAMS ALTER on a cataloged data
 * set, this renames a member of a partitioned data set. */
extern int __renmem(const char *dsn, const char *oldmem, const char *newmem);

/* __delmem() delete PDS member mem from dsn via STOW delete, under a
 * DISP=SHR allocation - never exclusive, see #127 / mvslovers/mvsmf#342.
 * Returns 0 on success, a positive STOW return code (8=member not found,
 * ...), or a negative value for allocation/open failures.  remove() routes
 * dsn(member) names here; IDCAMS DELETE stays for whole data sets. */
extern int __delmem(const char *dsn, const char *mem);

#endif /* MVS_PDS_H */
