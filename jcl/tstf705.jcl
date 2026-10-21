//TSTF705 JOB (SYS),'LIBC370 705',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #705 - __fabandon() after a CLOSE that abends.  Measurement
//* only, see test/mvs/tstfab705.c.  One case per step, so each runs in
//* an address space of its own; each step runs two rounds.
//*
//*   C  fclose() under try() abends SB14 (full directory), then
//*      __fabandon() - the consumer's shape
//*   D  __fabandon() instead of fclose() on the same shape
//*   E  E37-04 in a PDS member write, then __fabandon()
//*   S  D37 on a sequential data set (ENOSPC since #176)
//*   A  C with the second CLOSE bare, for a SYSUDUMP of its abend
//*
//* TSTF705G is linked with this tree, TSTF705R with libc370 2.10.3,
//* the library the issue was measured with.  BARER is EXPECTED to end
//* SA0A on 2.10.3; it runs last.  grep the job log for TF705.
//*
//CONS     EXEC PGM=TSTF705G,PARM='C',REGION=4M,TIME=(0,30)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.F705SCR
//DIRECT   EXEC PGM=TSTF705G,PARM='D',REGION=4M,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.F705SCR
//E37      EXEC PGM=TSTF705G,PARM='E',REGION=4M,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.F705SCR
//D37      EXEC PGM=TSTF705G,PARM='S',REGION=4M,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.F705SCR
//BAREG    EXEC PGM=TSTF705G,PARM='AG',REGION=4M,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.F705SCR
//SYSUDUMP DD  SYSOUT=*
//CONSR    EXEC PGM=TSTF705R,PARM='CR',REGION=4M,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.F705SCR
//DIRECTR  EXEC PGM=TSTF705R,PARM='DR',REGION=4M,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.F705SCR
//BARER    EXEC PGM=TSTF705R,PARM='AR',REGION=4M,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.F705SCR
//SYSUDUMP DD  SYSOUT=*
//*
//* A net: each step removes its own data sets, so these are expected
//* to answer 8 - except AR, which BARER leaves behind on purpose.
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TF705.C PURGE
  DELETE IBMUSER.TF705.D PURGE
  DELETE IBMUSER.TF705.E1 PURGE
  DELETE IBMUSER.TF705.E2 PURGE
  DELETE IBMUSER.TF705.S1 PURGE
  DELETE IBMUSER.TF705.S2 PURGE
  DELETE IBMUSER.TF705.AG PURGE
  DELETE IBMUSER.TF705.CR PURGE
  DELETE IBMUSER.TF705.DR PURGE
  DELETE IBMUSER.TF705.AR PURGE
  SET MAXCC=0
/*
//
