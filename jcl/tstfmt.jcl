//TSTFMT   JOB (SYS),'LIBC370 F2',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 size analysis F2 - the formatter takes its output functions
//* as pointers.  See test/mvs/tstfmt.c.  FMTN: this tree's libc.a;
//* FMTR: the installed one - SYSPRINT and OUT must be identical.
//* Run:     mvsdev JOB02408/02410, 2026-10-17: identical, checks pass.
//*
//NFMT     EXEC PGM=FMTN,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FMSCR
//SYSPRINT DD  SYSOUT=*
//OUT      DD  SYSOUT=*
//IN       DD  *
X
/*
//RFMT     EXEC PGM=FMTR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FMSCR
//SYSPRINT DD  SYSOUT=*
//OUT      DD  SYSOUT=*
//IN       DD  *
X
/*
//
