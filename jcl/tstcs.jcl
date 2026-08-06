//TSTCS    JOB (SYS),'LIBC370 48',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #48 - __cs() must store new_value, not the word at that address.
//* See test/mvs/tstcs.c; MVS only, __cs() is inline assembler.
//*
//* Build:   cc370 -O1 -Iinclude test/mvs/tstcs.c -o TSTCS \
//*                -flinker-output=xmit
//* Install: RECEIVE the XMIT into the STEPLIB below.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTCS,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
