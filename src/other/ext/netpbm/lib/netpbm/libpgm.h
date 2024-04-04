/* This is the intra-libnetpbm interface header file for libpgm*.c
*/

#ifndef LIBPGM_H_INCLUDED
#define LIBPGM_H_INCLUDED

#include "pgm.h"

void
pgm_readpgminitrest(FILE * const file, 
                    int *  const colsP, 
                    int *  const rowsP, 
                    gray * const maxvalP);

#endif
