//TSTLSTDS JOB (SYS),'LIBC370 50 CATNM',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #50 - DSLIST.catnm through the real __listds(), measured.
//*
//* The host test feeds parse() LISTCAT lines as SYSPRINT showed them.
//* This job checks the records __listc() reads back from its OUTFILE:
//* every record named, one shared name per level, __freeds() clean.
//* Read only - LISTCAT and OBTAIN, nothing is written.
//*
//* PARM lists the levels (default IBMUSER SYS2).
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvlstd.jcl restores into.
//*
//S1       EXEC PGM=TSTLSTDS,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.LSTDSSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
