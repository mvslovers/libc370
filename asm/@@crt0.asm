@@CRT0   TITLE '@ @ C R T 0  ***  MVS startup routine for C main pgm'
***********************************************************************
*  Original code and concepts provided by: PAUL EDWARDS.              *
*  Extensive modifications provided by: Mike Rayborn                  *
*                                                                     *
*  This startup code requires elements from the CLIB datasets.        *
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
         LR    R11,R1
*
         WXTRN @@STKLEN
         ICM   R1,15,=V(@@STKLEN)       Get stack length address
         BZ    USEDFLT                  No, use default
         L     R8,0(R1)                 Yes, load stack size value
         C     R8,=F'4096'              At least 4K?
         BNL   PLUSPPA                  Yes, continue
USEDFLT  DS    0H
         L     R8,=A(STACKLEN)          Default stack length
PLUSPPA  DS    0H
         AL    R8,=A(L'CLIBPPA+7)       Add in our CLIBPPA length
         N     R8,=X'00FFFFF8'          Round to nearest double word
         LA    R2,SUBPOOL               Subpool number
* CONDITIONAL GETMAIN (#108).  The R-form abended S80A from inside the
* SVC when the private area could not spare the stack.  That named
* neither the requester nor the size - httpd logged only "EXTERNAL
* PROGRAM MVSMF failed with S80A ABEND", and the S80A could equally
* have come from anywhere else in the address space.  RC lets us fail
* by name, as U0801, the way the CLIBGRT and CLIBCRT guards below
* already do (#81, #85).
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
         WTO   '@@CRT0 - No storage for C stack'
         ABEND 801,DUMP          Cannot run C code without a stack
STKOK    DS    0H
         XC    0(L'CLIBPPA,R1),0(R1)    Clear PPA
         ST    R13,4(,R1)
         ST    R1,8(,R13)
         LR    R6,R1                    -> PPA
         USING CLIBPPA,R6               Program Properties Area
         MVC   PPAEYE,=A(PPAEYE$)
         ST    R8,PPASTKLN              Save length of stack area
         LA    R0,SUBPOOL               Subpool number
         STC   R0,PPASUBPL              Save subpool number
*
         LA    R1,L'CLIBPPA(,R6)        -> New Save Area
         ST    R6,4(,R1)
         ST    R1,8(,R6)
         LR    R13,R1
         USING STACK,R13                Our Save Area
*
         L     R2,PSATOLD
         USING TCB,R2
         SR    R15,R15
         ICM   R15,B'0111',TCBFSAB => TCB first save area
         L     R0,8(,15)         get "next" value from fsa
         ST    R0,PPASAVE        save old "next" value in PPA
* Inherit the caller's heap subpool (#89).  For the first @@CRT0 on
* this TCB the word from 8(fsa) is whatever MVS left there, NOT a
* PPA, so validate it the way @@PPAGET does before reading from it.
* Anything else leaves PPAHEAPS as the XC above set it: subpool 0.
         LTR   R3,R0             candidate PPA to a real base reg
         BZ    NOINHER           zero, no caller PPA
         CL    R3,=F'16777215'   GT X'FFFFFF'?
         BH    NOINHER           not a 24 bit address
         CLC   PPAEYE-CLIBPPA(L'PPAEYE,R3),=A(PPAEYE$) eye catcher?
         BNE   NOINHER           not a PPA, inherit nothing
         MVC   PPAHEAPS,PPAHEAPS-CLIBPPA(R3) inherit heap subpool
NOINHER  DS    0H
         ST    R6,8(,R15)        save PPA as fsa "next" value
*
CRTSETUP DS    0H
         LA    R0,0
         ST    R0,DUMMYPTR       Unused in C, used by PL/1
         LA    R0,MAINSTK        Next available stack location
         ST    R0,THEIRSTK       => Next available stack (NAB)
*
* Create our CLIBCRT
         L     R15,=V(@@CRTSET)
         BALR  R14,R15           Create our CLIBCRT area
         L     R15,=V(@@GRTSET)
         BALR  R14,R15           Anchor a CLIBGRT area as CRTGRT
* A missing CLIBGRT surfaces later as NULL stdio/env anchors on a
* running program; fail loudly at startup instead (#85)
         LTR   R15,R15           Did we get a CLIBGRT?
         BZ    GRTOK             Yes, continue
         WTO   '@@CRT0 - No storage for CLIBGRT'
         ABEND 801,DUMP          Cannot run C code without a GRT
GRTOK    DS    0H
*
* Save R13 in CRTSAVE
         L     R15,=V(@@CRTGET)
         BALR  R14,R15           Get our CLIBCRT area
* A NULL CLIBCRT would make the ST below a store into low storage;
* fail loudly instead (#81)
         LTR   R15,R15           Did we get a CLIBCRT?
         BNZ   CRTOK             Yes, continue
         WTO   '@@CRT0 - No storage for CLIBCRT'
         ABEND 801,DUMP          Cannot run C code without a CRT
CRTOK    DS    0H
         ST    R13,CRTSAVE-CLIBCRT(,R15) Save our save area address
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
PPASETUP DS    0H
         EXTRACT WORKAREA,FIELDS=(TIOT,TSO,PSB),MF=(E,EXTRLIST)
         LM    R1,R3,WORKAREA    R1   R2  R3
         ST    R1,PPATIOT        SAVE POINTER TO TIOT
         TM    0(R2),X'80'       Is this TSO forground?
         BNO   PPASET10          No, check TSO background
         OI    PPAFLAG,PPATSOFG  Yes, set TSO flag
*
PPASET10 DS    0H
         LTR   R3,R3             Do we have PSCB?
         BZ    PPASET20          No, continue
         ST    R3,PPAPSCB        Yes, save PSCB
         OI    PPAFLAG,PPATSOBG  Yes, set TSO background flag
*
PPASET20 DS    0H
*
         ST    R11,PGMR1         R11 == R1 on entry to @@CRT0
         L     R2,0(,R11)        A(arguments to program)
         LA    R2,0(,R2)         ... clean address value
         ST    R2,ARGPTR         A(execution parameters)
         LA    R2,PGMNAME
         ST    R2,PGMNPTR        A(program name)
*
         L     R1,=A(CTHREAD)    A(thread driver routine)
         LA    R0,=CL8'CTHREAD'
         IDENTIFY EPLOC=(0),ENTRY=(1)
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
         LTORG
         TITLE 'CTHREAD - subtask driver (IDENTIFY entry point)'
         ENTRY CTHREAD
CTHREAD  DS    0H
         SAVE  (14,12),,'CTHREAD &SYSDATE &SYSTIME'
         LA    R12,0(,R15)
         USING CTHREAD,R12
*
         LA    R11,0(,R1)
         USING CTHDTASK,R11
*
* Chain stack with callers save area
         LA    R1,CTHDSTK        => stack for function
         ST    R13,4(,R1)        ... chain stack areas
         ST    R1,8(,R13)        ... chain stack areas
         LR    R13,R1            new stack
         USING STK,R13
*
* Save thread handle in stack
         ST    R11,STKCTHD       A(CTHDTASK)
*
* Set next available byte in stack
         LA    R0,STKNAB         next available byte in stack
         ST    R0,STKSVNAB       next available byte in stack
*
* Allocate CLIBCRT area in PPA
         L     R15,=V(@@CRTSET)
         BALR  R14,R15           Create CLIBCRT in PPA
*
* Save R13 in CRTSAVE
         L     R15,=V(@@CRTGET)
         BALR  R14,R15           Get our CLIBCRT area
* A NULL CLIBCRT would make the ST below a store into low storage;
* fail loudly instead (#81)
         LTR   R15,R15           Did we get a CLIBCRT?
         BNZ   TCRTOK            Yes, continue
         WTO   'CTHREAD - No storage for CLIBCRT'
         ABEND 801,DUMP          Cannot run C code without a CRT
TCRTOK   DS    0H
         ST    R13,CRTSAVE-CLIBCRT(,R15) Save our save area address
*
* Call thread function
         L     R15,CTHDFUNC      get function address from plist
         LA    R1,CTHDARG1       => parameters for function
         BALR  R14,R15           call function
         ST    R15,CTHDRC        save return code from function
*
* Call thread exit
         LA    R1,CTHDRC         => return code
         L     R15,=A(@@CTEXIT)
         BR    R15               exit thread environment
         LTORG
         TITLE '@@CTEXIT - exit C thread environment'
         ENTRY @@CTEXIT
@@CTEXIT DS    0H
         LA    R12,0(,R15)
         USING @@CTEXIT,R12
         L     R9,0(R1)          Get @@EXITB(rc) value
*
* Get save area address from CLIBCRT area
         L     R15,=V(@@CRTGET)
         BALR  R14,R15           Get our CLIBCRT area
         L     R13,CRTSAVE-CLIBCRT(,R15) Restore thread stack
         USING STK,R13
*
* Get thread task control block
         L     R11,STKCTHD       => thread task control block
         USING CTHDTASK,R11
*
* Get return code passed to us
*        L     R9,0(R1)          Get @@EXITB(rc) value
         ST    R9,CTHDRC         save as return code
*
* Do thread cleanup
         WXTRN @@CTCLUP
         ICM   R15,15,=V(@@CTCLUP) Get thread level cleanup
         BZ    THRDDONE
         BALR  R14,R15           Call __ctclup() routine
*
* Deallocate CLIBCRT area
THRDDONE DS    0H
         L     R15,=V(@@CRTRES)
         BALR  R14,R15           release CLIBCRT area from PPA
*
* Get callers save area
         L     R13,STKSV+4       switch back to callers stack
         LR    R15,R9            restore return code
RETURN   RETURN (14,12),RC=(15)
* Note:
* The task level area CTHDTASK persists until the main thread or
* thread manager code calls @@CTDEL() to delete the thread.
*
         LTORG ,
         TITLE 'Dummy Sections'
* Stack for C thread
STK      DSECT
STKSV    DS    18F               00 (0)  callers registers go here
STKSVLWS DS    A                 48 (72) PL/I Language Work Space N/A
STKSVNAB DS    A                 4C (76) next available byte -------+
STKCTHD  DS    A                 50 (80) A(CTHDTASK)                |
STKAVAIL DS    F                 54 (84) unused/available           |
STKNAB   DS    0D                58 stack next available byte <-----+
*
* C thread parameter list
CTHDTASK DSECT
CTHDEYE  DS    CL8               00 eye catcher for dumps
CTHDTCB  DS    F                 08 subtask TCB address
CTHDOTCB DS    F                 0C subtask owner TCB address
CTHDECB  DS    F                 10 posted by MVS when task ends
CTHDRC   DS    F                 14 return code from function
CTHDSSIZ DS    F                 18 stack size in bytes
CTHDFUNC DS    A                 1C subtask function address
CTHDARG1 DS    A                 20 arg1 for subtask function
CTHDARG2 DS    A                 24 arg2 for subtask function
CTHDSTK  DS    F                 28 start of stack for driver
*
         IKJTCB LIST=YES
         IEZJSCB
         IHAPSA
         IHARB
         IHACDE
STACK    DSECT
SAVEAREA DS    18F
DUMMYPTR DS    F                 => PL/I Language Work Space N/A
THEIRSTK DS    F                 => Next Available Byte (NAB)
WORKAREA DS    4F                work area
EXTRLIST EXTRACT MF=L            EXTRACT PARAMETER LIST
PARMLIST DS    0F                Parameter list passed to @@START
ARGPTR   DS    F                 A(parms)
PGMNPTR  DS    F                 A(program name)
TYPE     DS    F                 F'TSO job id'
PGMR1    DS    F                 R1 at entry to program
PGMNAME  DS    CL8
PGMNAMEN DS    C                 NUL BYTE FOR C
         DS    0D
MAINSTK  DS    65536F            stack for @@START -> main()
MAINLEN  EQU   *-MAINSTK
STACKLEN EQU   *-STACK
         END
