/* This is the intra-libnetpbm interface header file for libppm*.c
*/

#ifndef LIBPPM_H_INCLUDED
#define LIBPPM_H_INCLUDED

#include "ppm.h"

void
ppm_readppminitrest(FILE *   const file, 
                    int *    const colsP, 
                    int *    const rowsP, 
                    pixval * const maxvalP);

#endif
