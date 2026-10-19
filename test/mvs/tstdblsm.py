#!/usr/bin/env python3
"""tstdblsm.py - judge the %.16e lines test/mvs/tstdblsm.c printed (#225).

Each line is the 16 hex digits of a System/370 double and its %.16e text.
The exact value of the double is sign * 0.F * 16**(E-64); the script
compares the printed significand and exponent with it and reports how many
significant digits agree and the error in units of the 17th digit.

Usage: tstdblsm.py OUTPUT [OUTPUT ...]   (one summary per file)
"""
import re
import sys
from fractions import Fraction

LINE = re.compile(r'^([0-9A-F]{16}) (-?\d\.\d{16})[eE]([-+]\d+)\s*$')


def exact(bits):
    w = int(bits, 16)
    sign = -1 if w >> 63 else 1
    e = (w >> 56) & 0x7F
    f = w & ((1 << 56) - 1)
    return sign * Fraction(f, 1 << 56) * Fraction(16) ** (e - 64)


def judge(path):
    worst = 0
    bad15 = 0
    n = 0
    hist = {}
    for line in open(path, errors='replace'):
        m = LINE.match(line.strip())
        if not m:
            continue
        n += 1
        x = exact(m.group(1))
        printed = Fraction(m.group(2)) * Fraction(10) ** int(m.group(3))
        if x == 0:
            continue
        # error in units of the 17th significant digit of the exact value
        mag = abs(x)
        k = 0
        while mag >= 10:
            mag /= 10
            k += 1
        while mag < 1:
            mag *= 10
            k -= 1
        unit = Fraction(10) ** (k - 16)
        err = abs(printed - x) / unit
        u = int(err)
        worst = max(worst, u)
        hist[min(u, 100)] = hist.get(min(u, 100), 0) + 1
        # the first 15 digits: %.14e correctly rounded from the exact value
        want = round(abs(x) / Fraction(10) ** (k - 14))
        got = round(abs(printed) / Fraction(10) ** (k - 14))
        if want != got:
            bad15 += 1
    print(f"{path}: {n} values, worst error {worst} units of the 17th digit, "
          f"{bad15} with a wrong 15-digit rounding")
    print("   error (units of digit 17) : count  " +
          ", ".join(f"{k}:{v}" for k, v in sorted(hist.items())))
    return bad15


if __name__ == '__main__':
    sys.exit(1 if any(judge(p) for p in sys.argv[1:]) else 0)
