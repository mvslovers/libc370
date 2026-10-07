*        int eachchr(const char *s, int (*fn)(int c, void *arg),
*                    void *arg)
*        Calls FN(c, ARG) for each character of the string S until
*        FN returns non-zero. Returns that value, or 0 at the end.
         CSECT
         ENTRY EACHCHR
EACHCHR  DS    0H
         STM   14,12,12(13)
         LR    12,15
         USING EACHCHR,12
         L     15,76(,13)
         ST    13,4(,15)
         ST    15,8(,13)
         LR    13,15
         LA    15,96(,15)         save area, NAB, 2-word parm list
         ST    15,76(,13)
*
         LM    2,4,0(1)           R2: S  R3: FN  R4: ARG
         SR    15,15              result if the string is empty
LOOP     SR    5,5
         IC    5,0(,2)            next character
         LTR   5,5
         BZ    DONE
         ST    5,88(,13)          parameter list: c ...
         ST    4,92(,13)          ... and arg, both by value
         LA    1,88(,13)
         LR    15,3
         BALR  14,15              call fn
         LTR   15,15
         BNZ   DONE
         LA    2,1(,2)
         B     LOOP
*
DONE     L     13,4(,13)
         L     14,12(,13)
         LM    0,12,20(13)
         BR    14
         END
