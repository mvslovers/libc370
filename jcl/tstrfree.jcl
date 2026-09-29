//TSTRFREE JOB (SYS),'LIBC370 229 231',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #229 - rclose() frees the DD ropen() allocated by name, and
//* so does ropen() when __aopen() fails after the allocation.
//*
//* The probe measures the step's DSAB chain around ropen()/rclose() by
//* data set name; see test/mvs/tstrfree.c for the checks.
//*
//* #231: ropen() of a quoted name without a member, checks (11)/(12).
//*
//*   RED    = step RED ends CC 0008: TSTRFRED is the same source linked
//*            against the installed sysroot.  Before #229 reached it,
//*            (5), (6), (8), (10) failed; with #229 but not #231 only
//*            (11) fails.
//*   GREEN  = step GREEN ends CC 0000, no FAIL line.
//*
//* Every line also goes to the console (grep the job log for TSTRFREE).
//*
//* Build against build/sdk, not the installed sysroot:
//*     make build
//*     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstrfree.c \
//*           -o TSTRFREE -flinker-output=iebcopy
//*     cc370 -O1 -Iinclude test/mvs/tstrfree.c \
//*           -o TSTRFRED -flinker-output=iebcopy
//*     ld370 --pack TSTRFREE=TSTRFREE.iebcopy TSTRFRED=TSTRFRED.iebcopy \
//*           -o probe -xmit --dsn IBMUSER.LIBC370.RFRSCR
//* and upload probe.xmit to IBMUSER.LIBC370.RFRXMIT - a private staging
//* data set, not the shared IBMUSER.MBT.XMIT.IN.  RECEIVE cannot merge,
//* so DEL scratches the load library first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.RFRSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.RFRXMIT')
/*
//*
//* The work data sets, in a step of their own so the probe step starts
//* with neither of them allocated.
//*
//SETUP    EXEC PGM=IEFBR14,COND=(0,NE,RECV)
//PS       DD  DSN=IBMUSER.TSTRFREE.PS,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1)),
//             DCB=(DSORG=PS,RECFM=FB,LRECL=80,BLKSIZE=800)
//PDS      DD  DSN=IBMUSER.TSTRFREE.PDS,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1)),
//             DCB=(DSORG=PO,RECFM=VB,LRECL=255,BLKSIZE=6233)
//*
//RED      EXEC PGM=TSTRFRED,REGION=4096K,COND=(0,NE,SETUP)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RFRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//GREEN    EXEC PGM=TSTRFREE,REGION=4096K,COND=((0,NE,SETUP),EVEN)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RFRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTRFREE.PS PURGE
  DELETE IBMUSER.TSTRFREE.PDS PURGE
  SET MAXCC=0
/*
//
