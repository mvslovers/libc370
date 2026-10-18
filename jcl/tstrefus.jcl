//TSTREFUS JOB (SYS),'LIBC370 405',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #405/#407 - racf_logout(NULL), ssct_remove(NULL) and the
//* SSVT slot bounds are refused before any service.  Problem state, no
//* APF.  No red run on MVS - see test/mvs/tstrefus.c.
//* Run:     mvsdev JOB02446, 2026-10-18: 5/5 PASS.
//*
//REFUS    EXEC PGM=TSTREFUS,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RFSCR
//SYSPRINT DD  SYSOUT=*
//
