#ifndef STDIO_H
#define STDIO_H
#include <sys/_cc370.h>

#include <stdarg.h>
#include <stddef.h>

typedef struct _file    FILE;       /* file handle                          */
typedef struct _file    _FILE;      /* file handle                          */

struct _file {
    char            eye[8];         /* 00 eye catcher for dumps             */
#define _FILE_EYE   "F I L E"       /* ...                                  */
    void            *dcb;           /* 08 DCB address                       */
    void            *asmbuf;        /* 0C buffer for assembler routines     */

    unsigned short  lrecl;          /* 10 logical record length             */
    unsigned short  blksize;        /* 12 physical block size               */
    int             ungetch;        /* 14 unget character                   */
    unsigned        filepos;        /* 18 character offset in file          */
    unsigned char   *buf;           /* 1C file buffer (lrecl+8)             */

    unsigned char   *upto;          /* 20 current position in buffer        */
    unsigned char   *endbuf;        /* 24 end of buffer                     */
    unsigned short  flags;          /* 28 processing flags                  */
#define _FILE_FLAG_DYNAMIC  0x8000  /* ... DD dynamically allocated         */
#define _FILE_FLAG_OPEN     0x4000  /* ... dataset is open                  */
#define _FILE_FLAG_READ     0x2000  /* ... open for read                    */
#define _FILE_FLAG_WRITE    0x1000  /* ... open for write                   */
#define _FILE_FLAG_APPEND   0x0800  /* ... open for append                  */
#define _FILE_FLAG_BINARY   0x0400  /* ... file data is binary              */
#define _FILE_FLAG_RECORD   0x0200  /* ... fread/fwrite uses record i/o     */
/* Record mode (",record" in the fopen() mode): one fread()/fwrite() is one
   record.  On RECFM=V the record carries its RDW both ways - bytes 0-1 the
   length including the RDW, bytes 2-3 zero - as rread()/rwrite() do.
   fwrite() of size*nmemb bytes refuses with EINVAL, writing nothing and
   leaving the stream usable, what exceeds LRECL (LRECL-4 spanned) or, on V,
   an RDW that is short or does not match; size or nmemb 0 writes nothing
   (#236). */
#define _FILE_FLAG_BSAM     0x0100  /* ... BSAM instead of QSAM access      */
#define _FILE_FLAG_TERM		0x0080	/* ... TERM opened in __fpstar()		*/
#define _FILE_FLAG_RLSE     0x0040  /* ... release unused space at CLOSE    */

/* '+' streams (#189).  A stream opened "r+", "w+" or "a+" has both
   _FILE_FLAG_READ and _FILE_FLAG_WRITE set: those say what the caller may
   do.  Its DCB is still opened one way at a time, and _FILE_FLAG_DCBOUT
   says which: set, the open DCB writes (OUTPUT or EXTEND); clear, it
   reads.  __fpswt() turns it round.  Plain "w" and "a" streams carry
   DCBOUT from the start, plain "r" never.
   _FILE_FLAG_EXTEND: any output open from here on is EXTEND, never OUTPUT
   - a direction switch must not truncate what "w+" already wrote.
   _FILE_FLAG_POSEND: "a+" - the position is the end of the data set, and
   its size has not been counted yet.  fopen() counts it before it returns;
   ftell()/fseek() would count it if anything else left it set. */
#define _FILE_FLAG_POSEND   0x0020  /* ... position = end, size not known   */
#define _FILE_FLAG_DCBOUT   0x0010  /* ... the open DCB writes              */
#define _FILE_FLAG_EXTEND   0x0008  /* ... reopen for output as EXTEND      */

/* _FILE_FLAG_ENOSPC rides along with _FILE_FLAG_ERROR and records WHICH
   error set it, because the FILE itself keeps no errno and @@AWRITE clears
   IOSFLAGS before it returns.  A fail-fast return (#149) needs it to say
   ENOSPC rather than leave a stale value standing.  There is no x37 on
   input, so it is only ever set on the write side.  Cleared wherever
   _FILE_FLAG_ERROR is cleared. */
#define _FILE_FLAG_ENOSPC   0x0004  /* ... the error was out of space       */
#define _FILE_FLAG_ERROR    0x0002  /* ... i/o error                        */
#define _FILE_FLAG_EOF      0x0001  /* ... EOF has occured                  */

    unsigned char   recfm;          /* 2A record format                     */
#define _FILE_RECFM_TYPE    0xC0    /* ... mask for F,V,U                   */
#define _FILE_RECFM_F       0x80    /* ... FIXED RECORD LENGTH              */
#define _FILE_RECFM_V       0x40    /* ... VARIABLE RECORD LENGTH           */
#define _FILE_RECFM_U       0xC0    /* ... UNDEFINED RECORD LENGTH          */

#define _FILE_RECFM_O       0x20    /* ... TRACK OVERFLOW                   */
#define _FILE_RECFM_B       0x10    /* ... BLOCKED RECORDS                  */
#define _FILE_RECFM_S       0x08    /* ... FOR FIXED LENGTH RECORD FORMAT -
                                           STANDARD BLOCKS.
                                           FOR VARIABLE LENGTH RECORD FORMAT -
                                           SPANNED RECORDS                  */
#define _FILE_RECFM_CC      0x06    /* ... mask for carriage control A,M    */
#define _FILE_RECFM_A       0x04    /* ... ASA CONTROL CHARACTER            */
#define _FILE_RECFM_M       0x02    /* ... MACHINE CONTROL CHARACTER        */

#define _FILE_RECFM_L       0x01    /* ... KEY LENGTH (KEYLEN) WAS SPECIFIED
                                           IN DCB MACRO INSTRUCTION         */

    char            ddname[9];      /* 2B DD name                           */
    char            member[9];      /* 34 member name                       */
    char            dataset[45];    /* 3D dataset name                      */
    char            mode[85];       /* 6A open mode string                  */
    unsigned char   xflags;         /* BF more flags - '+' streams (#189)   */
#define _FILE_XFLAG_UPDAT   0x01    /* ... the reading DCB is open UPDAT:   */
                                    /*     a write overwrites in place      */
#define _FILE_XFLAG_DIRTY   0x02    /* ... the record in buf was changed    */
                                    /*     and not yet handed to __awrite() */
};                                  /* C0 (192 bytes)                       */

typedef unsigned long fpos_t;

#define NULL            ((void *)0)
#define FILENAME_MAX    260
#define FOPEN_MAX       256
#define _IOFBF          1
#define _IOLBF          2
#define _IONBF          3

/* set it to maximum possible LRECL to simplify processing */
/* also add in room for a RDW and dword align it just to be
   on the safe side */
#define BUFSIZ 32768
#define EOF -1
#define L_tmpnam        FILENAME_MAX
#define TMP_MAX         25
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

extern FILE **__gtin(void);
extern FILE **__gtout(void);
extern FILE **__gterr(void);

#define stdin           (*(__gtin()))
#define stdout          (*(__gtout()))
#define stderr          (*(__gterr()))

extern int      printf(const char *format, ...);
extern int      vprintf(const char *format, va_list arg);
extern FILE     *fopen(const char *filename, const char *mode);
extern int      fclose(FILE *stream);
extern size_t   fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern size_t   fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int      fputc(int c, FILE *stream);
extern int      fputs(const char *s, FILE *stream);
extern int      fprintf(FILE *stream, const char *format, ...);
extern int      vfprintf(FILE *stream, const char *format, va_list arg);
extern int      remove(const char *filename);
extern int      rename(const char *old, const char *newnam);
extern int      sprintf(char *s, const char *format, ...);
extern int      snprintf(char *s, size_t n, const char *format, ...);
extern int      vsprintf(char *s, const char *format, va_list arg);
extern char     *fgets(char *s, int n, FILE *stream);
extern int      ungetc(int c, FILE *stream);
extern int      fgetc(FILE *stream);
extern int      fseek(FILE *stream, long int offset, int whence);
extern long int ftell(FILE *stream);
extern int      fsetpos(FILE *stream, const fpos_t *pos);
extern int      fgetpos(FILE *stream, fpos_t *pos);
extern void     rewind(FILE *stream);
extern void     clearerr(FILE *stream);
extern void     perror(const char *s);
extern int      setvbuf(FILE *stream, char *buf, int mode, size_t size);
extern void     setbuf(FILE *stream, char *buf);
extern FILE     *freopen(const char *filename, const char *mode, FILE *stream);
extern int      fflush(FILE *stream);
extern char     *tmpnam(char *s);
extern FILE     *tmpfile(void);
extern int      fscanf(FILE *stream, const char *format, ...);
extern int      scanf(const char *format, ...);
extern int      sscanf(const char *s, const char *format, ...);
extern int      vfscanf(FILE *stream, const char *format, va_list arg);
extern int      vscanf(const char *format, va_list arg);
extern int      vsscanf(const char *s, const char *format, va_list arg);
extern char     *gets(char *s);
extern int      puts(const char *s);
extern int      getchar(void);
extern int      putchar(int c);
extern int      getc(FILE *stream);
extern int      putc(int c, FILE *stream);
extern int      feof(FILE *stream);
extern int      ferror(FILE *stream);
extern int      vsnprintf(char *s, size_t n, const char *format, va_list arg);

#define getchar()       (getc(stdin))
#define putchar(c)      (putc((c), stdout))
#define getc(stream)    (fgetc((stream)))
#define putc(c, stream) (fputc((c), (stream)))
/* != 0, not the raw flag: the function forms return 1/0 and a caller who
   writes ferror(f) == 1 must not get a different answer for including
   <stdio.h> than for not including it (#149). */
#define feof(stream)    (((stream)->flags & _FILE_FLAG_EOF)   != 0)
#define ferror(stream)  (((stream)->flags & _FILE_FLAG_ERROR) != 0)

#if 0
#undef  stdin
#define stdin                   (*(stdio->___gtin()))
#undef  stdout
#define stdout                  (*(stdio->___gtout()))
#undef  stderr
#define stderr                  (*(stdio->___gterr()))
#endif // 0

#endif
