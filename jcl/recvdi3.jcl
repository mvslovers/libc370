//TSTRCVR  JOB (SYS),'RECV TSTDI3',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #187 long long probe into a scratch PDS.  RECEIVE cannot
//* merge into an existing PDS, so the old copy goes first; tstdi3.jcl
//* STEPLIBs straight at the result.
//*
//* Upload test/mvs/tstdi3.c built as
//*     make build
//*     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstdi3.c -o TSTDI3 \
//*           -flinker-output=iebcopy
//*     ld370 --pack TSTDI3=TSTDI3.iebcopy -o tstdi3 -xmit \
//*           --dsn IBMUSER.LIBC370.DI3SCR
//* to IBMUSER.MBT.XMIT.IN first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.DI3SCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.MBT.XMIT.IN')
/*
//
