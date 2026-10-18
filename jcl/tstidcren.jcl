//TSTIDCRN JOB (SYS),'LIBC370 590',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #590 - rename() to a name past column 72 of IDCAMS ALTER.
//* See test/mvs/tstidcren.c.  TSTIDCRN: this tree (prefix
//* IBMUSER.LIBC370.RN); TSTIDCRR: the installed library (prefix
//* IBMUSER.LIBC370.RR), the red.  Each step removes what it created.
//* Run:     mvsdev JOB02454, 2026-10-18: green 3/3; red rc 0 with
//*          the cut name catalogued in all three cases.
//*
//NREN     EXEC PGM=TSTIDCRN,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ICSCR
//SYSPRINT DD  SYSOUT=*
//RREN     EXEC PGM=TSTIDCRR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ICSCR
//SYSPRINT DD  SYSOUT=*
//
