#ifndef SRC_INTERNAL_IDCREC_H
#define SRC_INTERNAL_IDCREC_H
/* src/internal/idcrec.h - internal: IDCAMS input records from a command
** string (#590).  See src/mvs/idcams/@@idcrec.c. */

/* the cursor through a command string; set next to the string and cont to
   0 before the first call */
typedef struct idcrec {
    const char  *next;      /* the rest of the command, NULL at the end */
    int         cont;       /* next is in the middle of a line */
} IDCREC;

#define IDCREC_MAX  72      /* IDCAMS reads columns 2-72 */

/* the next input record into rec (IDCREC_MAX + 1 bytes): its length, -1
   at the end, -2 when a line cannot be broken before column 72 */
extern int __idcrec(IDCREC *c, char *rec);

/* 0 when every record of cmds fits, -2 when one does not */
extern int __idcchk(const char *cmds);

#endif /* SRC_INTERNAL_IDCREC_H */
