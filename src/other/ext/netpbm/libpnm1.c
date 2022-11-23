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

#include "mallocvar.h"

#include "pnm.h"

#include "ppm.h"
#include "libppm.h"

#include "pgm.h"
#include "libpgm.h"

#include "pbm.h"
#include "libpbm.h"

#include "pam.h"
#include "libpam.h"



xel *
pnm_allocrow(unsigned int const cols) {

    xel * xelrow;

    MALLOCARRAY(xelrow, cols);

    if (xelrow == NULL)
        pm_error("Unable to allocate space for a %u-column xel row", cols);

    return xelrow;
}



void
pnm_init(int * const argcP, char ** const argv) {
    ppm_init( argcP, argv );
}

void
pnm_nextimage(FILE * const file, int * const eofP) {
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

   It is very normal to allocate space for a pixel row, so we make sure
   the size of a pixel row, in bytes, can be represented by an 'int'.

   A common operation is adding 1 or 2 to the highest row or
   column number in the image, so we make sure that's possible.
-----------------------------------------------------------------------------*/
    if (cols > INT_MAX/(sizeof(pixval) * 3) || cols > INT_MAX - 2)
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
        pm_error("bad magic number 0x%x - not a PPM, PGM, PBM, or PAM file",
                 realFormat);
    }
    validateComputableSize(*colsP, *rowsP);
}



static void
readpgmrow(FILE * const fileP,
           xel *  const xelrow,
           int    const cols,
           xelval const maxval,
           int    const format) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    gray * grayrow;

    grayrow = pgm_allocrow(cols);

    if (setjmp(jmpbuf) != 0) {
        pgm_freerow(grayrow);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int col;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        pgm_readpgmrow(fileP, grayrow, cols, (gray) maxval, format);

        for (col = 0; col < cols; ++col)
            PNM_ASSIGN1(xelrow[col], grayrow[col]);

        pm_setjmpbuf(origJmpbufP);
    }
    pgm_freerow(grayrow);
}



static void
readpbmrow(FILE * const fileP,
               xel *  const xelrow,
               int    const cols,
               xelval const maxval,
               int    const format) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    bit * bitrow;

    bitrow = pbm_allocrow_packed(cols);

    if (setjmp(jmpbuf) != 0) {
        pbm_freerow_packed(bitrow);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int col;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        pbm_readpbmrow_packed(fileP, bitrow, cols, format);

        for (col = 0; col < cols; ++col) {
            pixval const g =
                ((bitrow[col/8] >> (7 - col%8)) & 0x1) == PBM_WHITE ?
                maxval : 0;
            PNM_ASSIGN1(xelrow[col], g);
        }
        pm_setjmpbuf(origJmpbufP);
    }
    pbm_freerow(bitrow);
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

    case PGM_TYPE:
        readpgmrow(fileP, xelrow, cols, maxval, format);
        break;
        
    case PBM_TYPE:
        readpbmrow(fileP, xelrow, cols, maxval, format);
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

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    int cols, rows;
    xelval maxval;
    int format;
    xel ** xels;

    pnm_readpnminit(fileP, &cols, &rows, &maxval, &format);

    xels = pnm_allocarray(cols, rows);

    if (setjmp(jmpbuf) != 0) {
        pnm_freearray(xels, rows);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int row;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        for (row = 0; row < rows; ++row)
            pnm_readpnmrow(fileP, xels[row], cols, maxval, format);

        pm_setjmpbuf(origJmpbufP);
    }
    *colsP = cols;
    *rowsP = rows;
    *maxvalP = maxval;
    *formatP = format;

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
