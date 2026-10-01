#ifndef MVS_IDCAMS_H
#define MVS_IDCAMS_H
/* mvs/idcams.h - run IDCAMS commands.
**
** libc370 2.0 splits this out of mvssupa.h (#256).
*/

#include <stddef.h>

#pragma linkage(__idcams, OS)
int __idcams(size_t len, char *data);   /* non-reentrant assembler subroutine, Yick! */

int idcams(const char *fmt, ...);   /* reentrant C function, LINKs to IDCAMS external program */

/* idcams_sysprint() - idcams(), and every line IDCAMS prints handed to fn.
**
** idcams() returns IDCAMS's highest condition code and nothing else: 8 is
** "entry not found" and "refused" alike.  The text that tells them apart is
** what IDCAMS prints, and each line comes with its message number.  This
** runs the same commands and calls
**
**     fn(arg, msgno, text, len)
**
** once for every SYSPRINT line, in the order IDCAMS writes them:
**
**   msgno  the IDCnnnnI number the line carries -- 3012 for IDC3012I, 2 for
**          IDC0002I -- or 0 for a line that is no message (page heading,
**          blank line, the echoed command, LISTCAT's counts).  libc370 reads
**          it from the text: the number IDCAMS hands its exit drops the
**          leading digit (IDC3012I arrives as 12) and turns the two
**          summaries into -1 and -2 (measured, #71).
**   text   the record as IDCAMS writes it: one carriage-control byte ('1'
**          new page, '0' a blank line first, ' ' next line; x'00' was seen
**          on IDC3007I/IDC3009I), then the text -- a message starts
**          "IDCnnnnI" at text[1].  len bytes, NOT NUL-terminated, valid only
**          during the call: copy what you keep.
**   arg    passed through untouched
**
** The return value is idcams()'s: the highest condition code, or a negative
** value when IDCAMS could not be LINKed.
**
** libc370 picks no message for you.  IDCAMS ends every command with
** IDC0001I FUNCTION COMPLETED ... and the run with IDC0002I IDCAMS
** PROCESSING COMPLETE ..., so "the last message" is one of those.  The
** first other message is the one that explains a condition code -- on MVS
** 3.8j: DELETE or ALTER of a missing entry, IDC3012I (rc 8); ALTER NEWNAME
** with a qualifier of nine characters, IDC3203I (rc 12).
**
** fn runs inside IDCAMS's output exit, on the stack libc370 lends that exit
** (8000 bytes for the exit and everything it calls): keep it short -- copy,
** count, set a flag -- and do not call idcams() from it.  Nothing is shared
** between calls, so concurrent threads may each run their own.
**
** Example -- keep the first message that is not a summary:
**
**     static void first(void *arg, int msgno, const char *text, int len)
**     {
**         int *keep = arg;
**         if (!*keep && msgno && msgno != 1 && msgno != 2)
**             *keep = msgno;
**     }
**     ...
**     int msgno = 0;
**     rc = idcams_sysprint(first, &msgno, " DELETE '%s'", dsn);
**     if (rc) printf("IDC%04dI, condition code %d\n", msgno, rc);
*/
typedef void (*IDCAMS_SYSPRINT)(void *arg, int msgno, const char *text, int len);
int idcams_sysprint(IDCAMS_SYSPRINT fn, void *arg, const char *fmt, ...) asm("IDCSYSPR");

#endif /* MVS_IDCAMS_H */
