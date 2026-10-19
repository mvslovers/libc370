#!/usr/bin/env python3
"""tstatan.py - write test/mvs/tstatancase.h, the cases of tstatan.c (#675).

Each row is an argument and the value Python's math library gives for it
(IEEE double, correctly rounded or within an ulp).  The MVS probe compares
its System/370 result with that reference; HFP double carries 53 to 56
bits, so the probe accepts a relative error of 1e-14.

Groups: P atan() of positive arguments, N atan() of negative ones, S asin()
and acos(), Q atan2() in all four quadrants and on the axes.

Usage: python3 test/mvs/tstatan.py > test/mvs/tstatancase.h
"""
import math

S3 = math.sqrt(3.0)


def red(x):
    """The value atan()'s pi/6 reduction turns x (tan 15 < x <= 1) into."""
    return (x * S3 - 1.0) / (x + S3)


# positive arguments, the ones that returned before #675 first, then the
# band whose reduction comes out negative (tan 15 .. 1/sqrt3 and their
# reciprocals), which looped; last the ones whose series reached the
# exponent underflow
POS = ["0.0", "1e-12", "1e-9", "1e-8", "0.06", "0.1", "0.25",
       "0.2679", "0.6", "0.75", "1.0", "1.5", "1.7", "3.8", "10.0",
       "100.0", "1e6",
       "0.27", "0.3", "0.5", "0.577", "1.75", "2.0", "3.0", "3.7",
       "1e-30", "1e30", "1e70"]
REDS = [red(0.3), red(0.5), red(0.577)]
NEG = ["-0.06", "-0.1", "-0.25", "-0.3", "-0.5", "-1.0", "-2.0", "-3.0",
       "-10.0", "-1e6", "-1e30", "-1e70", "-1e-30", "-1e-8"]
ASIN = ["0.5", "0.75", "0.9", "1.0", "-0.5", "-0.75", "-0.9", "-1.0"]
ACOS = ["0.5", "-0.5", "-0.9", "-1.0"]
QUAD = [("1.0", "1.0"), ("1.0", "-1.0"), ("-1.0", "-1.0"), ("-1.0", "1.0"),
        ("1.0", "2.0"), ("2.0", "1.0"), ("2.0", "-1.0"), ("1.0", "-2.0"),
        ("-1.0", "-2.0"), ("-2.0", "-1.0"), ("-2.0", "1.0"), ("-1.0", "2.0"),
        ("0.5", "-3.0"), ("-0.5", "-3.0"),
        ("0.0", "1.0"), ("1.0", "0.0"), ("0.0", "-1.0"), ("-1.0", "0.0")]


def row(grp, fn, label, a, b, want):
    return '    { \'%s\', %d, "%s", %s, %s, %r },' % (grp, fn, label, a, b,
                                                     want)


def main():
    out = ["/* tstatancase.h - written by test/mvs/tstatan.py; do not edit.",
           " * group, function (0 atan, 1 asin, 2 acos, 3 atan2), label,",
           " * argument (atan2: y), second argument (atan2: x), reference. */"]
    for a in POS:
        out.append(row("P", 0, a, a, "0.0", math.atan(float(a))))
    for v in REDS:
        lbl = "%.6f" % v
        out.append(row("N", 0, lbl, repr(v), "0.0", math.atan(v)))
    for a in NEG:
        out.append(row("N", 0, a, a, "0.0", math.atan(float(a))))
    for a in ASIN:
        out.append(row("S", 1, a, a, "0.0", math.asin(float(a))))
    for a in ACOS:
        out.append(row("S", 2, a, a, "0.0", math.acos(float(a))))
    for y, x in QUAD:
        out.append(row("Q", 3, y + "," + x, y, x,
                       math.atan2(float(y), float(x))))
    print("\n".join(out))


main()
