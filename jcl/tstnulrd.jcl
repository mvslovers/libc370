//TSTNULRD JOB (SYS),'LIBC370 454',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #454 - a text-mode read keeps a X'00' inside a record.  See
//* test/mvs/tstnulrd.c.  STEPLIB is the scratch PDS recvnrd.jcl
//* restores into.  TXT is a temporary FB 80 data set per step.
//*
//* GREEN is this tree's libc.a (CC 0000 expected), RED the installed
//* one (CC 0001 expected: R1 and R3 cut at the first X'00').
//*
//* Run:     mvsdev JOB01542, 2026-10-06: GREEN CC 0000, 13/13; RED CC 0001
//*          (R1 1 byte, R3 0 bytes).
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTNRD,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.NRDSCR
//TXT      DD  DSN=&&TXTG,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTNRDR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.NRDSCR
//TXT      DD  DSN=&&TXTR,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
