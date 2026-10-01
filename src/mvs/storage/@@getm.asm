         COPY  MVSMACS
         COPY  PDPTOP
         PRINT OFF
         COPY  CLIBPPA                  CLIB Program Properties Area
         PRINT ON
         CSECT ,
         PRINT GEN
         SPACE 1
***********************************************************************
*                                                                     *
*  GETM - GET MEMORY                                                  *
*                                                                     *
*  The heap subpool is a runtime value (#89): resolved per call from  *
*  PPAHEAPS in the current TCB's own PPA and recorded in the high     *
*  byte of the rounded-size header word at block-8, where @@FREEM     *
*  reads it back as the SP||LV pair the R-form FREEMAIN wants in R0.  *
*  No PPA on this TCB (or an unset PPAHEAPS) means subpool 0 - the    *
*  pre-#89 behaviour, and the safe default for cthread TCBs, which    *
*  carry no PPA at all.                                               *
*                                                                     *
***********************************************************************
@@GETM   FUNHEAD ,
*
         LDINT R3,0(,R1)          LOAD REQUESTED STORAGE SIZE
         SLR   R1,R1              PRESET IN CASE OF ERROR
         LTR   R4,R3              CHECK REQUEST
         BNP   GETMEX             QUIT IF INVALID
*
* To reduce fragmentation, round up size to 64 byte multiple
*
         A     R3,=A(8+(64-1))    OVERHEAD PLUS ROUNDING
         N     R3,=X'FFFFFFC0'    MULTIPLE OF 64
*
* THE HIGH BYTE OF THE HEADER WORD CARRIES THE SUBPOOL (#89), SO A
* ROUNDED SIZE PAST 24 BITS WOULD DECODE AS A FOREIGN SUBPOOL ON
* FREE.  MALLOC'S 6M CAP DOES NOT PROTECT THIS PATH: __GETM() IS
* CALLABLE DIRECTLY
         CL    R3,=X'00FFFFFF'    FITS IN 24 BITS?
         BH    GETMEX             NO, RETURN NULL
*
* RESOLVE THE AMBIENT HEAP SUBPOOL: PSATOLD -> TCB -> TCBFSAB ->
* 8(FSA), VALIDATED THE WAY @@PPAGET TIER 1 DOES.  INLINE, NOT A
* BALR: FUNHEAD ESTABLISHED NO SAVE AREA, SO A NESTED SAVE WOULD
* OVERWRITE THE C CALLER'S REGISTERS AT 12(R13).  NO FALLBACK
* TIERS: A TCB WITHOUT ITS OWN PPA ALLOCATES FROM SUBPOOL 0
         SLR   R5,R5              DEFAULT HEAP SUBPOOL 0
         USING PSA,R0
         L     R2,PSATOLD
         USING TCB,R2
         SR    R6,R6
         ICM   R6,B'0111',TCBFSAB -> FIRST TCB SAVE AREA
         BZ    GETMSP             NO SAVE AREA, SUBPOOL 0
         ICM   R6,B'1111',8(R6)   -> NEXT TCB SAVE AREA
         BZ    GETMSP             NO NEXT, SUBPOOL 0
         CL    R6,=F'16777215'    GT X'FFFFFF'?
         BH    GETMSP             NOT A 24 BIT ADDRESS, SUBPOOL 0
         USING CLIBPPA,R6         PROGRAM PROPERTIES AREA
         CLC   PPAEYE,=A(PPAEYE$) VALID EYE CATCHER?
         BNE   GETMSP             NOT A PPA, SUBPOOL 0
         IC    R5,PPAHEAPS        AMBIENT HEAP SUBPOOL
         DROP  R6                 (CLIBPPA)
         DROP  R2                 (TCB)
GETMSP   DS    0H
*
         AIF   ('&SYS' NE 'S380').NOANY
         GETMAIN RC,LV=(R3),SP=(R5),LOC=ANY
         AGO   .FINANY
.NOANY   ANOP  ,
         GETMAIN RC,LV=(R3),SP=(R5)
.FINANY  ANOP  ,
*
* CONDITIONAL GETMAIN: A STORAGE SHORTAGE MUST SURFACE AS A NULL
* RETURN FROM MALLOC(), NOT AS AN S878 ABEND (#81)
         LTR   R15,R15            STORAGE OBTAINED?
         BZ    GETMOK             YES, SET UP THE PREFIX
         SLR   R1,R1              NO, RETURN NULL
         B     GETMEX
GETMOK   DS    0H
*
* WE STORE THE AMOUNT WE REQUESTED FROM MVS INTO THIS ADDRESS,
* WITH THE SUBPOOL IT CAME FROM IN THE HIGH BYTE (#89)
         SLL   R5,24              SUBPOOL TO THE HIGH BYTE
         ALR   R3,R5              SP||LV
         ST    R3,0(,R1)
* AND JUST BELOW THE VALUE WE RETURN TO THE CALLER, WE SAVE
* THE AMOUNT THEY REQUESTED
         ST    R4,4(,R1)
         A     R1,=F'8'
*
GETMEX   FUNEXIT RC=(R1)
         LTORG ,
         TITLE 'Dummy Sections'
         IKJTCB LIST=YES
         IHAPSA
         END
