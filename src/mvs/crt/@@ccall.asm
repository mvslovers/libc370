@@CCALL  TITLE '@ @ C C A L L  ***  Call C from a non-C caller'
***********************************************************************
*  @@CCALL - call a C function from code that is not C (#502, #507)   *
*                                                                     *
*  COBOL and assembler code call with an OS save area of 72 bytes in *
*  R13.  A C function takes its frame from the word at offset 76 of   *
*  the caller's R13, the NAB of the C stack, which such a save area   *
*  does not have.  @@CCALL finds the C stack, builds a frame of its   *
*  own there and calls the C function from it.                        *
*                                                                     *
*  Called through a stub, one per C function (macro CCALL):           *
*      CCFUNC   CCALL CFUNC                                           *
*  COBOL then calls CALL 'CCFUNC' USING ...                           *
*                                                                     *
*  ENTRY  R0  = A(C function)                                         *
*         R1  = OS parameter list (addresses, X'80' on the last) or 0 *
*         R13 = the caller's save area, R14 = return, R15 = @@CCALL   *
*  EXIT   R15 = the C function's return value (an int or a pointer)   *
*         R0-R14 as on entry                                          *
*                                                                     *
*  The C function gets a copy of the list without the X'80' on the   *
*  last address, at most MAXARGS entries.                             *
*                                                                     *
*  Two kinds of caller:                                               *
*                                                                     *
*  1. Under a C program (#502).  @@CRT0 puts the CLIBPPA in front of  *
*     the stack and chains the first stack frame back to it.          *
*       pass 1  back chain from the caller's save area to the PPA:    *
*               the stack is [PPA, PPA+PPASTKLN).                     *
*       pass 2  the first save area on that chain inside the stack is *
*               the C frame that called out; its word at 76 is the    *
*               NAB.  Save areas outside the stack (static ones of    *
*               COBOL or assembler code) are passed over.             *
*                                                                     *
*  2. With no C program on the task, e.g. under a COBOL main program *
*     (#507).  The first call builds a C environment the way @@CRT0   *
*     does - PPA and stack (@@STKLEN, 256K by default), the PPA       *
*     anchored as the "next" save area of the TCB's first save area,  *
*     CLIBCRT, CLIBGRT, EXTRACT for TSO, then @@RTINIT for the        *
*     standard streams, the environment variables and the time zone - *
*     and marks the PPA PPACCALL.  Later calls find it there.  A call *
*     with no C frame on its chain (the outermost one) takes the      *
*     stack's first frame and records its caller's save area at 4 of  *
*     the PPA, so that exit() in the C function returns to that       *
*     caller through @@EXITA, with the environment released and R15 = *
*     the exit status.  CCTERM releases it the same way: COBOL calls  *
*     it before STOP RUN, or buffered output and the atexit()        *
*     functions are lost.  Not built when a TCB above this one has a  *
*     C environment (a CTHREAD thread or a subtask of a C program):   *
*     U3101, as before #507.                                          *
*                                                                     *
*  SPIE.  The caller's SPIE (COBOL establishes one for the step) is   *
*  cancelled for the call and established again afterwards, so a      *
*  program check in C reaches libc370's recovery (try(), ESTAE) as    *
*  it would in a C program.  With COBOL's SPIE active, try() got      *
*  COBOL's U3001 instead of the S0C1, and the console a COBOL line    *
*  that did not exist (measured, #502).  An outer try() that retries  *
*  past this call, and an exit() in the C function, leave the         *
*  caller's SPIE cancelled.                                           *
*                                                                     *
*  ABENDs  U3101  no C environment to use or build here               *
*          U3102  no C frame on the back chain under a C program      *
*          U3103  no room on the C stack for the frame                *
*          U3104  more than MAXARGS parameters                        *
*          U3105  the C environment could not be built (storage, a    *
*                 standard stream - the cause is in the job log)      *
*                                                                     *
*  Reentrant: it stores only into the caller's save area, the TCB's   *
*  first save area and the storage it obtains; the WTO, ABEND, SPIE   *
*  lists inline are only read.                                        *
***********************************************************************
         COPY  PDPTOP
         PRINT OFF
         USING PSA,R0
         PRINT ON
         COPY  CLIBCRT
         COPY  CLIBPPA                  CLIB Program Properties Area
         CSECT
         ENTRY @@CCALL
@@CCALL  DS    0H
         STM   R14,R12,12(R13)    save the caller's registers
         LR    R12,R15
         USING @@CCALL,R12
         LR    R9,R0              R9  = A(C function)
         LR    R10,R1             R10 = the caller's parameter list
*
* Pass 1: back chain to the PPA
         LR    R3,R13
         LA    R4,MAXHOPS
P1LOOP   DS    0H
         L     R3,4(,R3)          previous save area
         N     R3,=X'00FFFFFF'    24-bit address
         BZ    FSAPPA             end of the chain
         CLC   0(4,R3),=A(PPAEYE$) the PPA?
         BE    P1FOUND
         BCT   R4,P1LOOP
         B     FSAPPA
P1FOUND  DS    0H
         LR    R5,R3              R5 = low end of the stack (PPA)
         B     BOUNDS
*
* Not on the chain: an environment @@CCALL built, anchored at the
* TCB's first save area?
FSAPPA   DS    0H
         L     R2,PSATOLD
         USING TCB,R2
         SR    R3,R3
         ICM   R3,B'0111',TCBFSAB
         BZ    NOPPA              no first save area: nothing to do
         ICM   R3,B'1111',8(R3)   its "next"
         BZ    GUARD
         N     R3,=X'00FFFFFF'
         CLC   0(4,R3),=A(PPAEYE$)
         BNE   GUARD
         TM    PPAFLAG-CLIBPPA(R3),PPACCALL
         BNO   NOPPA              a C program's, but not on our chain
         LR    R5,R3
         B     BOUNDS
*
* None here.  If a TCB above has a C environment, this is a C thread
* or a subtask of a C program: leave it alone.  Otherwise build one.
GUARD    DS    0H
         LR    R4,R2
GLOOP    DS    0H
         C     R4,TCBJSTCB        reached the job step TCB?
         BE    BUILD
         SR    R0,R0
         ICM   R0,B'0111',TCBOTC+1-TCB(R4)
         BZ    BUILD              no owner
         LR    R4,R0
         SR    R3,R3
         ICM   R3,B'0111',TCBFSAB-TCB(R4)
         BZ    GLOOP
         ICM   R3,B'1111',8(R3)
         BZ    GLOOP
         N     R3,=X'00FFFFFF'
         CLC   0(4,R3),=A(PPAEYE$)
         BE    NOPPA              an owner runs C: not ours to build
         B     GLOOP
         DROP  R2
*
BOUNDS   DS    0H
         L     R6,PPASTKLN-CLIBPPA(,R5)
         ALR   R6,R5              R6 = high end of the stack
*
* Pass 2: the first save area inside the stack, above the PPA
P2START  DS    0H
         LR    R3,R13
         LA    R4,MAXHOPS
P2LOOP   DS    0H
         L     R3,4(,R3)
         N     R3,=X'00FFFFFF'
         BZ    P2NONE
         CLR   R3,R5
         BE    P2NONE             reached the PPA: no C frame
         BL    P2NEXT             below the stack: a static save area
         CLR   R3,R6
         BL    P2FOUND            inside the stack: the C frame
P2NEXT   DS    0H
         BCT   R4,P2LOOP
P2NONE   DS    0H
         TM    PPAFLAG-CLIBPPA(R5),PPACCALL
         BNO   NOFRAME            under a C program: an error
* The outermost call into an environment @@CCALL built: the stack's
* first frame, and exit() returns to this caller (see the prologue)
         LA    R3,L'CLIBPPA(,R5)  the stack's first frame
         ST    R13,4(,R5)         @@EXITA returns through this one
P2FOUND  DS    0H
         L     R7,76(,R3)         R7 = its NAB = our frame
         N     R7,=X'00FFFFFF'
         CLR   R7,R3
         BNH   NOFRAME            a NAB lies above its own frame
         LA    R0,GLEN(,R7)
         CLR   R0,R6
         BH    STKFULL            our frame would leave the stack
*
* Our frame: back to the caller's save area, the NAB past it
         ST    R13,4(,R7)
         ST    R7,8(,R13)
         ST    R0,76(,R7)         NAB for the C function
         XC    72(4,R7),72(R7)
*
* Copy the parameter list without the X'80' on the last address
         SR    R11,R11            no list: R1 = 0 for the function
         LTR   R10,R10
         BZ    CALLC
         LA    R11,GARGS(,R7)
         LR    R2,R11
         LA    R4,MAXARGS
CPLOOP   DS    0H
         L     R8,0(,R10)
         LR    R0,R8
         N     R0,=X'00FFFFFF'
         ST    R0,0(,R2)
         LTR   R8,R8
         BM    CALLC              X'80': that was the last one
         LA    R10,4(,R10)
         LA    R2,4(,R2)
         BCT   R4,CPLOOP
         B     TOOMANY            no X'80' within MAXARGS
*
CALLC    DS    0H
* Cancel the caller's SPIE for the call (R1 = its PICA, or 0)
         SPIE  ,
         ST    R1,GPICA(,R7)
         LR    R1,R11
         LR    R13,R7
         LR    R15,R9
         BALR  R14,R15            call the C function
* Establish the caller's SPIE again (a PICA of 0 cancels: as before)
         LR    R2,R15
         L     R1,GPICA(,R13)
         SPIE  MF=(E,(1))
         LR    R15,R2
*
* Back to the caller with the function's R15
         L     R13,4(,R13)
         L     R14,12(,R13)
         LM    R0,R12,20(R13)
         BR    R14
         TITLE '@@CCALL - build a C environment (#507)'
***********************************************************************
*  BUILD - no C environment on this task or above it: build one the  *
*  way @@CRT0 does, then carry on with the call (pass 2 finds no C    *
*  frame, the call takes the stack's first frame).                    *
***********************************************************************
BUILD    DS    0H
         WXTRN @@STKLEN
         ICM   R1,15,=V(@@STKLEN) the program's stack length?
         BZ    BLDDFLT
         L     R8,0(,R1)
         C     R8,=F'4096'        at least 4K?
         BNL   BLDSIZE
BLDDFLT  DS    0H
         L     R8,=A(STACKLEN)    the default, as @@CRT0's
BLDSIZE  DS    0H
         AL    R8,=A(L'CLIBPPA+7) plus the PPA in front
         N     R8,=X'00FFFFF8'    a whole number of doublewords
         GETMAIN RC,LV=(R8),SP=0
         LTR   R15,R15
         BNZ   NOSTOR
         LR    R5,R1              R5 = the PPA
         XC    0(L'CLIBPPA,R5),0(R5)
         USING CLIBPPA,R5
         MVC   PPAEYE,=A(PPAEYE$)
         ST    R8,PPASTKLN        PPA + stack, for @@EXITA's FREEMAIN
         MVI   PPASUBPL,0
         OI    PPAFLAG,PPACCALL   built by @@CCALL
         ST    R13,PPAPREV        the caller, until a call sets its own
*
* The stack's first frame, chained to the PPA as @@CRT0 does
         LA    R7,L'CLIBPPA(,R5)
         ST    R5,4(,R7)
         ST    R7,PPANEXT
         XC    0(BASENAB,R7),0(R7)
         LA    R0,BASENAB(,R7)
         ST    R0,76(,R7)         its NAB
*
* Anchor the PPA as the "next" of the TCB's first save area
         L     R2,PSATOLD
         USING TCB,R2
         SR    R3,R3
         ICM   R3,B'0111',TCBFSAB
         L     R0,8(,R3)
         ST    R0,PPASAVE         restored by @@EXITA
         ST    R5,8(,R3)
*
* TSO: the same EXTRACT as @@CRT0 (stdio reads PPAFLAG_TSOFG)
         LA    R3,BEXTRL(,R7)     the list, zeroed above
         LA    R4,BWORK(,R7)      the answer area
         EXTRACT (R4),FIELDS=(TIOT,TSO,PSB),MF=(E,(R3))
         LM    R1,R3,BWORK(R7)
         ST    R1,PPATIOT
         TM    0(R2),X'80'        TSO foreground?
         BNO   BLDTSO2
         OI    PPAFLAG,PPATSOFG
BLDTSO2  DS    0H
         LTR   R3,R3              a PSCB?
         BZ    BLDCRT
         ST    R3,PPAPSCB
         OI    PPAFLAG,PPATSOBG
         DROP  R2
*
* CLIBCRT and CLIBGRT, then the runtime part of the startup, all of it
* C code running on the stack's first frame
BLDCRT   DS    0H
         LR    R11,R13            the caller's save area
         LR    R13,R7
         L     R15,=V(@@CRTSET)
         BALR  R14,R15            our CLIBCRT
         L     R15,=V(@@GRTSET)
         BALR  R14,R15            our CLIBGRT
         LTR   R15,R15
         BNZ   BLDFAIL
         L     R15,=V(@@CRTGET)
         BALR  R14,R15
         LTR   R15,R15
         BZ    BLDFAIL
         ST    R7,CRTSAVE-CLIBCRT(,R15) @@EXITA restores this frame
         L     R15,=V(@@RTINIT)
         BALR  R14,R15            streams, environment, time zone
         LTR   R15,R15
         BNZ   BLDFAIL
         LR    R13,R11
         DROP  R5
         B     BOUNDS             carry on with the call
*
* Could not build it: undo the anchor and the storage, then abend
BLDFAIL  DS    0H
         L     R2,PSATOLD
         USING TCB,R2
         SR    R3,R3
         ICM   R3,B'0111',TCBFSAB
         L     R0,PPASAVE-CLIBPPA(,R5)
         ST    R0,8(,R3)
         DROP  R2
         LR    R1,R5
         L     R0,PPASTKLN-CLIBPPA(,R5)
         FREEMAIN R,LV=(0),A=(1)
         LR    R13,R11
NOSTOR   DS    0H
         WTO   '@@CCALL - the C environment could not be built'
         ABEND 3105,DUMP
         TITLE '@@CCALL - CCTERM, release the C environment (#507)'
***********************************************************************
*  CCTERM - release the C environment @@CCALL built: COBOL calls      *
*  CALL 'CCTERM' before STOP RUN.  It ends the C side the way exit(0) *
*  does - atexit() functions, files flushed and closed, the storage   *
*  released, the TCB's first save area restored - and returns to its *
*  caller with RC 0.  RC 4: there is no such environment (none was    *
*  built, CCTERM ran already, or it belongs to a C program).          *
***********************************************************************
         ENTRY CCTERM
CCTERM   DS    0H
         STM   R14,R12,12(R13)
         LR    R12,R15
         LA    R0,CCTERM-@@CCALL
         SR    R12,R0             the base of @@CCALL
         L     R2,PSATOLD
         USING TCB,R2
         SR    R3,R3
         ICM   R3,B'0111',TCBFSAB
         BZ    TERMNONE
         ICM   R3,B'1111',8(R3)
         BZ    TERMNONE
         N     R3,=X'00FFFFFF'
         CLC   0(4,R3),=A(PPAEYE$)
         BNE   TERMNONE
         TM    PPAFLAG-CLIBPPA(R3),PPACCALL
         BNO   TERMNONE           a C program's: not ours to end
         DROP  R2
         ST    R13,4(,R3)         @@EXITA returns to our caller
         LA    R13,L'CLIBPPA(,R3) the stack's first frame
         LA    R1,=A(0)           exit status 0
         L     R15,=V(@@EXIT)
         BALR  R14,R15            does not return here
TERMNONE DS    0H
         LA    R15,4
         L     R14,12(,R13)
         LM    R0,R12,20(R13)
         BR    R14
*
NOPPA    DS    0H
         WTO   '@@CCALL - no C environment here (C thread or subtask)'
         ABEND 3101,DUMP
NOFRAME  DS    0H
         WTO   '@@CCALL - no C frame on the save area chain'
         ABEND 3102,DUMP
STKFULL  DS    0H
         WTO   '@@CCALL - no room on the C stack'
         ABEND 3103,DUMP
TOOMANY  DS    0H
         WTO   '@@CCALL - more than 16 parameters'
         ABEND 3104,DUMP
*
MAXHOPS  EQU   256                save areas walked at most
MAXARGS  EQU   16                 parameters passed at most
GARGS    EQU   80                 the copied list in our frame
GPICA    EQU   GARGS+4*MAXARGS    the caller's PICA
GLEN     EQU   (GPICA+4+7)/8*8    our frame, a multiple of 8
BWORK    EQU   96                 the first frame: EXTRACT's answers
BEXTRL   EQU   112                ... and its list
BASENAB  EQU   128                ... and where its NAB starts
STACKLEN EQU   65536*4            the default stack, as @@CRT0's
         LTORG
         TITLE 'Dummy Sections'
         IKJTCB LIST=YES
         IHAPSA
         END
