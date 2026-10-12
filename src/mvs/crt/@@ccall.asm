@@CCALL  TITLE '@ @ C C A L L  ***  Call C from a non-C caller'
***********************************************************************
*  @@CCALL - call a C function from code that is not C (#502)         *
*                                                                     *
*  COBOL and assembler code call with an OS save area of 72 bytes in *
*  R13.  A C function takes its frame from the word at offset 76 of   *
*  the caller's R13, the NAB of the C stack, which such a save area   *
*  does not have.  @@CCALL finds the C frame that called out, builds  *
*  a frame of its own at that frame's NAB and calls the C function    *
*  from there.  A C program must be running on the task.              *
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
*  SPIE.  The caller's SPIE (COBOL establishes one for the step) is   *
*  cancelled for the call and established again afterwards, so a      *
*  program check in C reaches libc370's recovery (try(), ESTAE) as    *
*  it would in a C program.  With COBOL's SPIE active, try() got      *
*  COBOL's U3001 instead of the S0C1, and the console a COBOL line    *
*  that did not exist (measured, #502).  An outer try() that retries  *
*  past this call leaves the caller's SPIE cancelled.                 *
*                                                                     *
*  How the C frame is found.  @@CRT0 puts the CLIBPPA in front of the *
*  stack and chains the first stack frame back to it, so the back     *
*  chain of every C frame ends at the PPA eye catcher.                *
*    pass 1  back chain from the caller's save area to the PPA:       *
*            the stack is [PPA, PPA+PPASTKLN).                        *
*    pass 2  the first save area on that chain inside the stack is    *
*            the C frame that called out; its word at 76 is the NAB.  *
*            Save areas outside the stack (static ones of COBOL or    *
*            assembler code in the load module) are passed over.     *
*                                                                     *
*  ABENDs  U3101  no PPA on the back chain: no C program on the task  *
*                 (a COBOL main), or a CTHREAD stack, which has none  *
*          U3102  no C frame on the back chain                        *
*          U3103  no room on the C stack for the frame                *
*          U3104  more than MAXARGS parameters                        *
*                                                                     *
*  Reentrant: it stores only into the caller's save area and the C    *
*  stack; the WTO, ABEND and SPIE lists inline are only read.         *
***********************************************************************
         COPY  PDPTOP
         PRINT OFF
         PRINT ON
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
         BZ    NOPPA              end of the chain
         CLC   0(4,R3),=A(PPAEYE$) the PPA?
         BE    P1FOUND
         BCT   R4,P1LOOP
         B     NOPPA
P1FOUND  DS    0H
         LR    R5,R3              R5 = low end of the stack (PPA)
         L     R6,PPASTKLN-CLIBPPA(,R3)
         ALR   R6,R5              R6 = high end of the stack
*
* Pass 2: the first save area inside the stack, above the PPA
         LR    R3,R13
         LA    R4,MAXHOPS
P2LOOP   DS    0H
         L     R3,4(,R3)
         N     R3,=X'00FFFFFF'
         BZ    NOFRAME
         CLR   R3,R5
         BE    NOFRAME            reached the PPA: no C frame
         BL    P2NEXT             below the stack: a static save area
         CLR   R3,R6
         BL    P2FOUND            inside the stack: the C frame
P2NEXT   DS    0H
         BCT   R4,P2LOOP
         B     NOFRAME
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
*
NOPPA    DS    0H
         WTO   '@@CCALL - no C program on this task (no PPA)'
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
         LTORG
         END
