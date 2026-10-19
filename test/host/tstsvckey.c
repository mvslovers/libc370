/*
 * tstsvckey.c - libc370 #634: svcmain()'s SVCMAIN_KEY sets the key the
 * caller asked for, also when the caller gave it in the low four bits.
 *
 * ISSUE #634: "if (svcregs->r15 < 0x10) svcregs->r15 << 4;" was an
 * expression statement - it shifted nothing - so a request for key 8 given
 * as 0x08 put 0x08 & 0xF0 = 0 into the caller's PSW: key 0.
 *
 * This #includes the real svcmain.c and calls svcmain() with an SVCREGS
 * and a request block of its own; the function only reads and writes
 * those two.  Byte 1 of the old PSW is key (high four bits) and flags.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -Wno-missing-braces \
 *        -Wno-missing-field-initializers -Wno-unused-parameter \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstsvckey.c && ./t
 *
 * RED, 2026-10-19, against main's svcmain.c: key 0x08 set key 0 (PSW
 * byte 01).  GREEN 4/4.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include "../../src/mvs/svc/svcmain.c"

static int bad;

/* the caller's old PSW byte 1 after an SVCMAIN_KEY request for key */
static unsigned keyafter(unsigned key, unsigned char b2, unsigned *rc)
{
    RBBASIC     rb      = {0};
    SVCREGS     regs    = {0};
    unsigned    r15 = 0, r0 = 0, r1 = 0;

    rb.RBOPSWB2 = b2;
    regs.rblink = &rb;
    regs.r1 = SVCMAIN_KEY;
    regs.r15 = key;
    svcmain(&regs, &r15, &r0, &r1);
    *rc = r15;
    return (unsigned char)rb.RBOPSWB2;
}

static void check(int cond, const char *what)
{
    printf("  %s  %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    unsigned rc, b;

    b = keyafter(0x80, 0x81, &rc);
    check(b == 0x81 && rc == 0x80, "key 0x80 (high bits): key 8, flags kept");
    b = keyafter(0x08, 0x01, &rc);
    printf("        key 0x08 -> PSW byte %02X\n", b);
    check(b == 0x81 && rc == 0x00, "key 0x08 (low bits): key 8, flags kept");
    b = keyafter(0x00, 0x81, &rc);
    check(b == 0x01 && rc == 0x80, "key 0x00: key 0");
    b = keyafter(PSWKEYNONE, 0x81, &rc);
    check(b == 0x81 && rc == 0x80, "PSWKEYNONE leaves the key");
    printf("tstsvckey: %s\n", bad ? "FAILED" : "passed");
    return bad ? 1 : 0;
}
