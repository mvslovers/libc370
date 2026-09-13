//TSTRCVR  JOB (SYS),'RECV TSTFPRLS',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #167 RLSE probe into a scratch PDS.  RECEIVE cannot merge
//* into an existing PDS, and it leaves a usable load library behind, so
//* tstfprls.jcl STEPLIBs straight at the scratch data set rather than
//* IEBCOPYing into the shared probe LINKLIB (which is out of extents).
//*
//* Upload test/mvs/tstfprls.c built as
//*     cc370 -O1 -Iinclude test/mvs/tstfprls.c -o TSTFPRLS \
//*           -flinker-output=xmit
//* to IBMUSER.MBT.XMIT.IN first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.RLSSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.MBT.XMIT.IN')
/*
//
