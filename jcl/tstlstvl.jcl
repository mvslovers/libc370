//TSTLSTVL JOB (SYS),'LIBC370 59',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #59 - the "unable to open" diagnostic in open_vatlst() must
//* name the data set it could not open.
//*
//* The message had two %s conversions and one argument, so vsprintf()
//* formatted a word nothing had stored into as a char *.  Garbage in the
//* best case, an S0C4 in the worst - in the handler for a failure that was
//* about to be reported cleanly by returning NULL.
//*
//* THE CHECK IS NOT THE COND CODE.  A WTO cannot be read back from inside
//* the program, so the verdict is in the job log: the program brackets each
//* call with two markers of its own.  Between the (1) and (2) markers there
//* must be exactly ONE line, and it must carry the data set name SYSPRINT
//* prints underneath it.  Between the (3) markers there must be NOTHING.
//*
//* No setup, and nothing is left behind: every data set named is one that
//* must not exist, and dolspace=0 keeps LSPACE - the only other line
//* __listvl() can write - out of the window.
//*
//* PARM is optional: a VATLST that DOES exist (member name, or dsn(member))
//* adds case (4), which checks the success path is still silent and still
//* returns comments.  Without it case (4) is skipped.
//*
//* See test/mvs/tstlstvl.c.
//*
//* Build:   cc370 -O1 -Iinclude test/mvs/tstlstvl.c -o TSTLSTVL \
//*                -flinker-output=xmit
//* Install: RECEIVE the XMIT into the STEPLIB below.
//*
//* RC 0 = every observable check passed, 8 = one did not.  The job log
//* decides the rest.
//*
//RUN      EXEC PGM=TSTLSTVL,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
