//TSTGMAIN JOB (SYS),'LIBC370 283',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #283/#401 - getmain()/freemain() without printf, and the
//* 24-bit length check.  See test/mvs/tstgmain.c.  TSTGMAIN: this
//* tree; TSTGMR: the installed library - console lines alike, case 4
//* red.
//* Run:     mvsdev JOB02444, 2026-10-18 (as TSTGETM/TSTGETMR): green;
//*          red: freemain dumped 64 bytes, case 4 ended S0C4.
//*
//NGETM    EXEC PGM=TSTGMAIN,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.GMSCR
//SYSPRINT DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//RGETM    EXEC PGM=TSTGMR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.GMSCR
//SYSPRINT DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
