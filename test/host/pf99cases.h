/*
 * pf99cases.h - the cases of tstpf99 (#578), shared by the host test
 * (test/host/tstpf99.c, against the host's snprintf) and the MVS probe
 * (test/mvs/tstpf99.c, which prints libc370's output for the same cases
 * so that it can be compared with the host build of the same formatter).
 */
static const int ints[] = { 0, 1, -1, 7, -42, 255, 0xbeef, INT_MAX, INT_MIN };
static const char *ifmts[] = {
    "%d", "%i", "%5d", "%-5d", "%05d", "%+d", "% d", "%+05d", "%-+6d",
    "%.0d", "%.3d", "%8.3d", "%-8.3d", "%08.3d", "%hd", "%hhd", "%ld",
};
static const char *ufmts[] = {
    "%u", "%o", "%x", "%X", "%#o", "%#x", "%#X", "%8x", "%-8x", "%08x",
    "%.0x", "%.5x", "%#.5x", "%#8x", "%#08x", "%hu", "%hhu", "%hx", "%lu",
    "%.0o", "%#.0o", "%#5o",
};
static const double dbls[] = {
    0.0, 1.0, -1.0, 0.5, 2.5, 3.25, 100.0, 123.456, -123.456, 0.001,
    1e-5, 1.5e-7, 12345.678, 100000.0, 1e6, 1234567.0, 1e10, 9.9999995,
    0.000123456, 99999.95,
};
static const char *dfmts[] = {
    "%f", "%.0f", "%.1f", "%.3f", "%10.2f", "%-10.2f", "%010.2f", "%+f",
    "% f", "%#.0f",
    "%e", "%E", "%.0e", "%.2e", "%12.3e", "%+e", "%#.0e",
    "%g", "%G", "%.0g", "%.1g", "%.3g", "%.10g", "%10g", "%-10g", "%+g",
    "%#g", "%#.3g",
};
static const char *strs[] = { "", "a", "abc", "hello world" };
static const char *sfmts[] = { "%s", "%5s", "%-5s", "%.2s", "%8.2s", "%-8.2s", "%.0s" };

