//TSTGETL  JOB (SYS),'LIBC370 467',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #467 - fopen("*GETLINE","r") under a batch TMP.  See
//* test/mvs/tstgetl.c.  The modules sit in the scratch PDS recvget.jcl
//* restores into.
//*
//* TMP: TSTGET reads the two lines after its CALL (they must not run as
//* commands; LISTALC after them must); TSTGET 'ALL' reads the rest of
//* SYSTSIN to EOF, and the TMP must end cleanly.  TMPR: the same with
//* the installed library, the red control.  NOTMP/NOTMPR: plain batch,
//* the open must fail with ENODEV.
//*
//* Run:     mvsdev JOB01626, 2026-10-07: as in test/mvs/tstgetl.c; NOTMPR
//*          CC 0001, the rest CC 0000.
//*
//TMP      EXEC PGM=IKJEFT01,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.GETSCR
//SYSTSPRT DD  SYSOUT=*
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSTSIN  DD  *
CALL 'IBMUSER.LIBC370.GETSCR(TSTGET)'
first line read by getline
   second line, leading and trailing blanks
LISTALC
CALL 'IBMUSER.LIBC370.GETSCR(TSTGET)' 'ALL'
remaining line A
remaining line B
/*
//TMPR     EXEC PGM=IKJEFT01,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.GETSCR
//SYSTSPRT DD  SYSOUT=*
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSTSIN  DD  *
CALL 'IBMUSER.LIBC370.GETSCR(TSTGETR)'
first line read by getline
   second line, leading and trailing blanks
LISTALC
/*
//NOTMP    EXEC PGM=TSTGET,PARM='NOTMP',REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.GETSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//NOTMPR   EXEC PGM=TSTGETR,PARM='NOTMP',REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.GETSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
