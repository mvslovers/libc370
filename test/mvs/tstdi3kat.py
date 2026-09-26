#!/usr/bin/env python3
"""Regenerate the known-answer table `kat[]` in test/mvs/tstdi3.c (#187).

Python integers are arbitrary precision, so every value here is exact and
independent of the helpers under test.  Signed division truncates toward
zero and the remainder takes the dividend's sign (C99 6.5.5); where the
divisor is 0 the division columns are 0 and the test skips them.

    python3 test/mvs/tstdi3kat.py > kat.inc
"""
M = (1 << 64) - 1


def signed(x):
    return x - (1 << 64) if x >> 63 else x


def tdiv(a, b):
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


EDGES = [0x0, 0x1, 0x3, 0xA, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF,
         0x100000000, 0x123456789ABCDEF0, 0x7FFFFFFFFFFFFFFF,
         0x8000000000000000, 0xFFFFFFFF00000000, 0xFFFFFFFFFFFFFFFD,
         0xFFFFFFFFFFFFFFFF]


def words(x):
    return "0x%08X,0x%08X" % (x >> 32, x & 0xFFFFFFFF)


for a in EDGES:
    for b in EDGES:
        mul, neg = (a * b) & M, (-a) & M
        if b == 0:
            ud = um = sd = sm = 0
        else:
            ud, um = a // b, a % b
            sa, sb = signed(a), signed(b)
            q = tdiv(sa, sb)
            sd, sm = q & M, (sa - q * sb) & M
        print("    { %s, %s, %s, %s, %s, %s, %s, %s }," % tuple(
            words(v) for v in (a, b, mul, neg, ud, um, sd, sm)))
