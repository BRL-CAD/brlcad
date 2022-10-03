#ifndef INFO_H
#define INFO_H

#include "sc_export.h"
/** \file info.h
 * informative functions that were in express.c/express.h
 */

extern SC_EXPRESS_EXPORT char * EXPRESSversion( void );

/** print usage message, then exit if _exit is non-zero */
extern SC_EXPRESS_EXPORT void EXPRESSusage( int _exit );

#endif /* INFO_H */
