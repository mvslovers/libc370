//TSTSTDM  JOB (SYS),'LIBC370 474',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #474 - stdio: fwrite, fseek, mode options.  See test/mvs/tststdm.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTSTDG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SDSCR
//RECS     DD  *
REC1
REC2
REC3
/*
//RED      EXEC PGM=TSTSTDR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SDSCR
//RECS     DD  *
REC1
REC2
REC3
/*
//
