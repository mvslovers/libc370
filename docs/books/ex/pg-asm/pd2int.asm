*        int pd2int(const void *pd, int len)
*        Returns the value of the packed decimal field PD, LEN bytes
*        (1 to 8). Reentrant: the work area is taken from the C stack.
         CSECT
         ENTRY PD2INT
PD2INT   DS    0H
         STM   14,12,12(13)       save the caller's registers
         LR    12,15              base register
         USING PD2INT,12
         L     15,76(,13)         next free byte of the C stack
         ST    13,4(,15)          chain the save areas
         ST    15,8(,13)
         LR    13,15              R13: our frame
         LA    15,96(,15)         frame of 96 bytes
         ST    15,76(,13)         new next free byte
*
         L     2,0(,1)            PD
         L     3,4(,1)            LEN
         BCTR  3,0                length code for EX
         EX    3,ZAP              ZAP 88(8,13),0(len,2)
         CVB   15,88(,13)         result in R15
*
         L     13,4(,13)          back to the caller's save area
         L     14,12(,13)
         LM    0,12,20(13)        restore all but R15
         BR    14
ZAP      ZAP   88(8,13),0(0,2)    executed with the length in R3
         END
