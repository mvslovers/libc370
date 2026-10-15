//TSTDDDCB JOB (SYS),'LIBC370 554',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #554 - recfm=, lrecl=, blksize= in the mode of a DD: open
//* take precedence over the DD and the DSCB.  See tstdddcb.c.
//* STEPLIB is the scratch PDS recvddd.jcl restores into.  GREEN:
//* this tree's libc.a; RED: the installed one.  Temporary data sets.
//*
//* Run:     mvsdev JOB02380, 2026-10-15: GREEN CC 0000, 10/10;
//*          RED CC 0008, 8 failed (the options are ignored).
//*
//* RC 0 = every check passed, 8 = a check failed.
//*
//GREEN    EXEC PGM=TSTDDDCB,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DDDSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//OUTA     DD  DSN=&&OUTA,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//OUTB     DD  DSN=&&OUTB,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=1600)
//OUTC     DD  DSN=&&OUTC,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),
//             DCB=(BLKSIZE=400)
//OUTD     DD  DSN=&&OUTD,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//OUTE     DD  DSN=&&OUTE,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//OUTF     DD  DSN=&&OUTF,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=1600)
//*
//RED      EXEC PGM=TSTDDDCR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DDDSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//OUTA     DD  DSN=&&OUTA,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//OUTB     DD  DSN=&&OUTB,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=1600)
//OUTC     DD  DSN=&&OUTC,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),
//             DCB=(BLKSIZE=400)
//OUTD     DD  DSN=&&OUTD,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//OUTE     DD  DSN=&&OUTE,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1))
//OUTF     DD  DSN=&&OUTF,DISP=(NEW,DELETE),UNIT=SYSDA,
//             SPACE=(TRK,(1,1)),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=1600)
