//TSTSYSIN JOB (SYS),'LIBC370 184',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #184 - a second concurrent open of a spool SYSIN.
//*
//* @@start opens dd:SYSIN as stdin, so a user fopen("dd:SYSIN","r") is
//* always a SECOND open.  On an instream DD JES2 refuses that outright
//* (HASPSSSM HO300: already open and not XBM means HOERR) and the S013-C0
//* follows.  libc370 now refuses it at the call instead: NULL + EBUSY.
//*
//* TWO STEPS, one member, and BOTH are the test:
//*   SPOOL  SYSIN DD *         -> the refusal, and the reopen-after-close
//*   REALDS SYSIN DD DSN=...   -> a real data set must STILL open twice
//* The member reads the TIOT to find out which it is in.  Running only
//* one step tests half the fix, and the half it skips is the regression.
//*
//* See test/mvs/tstsysin.c.  MVS only - the subject is a JES2 spool
//* data set, which a host has no equivalent of.
//*
//* Build:   cc370 -O1 -Iinclude -L build/sdk test/mvs/tstsysin.c \
//*                -o TSTSYSIN -flinker-output=iebcopy
//*          ld370 --pack TSTSYSIN=TSTSYSIN.iebcopy -o tstsysin -xmit \
//*                --dsn IBMUSER.LIBC370.TEST.LINKLIB
//* Install: RECEIVE the XMIT into the STEPLIB below.
//*
//* REALDS needs an FB80 data set with at least one record:
//*   IBMUSER.LIBC370.SIDATA
//*
//* Run:     mvsdev JOB00427, CC 0000 - 9/9 SPOOL, 7/7 REALDS.  Proven
//*          red by linking the same source against the pre-fix libc:
//*          IEC141I 013-C0 in the SPOOL step (JOB00428).
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//SPOOL    EXEC PGM=TSTSYSIN,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSIN    DD  *
ONE LINE OF DATA
/*
//*
//REALDS   EXEC PGM=TSTSYSIN,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSIN    DD  DISP=SHR,DSN=IBMUSER.LIBC370.SIDATA
