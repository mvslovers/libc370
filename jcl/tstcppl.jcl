//TSTCPPL  JOB (SYS),'LIBC370 210',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #210 - ppacppl is never set, tsocmd() answers "No CPPL".
//* See test/mvs/tstcppl.c for the cells.  STEPLIB is the scratch PDS
//* recvcppl.jcl restores into.
//*
//* The verdict is in the job log: one TSTCPPL WTO per cell and a
//* closing "TSTCPPL <mode> RC=n" per run.  Each step's COND CODE
//* carries that RC (the TMP passes the last command's RC on), but a
//* step that abends reports nothing of the cells after it - read the
//* WTOs.
//*
//* Red on the old library: mvsdev JOB00677, 2026-09-29 - BATCH 0000,
//* TSOCALL 0000, TSOCP 0008 (c1, c3 FAIL, tsocmd rc=8); again with
//* cc370 b91f913, JOB00681.  Green with the fix: JOB00683, all 0000;
//* with (c4): JOB00686, all 0000.
//*
//* BATCH   EXEC PGM=      no CPPL           control
//* TSOCALL TSO CALL       no CPPL           control
//* TSOCP   TSO command    CPPL, and tsocmd() LINKs the probe again
//*                        as CHILD, which must return 42
//*
//BATCH    EXEC PGM=TSTCPPL,PARM='BATCH',REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CPPLLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//TSOCALL  EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CPPLLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  CALL 'IBMUSER.LIBC370.CPPLLIB(TSTCPPL)' 'CALL'
/*
//*
//TSOCP    EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CPPLLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  TSTCPPL CP
/*
//
