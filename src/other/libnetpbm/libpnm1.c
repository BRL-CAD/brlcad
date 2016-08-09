/* libpnm1.c - pnm utility library part 1
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <string.h>
#include <errno.h>

#include "pnm.h"

#include "ppm.h"
#include "libppm.h"

#include "pgm.h"
#include "libpgm.h"

#include "pbm.h"
#include "libpbm.h"

#include "pam.h"
#include "libpam.h"

#include "mallocvar.h"



xel *
pnm_allocrow(unsigned int const cols) {

    xel * xelrow;

    MALLOCARRAY(xelrow, cols);

    if (xelrow == NULL)
        pm_error("Unable to allocate space for a %u-column xel row", cols);

    return xelrow;
}



void
pnm_init( argcP, argv )
    int* argcP;
    char* argv[];
    {
    ppm_init( argcP, argv );
    }

void
pnm_nextimage(FILE *file, int * const eofP) {
    pm_nextimage(file, eofP);
}



static void
validateComputableSize(unsigned int const cols,
                       unsigned int const rows) {
/*----------------------------------------------------------------------------
   Validate that the dimensions of the image are such that it can be
   processed in typical ways on this machine without worrying about
   overflows.  Note that in C, arithmetic is always modulus
   arithmetic, so if your values are too big, the result is not what
   you expect.  That failed expectation can be disastrous if you use
   it to allocate memory.

   A common operation is adding 1 or 2 to the highest row or
   column number in the image, so we make sure that's possible.
-----------------------------------------------------------------------------*/
    if (cols > INT_MAX - 2)
        pm_error("image width (%u) too large to be processed", cols);
    if (rows > INT_MAX - 2)
        pm_error("image height (%u) too large to be processed", rows);
}



void
pnm_readpnminit(FILE *   const fileP,
                int *    const colsP,
                int *    const rowsP,
                xelval * const maxvalP,
                int *    const formatP) {

    int realFormat;

    /* Check magic number. */
    realFormat = pm_readmagicnumber(fileP);
    switch (PAM_FORMAT_TYPE(realFormat)) {
    case PPM_TYPE: {
        pixval maxval;
        *formatP = realFormat;
        ppm_readppminitrest(fileP, colsP, rowsP, &maxval);
        *maxvalP = maxval;
    }
    break;

    case PGM_TYPE: {
        gray maxval;

        *formatP = realFormat;
        pgm_readpgminitrest(fileP, colsP, rowsP, &maxval);
        *maxvalP = maxval;
    }
    break;

    case PBM_TYPE:
        *formatP = realFormat;
        pbm_readpbminitrest(fileP, colsP, rowsP);
        *maxvalP = 1;
    break;

    case PAM_TYPE: {
        gray maxval;
        pnm_readpaminitrestaspnm(fileP, colsP, rowsP, &maxval, formatP);
        *maxvalP = maxval;
    }
    break;

    default:
        pm_error("bad magic number - not a ppm, pgm, or pbm file");
    }
    validateComputableSize(*colsP, *rowsP);
}



void
pnm_readpnmrow(FILE * const fileP,
               xel *  const xelrow,
               int    const cols,
               xelval const maxval,
               int    const format) {

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        ppm_readppmrow(fileP, (pixel*) xelrow, cols, (pixval) maxval, format);
        break;

    case PGM_TYPE: {
        gray * grayrow;
        unsigned int col;

        grayrow = pgm_allocrow(cols);
        pgm_readpgmrow(fileP, grayrow, cols, (gray) maxval, format);
        for (col = 0; col < cols; ++col)
            PNM_ASSIGN1(xelrow[col], grayrow[col]);
        pgm_freerow(grayrow);
    }
    break;
        
    case PBM_TYPE: {
        bit * bitrow;
        unsigned int col;
        bitrow = pbm_allocrow(cols);
        pbm_readpbmrow(fileP, bitrow, cols, format);
        for (col = 0; col < cols; ++col)
            PNM_ASSIGN1(xelrow[col], bitrow[col] == PBM_BLACK ? 0: maxval);
        pbm_freerow(bitrow);
    }
    break;

    default:
        pm_error("INTERNAL ERROR.  Impossible format.");
    }
}



xel **
pnm_readpnm(FILE *   const fileP,
            int *    const colsP,
            int *    const rowsP,
            xelval * const maxvalP,
            int *    const formatP) {

    xel ** xels;
    int row;

    pnm_readpnminit(fileP, colsP, rowsP, maxvalP, formatP);

    xels = pnm_allocarray(*colsP, *rowsP);

    for (row = 0; row < *rowsP; ++row)
        pnm_readpnmrow(fileP, xels[row], *colsP, *maxvalP, *formatP);

    return xels;
}



void
pnm_check(FILE *               const fileP,
          enum pm_check_type   const check_type, 
          int                  const format,
          int                  const cols,
          int                  const rows,
          int                  const maxval,
          enum pm_check_code * const retvalP) {

    switch (PNM_FORMAT_TYPE(format)) {
    case PBM_TYPE:
        pbm_check(fileP, check_type, format, cols, rows, retvalP);
        break;
    case PGM_TYPE: 
        pgm_check(fileP, check_type, format, cols, rows, maxval, retvalP);
        break;
    case PPM_TYPE:
        ppm_check(fileP, check_type, format, cols, rows, maxval, retvalP);
        break;
    default:
        pm_error("pnm_check() called with invalid format parameter");
    }
}
