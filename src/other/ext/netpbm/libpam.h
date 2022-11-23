/* This is the intra-libnetpbm interface header file for libpam.c
*/

#ifndef LIBPAM_H_INCLUDED
#define LIBPAM_H_INCLUDED

#include "pam.h"

void
pnm_readpaminitrestaspnm(FILE * const fileP, 
                         int *  const colsP, 
                         int *  const rowsP, 
                         gray * const maxvalP,
                         int *  const formatP);

#endif

