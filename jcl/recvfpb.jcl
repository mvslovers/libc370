//TSTRCVF  JOB (SYS),'RECV TSTFPBIG',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #385 fprintf probe into a scratch PDS.  RECEIVE cannot merge
//* into an existing PDS, so the old copy goes first; tstfpbig.jcl
//* STEPLIBs straight at the result.
//*
//* The staging data set is this probe's own, IBMUSER.LIBC370.FPBXMIT,
//* not IBMUSER.MBT.XMIT.IN: `make deploy` in any project stages there
//* too, and a RECEIVE that runs while another deploy is in flight takes
//* whichever XMIT is there at the time.
//*
//* Build and pack as test/mvs/tstfpbig.c says, then upload tstfpbig.xmit
//* to IBMUSER.LIBC370.FPBXMIT (FB 80, created first if it is gone -
//* RECEIVE may delete it).
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.FPBSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.FPBXMIT')
/*
//
