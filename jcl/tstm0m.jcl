//TSTM0M   JOB (SYS),'LIBC370 394',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #394 - malloc(0) without ENOMEM or message.
//* See test/mvs/tstm0m.c.
//* GREEN is this tree; RED a build before the fix.
//* Run: own MVS/CE container, 2026-10-20: GREEN CC 0000, no
//*      message; RED CC 0008, errno 12 and "Out of memory" 3 times.
//*
//GREEN    EXEC PGM=TSTM0G,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.MZ0SCR
//RED      EXEC PGM=TSTM0R,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.MZ0SCR
//
