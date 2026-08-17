@@CRTM   TITLE '@ @ C R T M  ***  Mini startup routine for C main pgm'
***********************************************************************
*  Provided by: Mike Rayborn                                          *
*                                                                     *
*  This startup code create a minimal environment for C programs      *
*                                                                     *
*  RELEASED TO THE PUBLIC DOMAIN                                      *
***********************************************************************
         COPY  PDPTOP
         PRINT OFF
*
SUBPOOL  EQU   0
         USING PSA,R0
         PRINT ON
         COPY  CLIBCRT
         COPY  CLIBPPA                  CLIB Program Properties Area
         CSECT
         ENTRY @@CRT0
@@CRT0   DS    0H
         SAVE  (14,12),,@@CRT0
         LA    R12,0(,R15)
         USING @@CRT0,R12
         LR    R10,R0
         LR    R11,R1
*
         WXTRN @@STKLEN
         ICM   R8,15,=V(@@STKLEN)       Get stack length address
         BZ    USEDFLT                  No, use default
         L     R8,0(R8)                 Yes, load stack size value
         C     R8,=F'4096'              At least 4K?
         BNL   ROUNDUP                  Yes, continue
USEDFLT  DS    0H
         L     R8,=A(STACKLEN)          Default stack length
ROUNDUP  DS    0H
         LA    R8,7(,R8)                Plus 7 for rounding
         N     R8,=X'00FFFFF8'          Round to nearest double word
         LA    R2,SUBPOOL               Subpool number
* CONDITIONAL GETMAIN (#108).  The R-form abended S80A from inside the
* SVC when the private area could not spare the stack.  That named
* neither the requester nor the size - httpd logged only "EXTERNAL
* PROGRAM MVSMF failed with S80A ABEND", and the S80A could equally
* have come from anywhere else in the address space.  RC lets us fail
* by name, as U0801, the way the CLIBCRT guard below already does
* (#81, #85).
*
* We do NOT carry on with a smaller stack: PDPPRLG has no bounds check
* (it only bumps the NAB at 76(13)), so a short stack would run off the
* end silently and corrupt whatever follows it.
*
* R8 still holds the length that was refused, so the dump carries the
* size even though a WTO cannot: there is no writable storage to format
* a number into here, and this CSECT is linked into RENT load modules.
         GETMAIN RC,LV=(R8),SP=(R2)
         LTR   R15,R15           Did we get the stack?
         BZ    STKOK             Yes, continue
         WTO   '@@CRTM - No storage for C stack'
         ABEND 801,DUMP          Cannot run C code without a stack
STKOK    DS    0H
*
         ST    R13,4(,R1)
         ST    R1,8(,R13)
         LR    R13,R1
         USING STACK,R13                Our Save Area
*
CRTSETUP DS    0H
         LA    R0,0
         ST    R0,DUMMYPTR       Unused in C, used by PL/1
         LA    R0,MAINSTK        Next available stack location
         ST    R0,THEIRSTK       => Next available stack (NAB)
*
* Save R13 in CRTSAVE
         L     R15,=V(@@CRTGET)
         BALR  R14,R15           Get our CLIBCRT area
* A NULL CLIBCRT would make the L/ST below touch low storage;
* fail loudly instead (#81)
         LTR   R15,R15           Did we get a CLIBCRT?
         BNZ   CRTOK             Yes, continue
         WTO   '@@CRTM - No CLIBCRT (C environment missing)'
         ABEND 801,DUMP          Cannot run C code without a CRT
CRTOK    DS    0H
         L     R0,CRTSAVE-CLIBCRT(,R15) Get previous save area
         ST    R0,OLDSAVE        Save for later
         ST    R13,CRTSAVE-CLIBCRT(,R15) Save our save area address
*
         L     R2,PSATOLD
         USING TCB,R2
*
         L     R7,TCBRBP
         USING RBBASIC,R7
         SLR   R8,R8
         ICM   R8,B'0111',RBCDE1
         DROP  R7                (RBBASIC)
*
         USING CDENTRY,R8
         MVC   PGMNAME,CDNAME
         MVI   PGMNAMEN,0
         DROP  R8                (CDENTRY)
*
         L     R2,TCBJSCB
         USING IEZJSCB,R2
         LH    R2,JSCBTJID
         ST    R2,TYPE           TSO terminal job identifier
         DROP  R2                (IEZJSCB)
*
         ST    R10,PGMR0         R10 == R0 on entry to @@CRT0
         ST    R11,PGMR1         R11 == R1 on entry to @@CRT0
         LA    R2,PGMNAME
         ST    R2,PGMNPTR        A(program name)
*
         LA    R1,PARMLIST       A(parms,program,type)
         L     R15,=V(@@START)
         BALR  R14,R15           Should never return
*
* The call to @@START never returns because it will call @@EXIT
* after it calls main().
* But just in case @@START returns here, we'll call @@EXIT which
* eventually calls @@EXITA below.
         LA    R1,=F'-1'
         L     R15,=V(@@EXIT)
         BR    R15               Just in case @@START returns
***      LTORG
         TITLE '@@EXITA - exit C runtime environment'
         ENTRY @@EXITA
@@EXITA  DS    0H
* SWITCH BACK TO OUR OLD SAVE AREA
         LA    R12,0(,R15)
         USING @@EXITA,R12
         L     R9,0(R1)          Get exit(rc) value
*
* Get save area address from CLIBCRT area
         L     R15,=V(@@CRTGET)
         BALR  R14,R15           Get our CLIBCRT area
         L     R13,CRTSAVE-CLIBCRT(,R15) Restore original stack
         L     R0,OLDSAVE        Get old CRTSAVE value
         ST    R0,CRTSAVE-CLIBCRT(,R15) restore CRTSAVE
*
* Release stack storage
*
         LR    R1,R13            R1=A(storage to be freed)
         L     R13,4(,R1)        Original save area
***      WXTRN @@STKLEN
         ICM   R8,15,=V(@@STKLEN)       Get stack length address
         BZ    EXITDFLT                 No, use default
         L     R8,0(R8)                 Yes, load stack size value
         C     R8,=F'4096'              At least 4K?
         BNL   EXITSIZE                 Yes, continue
EXITDFLT DS    0H
         L     R8,=A(STACKLEN)          Default stack length
EXITSIZE DS    0H
         LA    R8,7(,R8)                Plus 7 for rounding
         N     R8,=X'00FFFFF8'          Round to nearest double word
         LA    R0,SUBPOOL               Subpool number
         SLL   R0,24                    Shift into high byte
         ALR   R0,R8                    Plus size of storage we want
         FREEMAIN R,LV=(0),A=(R1)
*
* Return to system
         LR    R15,R9            Get exit(rc) value
         RETURN (14,12),RC=(15)
         LTORG
         TITLE 'Dummy Sections'
         IKJTCB LIST=YES
         IEZJSCB
         IHAPSA
         IHARB
         IHACDE
STACK    DSECT
SAVEAREA DS    18F
DUMMYPTR DS    F                 => PL/I Language Work Space N/A
THEIRSTK DS    F                 => Next Available Byte (NAB)
PARMLIST DS    0F                Parameter list passed to @@START
PGMR0    DS    F                 R0 at entry to program
PGMNPTR  DS    F                 A(program name)
TYPE     DS    F                 F'TSO job id'
PGMR1    DS    F                 R1 at entry to program
PGMNAME  DS    CL8
PGMNAMEN DS    C                 NUL BYTE FOR C
OLDSAVE  DS    F                 Old CRTSAVE value
         DS    0D
MAINSTK  DS    16384F            stack for @@START -> main()
MAINLEN  EQU   *-MAINSTK
STACKLEN EQU   *-STACK
         END

