//TSTPREMN JOB (SYS),'LIBC370 PREMAIN',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* cc370#10 - the __premain() startup hook.  See test/mvs/tstpremn.c.
//* STEPLIB is the scratch PDS recvprm.jcl restores into.
//*   W  hook sets stdout to DD MYOUT (weak-reference startup)  CC 0
//*   F  the same, startup built by cc370 1.1.0 (asm WXTRN)    CC 0
//*   R  hook returns 12: main() must not run                  CC 12
//*   N  no hook: the control                                  CC 0
//* Run:     mvsdev JOB01398, 2026-10-05: W/F/N CC 0000, R CC 0012.
//* For W and F, main()'s line must be in MYOUT, not SYSPRINT.
//*
//W        EXEC PGM=TSTPRMW,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PRMSCR
//MYOUT    DD  SYSOUT=*
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//F        EXEC PGM=TSTPRMF,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PRMSCR
//MYOUT    DD  SYSOUT=*
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//R        EXEC PGM=TSTPRMR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PRMSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//N        EXEC PGM=TSTPRMN,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PRMSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
