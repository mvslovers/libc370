         MACRO ,
&NM      FIXWRITE ,
&NM      L     R15,=V(@@ATROUT)
         BALR  R14,R15       TRUNCATE CURRENT WRITE BLOCK
         MEND  ,
         COPY  MVSMACS
         COPY  PDPTOP
         CSECT ,
         PRINT GEN
*        YREGS
         SPACE 1
*-----------------------ASSEMBLY OPTIONS------------------------------*
SUBPOOL  EQU   0                                                      *
*---------------------------------------------------------------------*
         SPACE 1
***********************************************************************
*                                                                     *
*  AWRITE - Write to an open data set                                 *
*                                                                     *
***********************************************************************
@@AWRITE FUNHEAD IO=YES,AM=YES,SAVE=SAVEADCB,US=NO   WRITE / PUT
         LR    R11,R1             SAVE PARM LIST
         NI    IOPFLAGS,255-IOFLSDW  A NEW CALL IS A NEW RECORD (#757)
WRITMORE NI    IOPFLAGS,255-IOFCURSE   RESET RECURSION
         L     R4,4(,R11)         R4 points to the record address
         L     R4,0(,R4)          Get record address
         L     R5,8(,R11)         R5 points to length of data to write
         L     R5,0(,R5)          Length of data to write
         TM    IOPFLAGS,IOFLSDW   Rest of a spanned record (#757)?
         BZ    *+8                No; the caller's record
         LM    R4,R5,KEPTREC      Yes; WRITEWAY left address, length
         TM    IOMFLAGS,IOFTERM   PUTLIN request?
         BNZ   TPUTWRIT           Yes
*
         TM    IOMFLAGS,IOFBLOCK  Block mode?
         BNZ   WRITBLK            Yes
         CLI   OPENCLOS,X'84'     Running in update mode ?
         BNE   WRITENEW           No
         LM    R2,R3,KEPTREC      Get last record returned
         LTR   R3,R3              Any?
         BNP   WRITEEX            No; ignore (or abend?)
         CLI   RECFMIX,4          RECFM=V...
         BNE   WRITUPMV           NO
         LA    R0,4               ADJUST FOR RDW
         AR    R2,R0              KEEP OLD RDW
         SR    R3,R0              ADJUST REPLACE LENGTH
         AR    R4,R0              SKIP OVER USER'S RDW
         SR    R5,R0              ADJUST LENGTH
WRITUPMV MVCL  R2,R4              REPLACE DATA IN BUFFER
         OI    IOPFLAGS,IOFLDATA  SHOW DATA IN BUFFER
         B     WRITEEX            REWRITE ON NEXT READ OR CLOSE
         SPACE 1
WRITENEW CLI   RECFMIX,4          V-FORMAT ?
         BH    WRITBLK            U - WRITE BLOCK AS IS
         BL    WRITEFIX           F - ADD RECORD TO BLOCK
         TM    IOPFLAGS,IOFLSDW   REST OF A SPANNED RECORD (#757)?
         BNZ   WRITEVAB           YES; NO RDW IN FRONT OF IT
         CH    R5,0(,R4)          RDW LENGTH = REQUESTED LEN?
         BNE   WRITEBAD           NO; FAIL
WRITEVAB L     R8,BUFFADDR        GET BUFFER
         ICM   R6,15,BUFFCURR     Get next record address
         BNZ   WRITEVAT
         LA    R0,4
         STH   R0,0(,R8)          BUILD BDW
         LA    R6,4(,R8)          SET TO FIRST RECORD POSITION
WRITEVAT L     R9,BUFFEND         GET BUFFER END
         SR    R9,R6              LESS CURRENT POSITION
         TM    ZRECFM,DCBRECSB    SPANNED?
         BZ    WRITEVAR           NO; ROUTINE VARIABLE WRITE
         LA    R1,4(,R5)          GET RECORD + BDW LENGTH
         C     R1,LRECL           VALID SIZE?
         BH    WRITEBAD           NO; TAKE A DIVE
         TM    IOPFLAGS,IOFLSDW   CONTINUATION ?
         BNZ   WRITEVAW           YES; DO HERE
         CR    R5,R9              WILL IT FIT AS IS?
         BNH   WRITEVAS           YES; DON'T NEED TO SPLIT
WRITEVAW CH    R9,=H'5'           AT LEAST FIVE BYTES LEFT ?
         BL    WRITEVNU           NO; WRITE THIS BLOCK; RETRY
         LR    R3,R6              SAVE START ADDRESS
         LR    R7,R9              COPY LENGTH
         CR    R7,R5              ROOM FOR ENTIRE SEGMENT ?
         BL    *+4+2              NO
         LR    R7,R5              USE ONLY WHAT'S AVAILABLE
         LR    R2,R7              SEGMENT LENGTH, SDW INCLUDED (#757)
         MVCL  R6,R4              COPY RDW + DATA
         ST    R6,BUFFCURR        UPDATE NEXT AVAILABLE
         SR    R6,R8              LESS START
         STH   R6,0(,R8)          UPDATE BDW
         STH   R2,0(,R3)          SDW: THE SEGMENT'S LENGTH (#757)
         MVC   2(2,R3),=X'0100'   SET FLAGS FOR START SEGMENT
         TM    IOPFLAGS,IOFLSDW   DID START ?
         BZ    *+8                NO; FIRST SEGMENT (SKIP THE MVI ONLY)
         MVI   2(R3),3            SHOW MIDDLE SEGMENT
         LTR   R5,R5              DID WE FINISH THE RECORD ?
         BP    WRITEWAY           NO
         MVI   2(R3),2            SHOW LAST SEGMENT
         NI    IOPFLAGS,255-IOFLSDW-IOFCURSE  RCD COMPLETE
         OI    IOPFLAGS,IOFLDATA  SHOW WRITE DATA IN BUFFER
         B     WRITEEX            DONE
*  The rest of the record goes out as the next segment, once this
*  block is written (#757).  MVCL has moved R4 past the bytes copied
*  and taken them off R5.  The next segment needs an SDW in front of
*  its data, so it starts 4 bytes back, on the last 4 bytes just
*  copied; their copy in the buffer is overwritten by the SDW built
*  above, the caller's record is never written to.  The block goes
*  out through WRITEVNU, and WRITMORE picks address and length up
*  from KEPTREC.
*  Before this, BZ *+4+6 above skipped the LTR as well, so every
*  first segment was flagged as the last one and the rest of the
*  record was dropped; this path had never run.
WRITEWAY SH    R4,=H'4'           BACK UP OVER THE NEXT SDW'S PLACE
         LA    R5,4(,R5)          REST OF THE DATA PLUS ITS SDW
         STM   R4,R5,KEPTREC      FOR THE NEXT PASS
         OI    IOPFLAGS,IOFLSDW   SHOW RECORD INCOMPLETE
         L     R6,BUFFCURR        END OF THE BLOCK, FOR WRITPREP
         B     WRITEVNU           GO FOR MORE
         SPACE 1
WRITEVAR LA    R1,4(,R5)          GET RECORD + BDW LENGTH
         C     R1,BLKSIZE         VALID SIZE?
         BH    WRITEBAD           NO; TAKE A DIVE
         L     R9,BUFFEND         GET BUFFER END
         SR    R9,R6              LESS CURRENT POSITION
         CR    R5,R9              WILL IT FIT ?
         BH    WRITEVNU           NO; WRITE NOW AND RECURSE
WRITEVAS LR    R7,R5              IN LENGTH = MOVE LENGTH
         MVCL  R6,R4              MOVE USER'S RECORD
         ST    R6,BUFFCURR        UPDATE NEXT AVAILABLE
         SR    R6,R8              LESS START
         STH   R6,0(,R8)          UPDATE BDW
         OI    IOPFLAGS,IOFLDATA  SHOW WRITE DATA IN BUFFER
         TM    DCBRECFM,DCBRECBR  BLOCKED?
         BNZ   WRITEEX            YES, NORMAL
         FIXWRITE ,               RECFM=V - WRITE IMMEDIATELY
         B     WRITEEX
         SPACE 1
WRITEVNU OI    IOPFLAGS,IOFCURSE  SET RECURSION REQUEST
         B     WRITPREP           SET ADDRESS/LENGTH TO WRITE
         SPACE 1
WRITEBAD ABEND 002,DUMP           INVALID REQUEST
         SPACE 1
WRITEFIX ICM   R6,15,BUFFCURR     Get next available record
         BNZ   WRITEFAP           Not first
         L     R6,BUFFADDR        Get buffer start
WRITEFAP L     R7,LRECL           Record length
         ICM   R5,8,=C' '         Request blank padding
         MVCL  R6,R4              Copy record to buffer
         ST    R6,BUFFCURR        Update new record address
         OI    IOPFLAGS,IOFLDATA  SHOW DATA IN BUFFER
         C     R6,BUFFEND         Room for more ?
         BL    WRITEEX            YES; RETURN
WRITPREP L     R4,BUFFADDR        Start write address
         LR    R5,R6              Current end of block
         SR    R5,R4              Current length
*NEXT*   B     WRITBLK            WRITE THE BLOCK
         SPACE 1
WRITBLK  AR    R5,R4              Set start and end of write
         STM   R4,R5,BUFFADDR     Pass to physical writer
         OI    IOPFLAGS,IOFLDATA  SHOW DATA IN BUFFER
         FIXWRITE ,               Write physical block
         B     WRITEEX            AND RETURN
         SPACE 1
TPUTWRIT CLI   RECFMIX,4          RECFM=V ?
         BE    TPUTWRIV           YES
         SH    R4,=H'4'           BACK UP TO RDW
         LA    R5,4(,R5)          LENGTH WITH RDW
TPUTWRIV STH   R5,0(,R4)          FILL RDW
         STCM  R5,12,2(R4)          ZERO REST
         L     R6,ZIOECT          RESTORE ECT ADDRESS
         L     R7,ZIOUPT          RESTORE UPT ADDRESS
         GO24
         PUTLINE PARM=ZPUTLINE,ECT=(R6),UPT=(R7),ECB=ZIOECB,           *
               OUTPUT=((R4),DATA),TERMPUT=EDIT,MF=(E,ZIOPL)
         GO31
         SPACE 1
WRITEEX  TM    IOPFLAGS,IOFCURSE  RECURSION REQUESTED?
         BNZ   WRITMORE
         TM    IOSFLAGS,IOFX37    Out of space on this data set?
         BNZ   WRITENOS           Yes; its own rc, see below (#176)
         TM    IOSFLAGS,IOFSYNAD  I/O error during physical write?
         BNZ   WRITEIOE           Yes; hand it to the caller (#147)
         FUNEXIT RC=0
         SPACE 1
WRITEIOE NI    IOSFLAGS,255-IOFSYNAD  Reset; error goes to the caller
         OI    IOSFLAGS,IOFWFAIL  ... and no STOW at CLOSE (#524)
         FUNEXIT RC=8
         SPACE 1
*  The x37 exit ran and asked IFG0554T for RC=1, so SYNAD was driven
*  and IOFSYNAD is set too - test IOFX37 FIRST and clear both.  A
*  separate return code because the C layer has to tell ENOSPC from
*  EIO: out of space is the caller's problem to solve, a bad track is
*  not, and #149 needs the difference (#176).
WRITENOS NI    IOSFLAGS,255-IOFSYNAD-IOFX37  Reset both
         OI    IOSFLAGS,IOFWFAIL  ... and no STOW at CLOSE (#524)
         FUNEXIT RC=12
*
         LTORG ,
         SPACE 2
         COPY  CLIBSUPA
*
         SPACE 2
         PRINT NOGEN
         IHAPSA ,            MAP LOW STORAGE
         CVT DSECT=YES
         IKJTCB ,            MAP TASK CONTROL BLOCK
         IKJECT ,            MAP ENV. CONTROL BLOCK
         IKJPTPB ,           PUTLINE PARAMETER BLOCK
         IKJCPPL ,
         IKJPSCB ,
         IEZJSCB ,
         IEZIOB ,
         IEFZB4D0 ,          MAP SVC 99 PARAMETER LIST
         IEFZB4D2 ,          MAP SVC 99 PARAMETERS
         IEFUCBOB ,
MYTIOT   DSECT ,
         IEFTIOT1 ,
         IHAPDS PDSBLDL=YES
         SPACE 1
         IFGACB ,                                               GP14233
         SPACE 1
         IFGRPL ,                                               GP14233
         IEFJESCT ,
         IKJUPT ,
         END
