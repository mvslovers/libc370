/* JESVOL.H - private to the JES2 sources: find a data set by its name on
 * the online DASD volumes (#142).
 */
#ifndef JESVOL_H
#define JESVOL_H

/* __jesvol() - OBTAIN (SEARCH) dsn in the VTOC of each online DASD volume,
 * in UCB order, and return the first volume that has it in volser (a
 * string of up to 6 characters).  The catalog is not consulted, so an
 * uncataloged data set is found as well as a cataloged one.
 *
 * Returns 0 when found, 1 when no online volume has the data set, -1 when
 * the volume list could not be built (errno ENOMEM) or dsn is no name.
 * *scanned (may be NULL) is set to the number of volumes searched. */
int __jesvol(const char *dsn, char volser[7], unsigned *scanned);

#endif
