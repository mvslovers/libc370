//TSTPF99  JOB (SYS),'LIBC370 578',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #578 - the printf cases of test/host/pf99cases.h formatted
//* on MVS (SYSPRINT, compared offline with "tstpf99 -l" of the host
//* build), and %ll/%j/%z/%t/%p against C99 (console).
//*
//PF99     EXEC PGM=TSTPF99,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PFSCR
//SYSPRINT DD  SYSOUT=*
//
