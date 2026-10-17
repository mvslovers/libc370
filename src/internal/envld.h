#ifndef SRC_INTERNAL_ENVLD_H
#define SRC_INTERNAL_ENVLD_H
/* src/internal/envld.h - internal: loading the environment from a DD. */

/* __envln() - one line of a SYSENV-style data set: "* " and "# " lines
   are comments, sequence numbers in the last 8 columns and trailing
   blanks are removed, and a "name=value" line is passed to putenv().
   buf has no newline; it is changed in place. */
extern void __envln(char *buf);

/* __envdd() - load the environment from DD ddname (8 characters, blank
   padded) through the BSAM layer, without the stream functions (size
   analysis F3).  0 when the DD was read, nonzero when it could not be
   opened, as loadenv() answers. */
extern int  __envdd(const char *ddname);

#endif /* SRC_INTERNAL_ENVLD_H */
