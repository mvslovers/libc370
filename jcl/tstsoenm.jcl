//TSTSOENM JOB (SYS),'LIBC370 601',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #601 - a program's sockets are closed when it ends.  See
//* test/mvs/tstsoenm.c.  G this tree (ports 24601-24603), R the
//* installed library (24611-24613).  TSTSOAx ends with a listening
//* socket open, TSTSOBx binds its port; TSTSOCx LINKs TSTSODx, which
//* abends under try() holding a port, then binds it; TSTSOEx is the
//* guard: a LINKed module binds the caller's socket.  Module build:
//*   cc370 -O1 -Wall -Werror [-Iinclude -L build/sdk | -DRED]
//*         -DCASE=n -DPORT=p test/mvs/tstsoenm.c
//*   A 1, B 2, C 3, D 4, E 5, F 6; p = 24600/24610 + 1, 1, 2, 2, 3, 3
//*
//AG       EXEC PGM=TSTSOAG,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//BG       EXEC PGM=TSTSOBG,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//CG       EXEC PGM=TSTSOCG,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//EG       EXEC PGM=TSTSOEG,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//AR       EXEC PGM=TSTSOAR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//BR       EXEC PGM=TSTSOBR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//CR       EXEC PGM=TSTSOCR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//ER       EXEC PGM=TSTSOER,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//
