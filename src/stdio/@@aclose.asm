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
*  ACLOSE - Close a data set                                          *
*                                                                     *
***********************************************************************
@@ACLOSE FUNHEAD IO=YES,SAVE=(WORKAREA,WORKLEN,SUBPOOL)  CLOSE
         SR    R9,R9              RC=0 unless the final block failed
         TM    IOMFLAGS,IOFTERM   TERMINAL I/O MODE?
         BNZ   FREEBUFF           YES; JUST FREE STUFF
         FIXWRITE ,          WRITE FINAL BUFFER, IF ONE
*
*  The final block goes out here and not through @@AWRITE, so its
*  failure has to be reported here too - the same RC as @@AWRITE:
*  12 the x37 exit took an out-of-space, 8 SYNAD took an I/O error.
*  Test IOFX37 first, the exit drives SYNAD as well (#176).  @@AWRITE
*  and @@AREAD reset what they report, so a flag still set here is a
*  failed write nobody has reported - this one, or a FIXWRITE in
*  @@ANOTE/@@APOINT/@@AREAD - and fclose() is the last chance to.  R9
*  survives the CLOSE and the FREEMAINs below; the flags do not, they
*  live in the work area freed at the end (#182).
*
         TM    IOSFLAGS,IOFX37    Out of space on the final block?
         BZ    CLOSSYN            No
         LA    R9,12              Yes; ENOSPC to the C layer
         B     FREEBUFF
CLOSSYN  TM    IOSFLAGS,IOFSYNAD  I/O error on the final block?
         BZ    FREEBUFF           No
         LA    R9,8               Yes; EIO to the C layer
*
*  Each buffer pointer is cleared before its FREEMAIN.  A CLOSE that
*  abends (SB14 from the STOW, say) leaves this work area alive, and
*  __fabandon() runs @@ACLOSE on it a second time: stale pointers
*  made that a second FREEMAIN of storage that could belong to
*  someone else by then - its own save area, measured: SA0A (#705).
*
FREEBUFF LM    R1,R2,ZBUFF1       Look at first buffer
         LTR   R0,R2              Any ?
         BZ    FREEDBF1           No
         XC    ZBUFF1(8),ZBUFF1   Freed: nothing left to free again
         FREEMAIN RC,LV=(0),A=(1),SP=SUBPOOL  Free BLOCK buffer
FREEDBF1 LM    R1,R2,ZBUFF2       Look at first buffer
         LTR   R0,R2              Any ?
         BZ    FREEDBF2           No
         XC    ZBUFF2(8),ZBUFF2   Freed: nothing left to free again
         FREEMAIN RC,LV=(0),A=(1),SP=SUBPOOL  Free RECRD buffer
FREEDBF2 TM    IOMFLAGS,IOFTERM   TERMINAL I/O MODE?
         BNZ   NOPOOL             YES; SKIP CLOSE/FREEPOOL
*
*  A member whose write failed - the final block here (R9), or an
*  earlier one @@AWRITE reported (IOFWFAIL) - must not be STOWed:
*  CLOSE would put a new member with what reached DASD into the
*  directory, or replace an existing one with it, its old content
*  lost (#524, measured).  DCBCNSTO says the STOW is done; the DASD
*  close executor IGG0201Z then skips its own.  The space written
*  stays in the PDS unused, as for any replaced member.
*
         LTR   R9,R9              The final block failed?
         BNZ   NOSTOW             Yes
         TM    IOSFLAGS,IOFWFAIL  An earlier write failed?
         BZ    DOCLOSE            No; CLOSE as always
NOSTOW   OI    DCBCIND2,DCBCNSTO  No STOW by CLOSE (#524)
DOCLOSE  CLOSE MF=(E,OPENCLOS)
         TM    DCBBUFCA+L'DCBBUFCA-1,1      BUFFER POOL?
         BNZ   NOPOOL             NO, INVALIDATED
         SR    R15,R15
         ICM   R15,7,DCBBUFCA     DID WE GET A BUFFER?
         BZ    NOPOOL             0-NO
         FREEPOOL ((R10))
NOPOOL   DS    0H
         FREEMAIN R,LV=ZDCBLEN,A=(R10),SP=SUBPOOL
         FUNEXIT RC=(R9)
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
