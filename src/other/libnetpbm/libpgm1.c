/* libpgm1.c - pgm utility library part 1
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

/* See libpbm.c for the complicated explanation of this 32/64 bit file
   offset stuff.
*/
#define _FILE_OFFSET_BITS 64
#define _LARGE_FILES  

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "pgm.h"
#include "libpgm.h"
#include "pbm.h"
#include "libpbm.h"
#include "pam.h"
#include "libpam.h"
#include "fileio.h"
#include "mallocvar.h"


gray *
pgm_allocrow(unsigned int const cols) {

    gray * grayrow;

    MALLOCARRAY(grayrow, cols);

    if (grayrow == NULL)
        pm_error("Unable to allocate space for a %u-column gray row", cols);

    return grayrow;
}



void 
pgm_init(int *   const argcP,
         char ** const argv) {

    pbm_init( argcP, argv );
}



void
pgm_nextimage(FILE * const file, int * const eofP) {
    pm_nextimage(file, eofP);
}



void
pgm_readpgminitrest(FILE * const fileP, 
                    int *  const colsP, 
                    int *  const rowsP, 
                    gray * const maxvalP) {

    gray maxval;
    
    /* Read size. */
    *colsP = (int)pm_getuint(fileP);
    *rowsP = (int)pm_getuint(fileP);

    /* Read maxval. */
    maxval = pm_getuint(fileP);
    if (maxval > PGM_OVERALLMAXVAL)
        pm_error("maxval of input image (%u) is too large.  "
                 "The maximum allowed by PGM is %u.", 
                 maxval, PGM_OVERALLMAXVAL);
    if (maxval == 0)
        pm_error("maxval of input image is zero.");

    *maxvalP = maxval;
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
pgm_readpgminit(FILE * const fileP,
                int *  const colsP, 
                int *  const rowsP,
                gray * const maxvalP,
                int *  const formatP) {

    int realFormat;

    /* Check magic number. */
    realFormat = pm_readmagicnumber(fileP);
    switch (PAM_FORMAT_TYPE(realFormat)) {
    case PGM_TYPE:
        *formatP = realFormat;
        pgm_readpgminitrest(fileP, colsP, rowsP, maxvalP);
        break;
        
    case PBM_TYPE:
        *formatP = realFormat;
        pbm_readpbminitrest(fileP, colsP, rowsP);
        
        /* Mathematically, it makes the most sense for the maxval of a PBM
           file seen as a PGM to be 1.  But we tried this for a while and
           found that it causes unexpected results and frequent need for a
           Pnmdepth stage to convert the maxval to 255.  You see, when you
           transform a PGM file in a way that causes interpolated gray shades,
           there's no in-between value to use when maxval is 1.  It's really
           hard even to discover that your lack of Pnmdepth is your problem.
           So we pick 255, which is the most common PGM maxval, and the highest
           resolution you can get without increasing the size of the PGM 
           image.

           So this means some programs that are capable of exploiting the
           bi-level nature of a PBM file must be PNM programs instead of PGM
           programs.
        */
        
        *maxvalP = PGM_MAXMAXVAL;
        break;

    case PAM_TYPE:
        pnm_readpaminitrestaspnm(fileP, colsP, rowsP, maxvalP, formatP);

        if (PAM_FORMAT_TYPE(*formatP) != PGM_TYPE)
            pm_error("Format of PAM input is not consistent with PGM");

        break;

    default:
        pm_error("bad magic number - not a pgm or pbm file");
    }
    validateComputableSize(*colsP, *rowsP);
}



gray
pgm_getrawsample(FILE * const file,
                 gray   const maxval) {

    if (maxval < 256) {
        /* The sample is just one byte.  Read it. */
        return(pm_getrawbyte(file));
    } else {
        /* The sample is two bytes.  Read both. */
        unsigned char byte_pair[2];
        size_t pairs_read;

        pairs_read = fread(&byte_pair, 2, 1, file);
        if (pairs_read == 0) 
            pm_error("EOF /read error while reading a long sample");
        /* This could be a few instructions faster if exploited the internal
           format (i.e. endianness) of a pixval.  Then we might be able to
           skip the shifting and oring.
           */
        return((byte_pair[0]<<8) | byte_pair[1]);
    }
}



void
pgm_readpgmrow(FILE * const file,
               gray * const grayrow, 
               int    const cols,
               gray   const maxval,
               int    const format) {

    switch (format) {
    case PGM_FORMAT: {
        unsigned int col;
        for (col = 0; col < cols; ++col) {
            grayrow[col] = pm_getuint(file);
            if (grayrow[col] > maxval)
                pm_error("value out of bounds (%u > %u)",
                         grayrow[col], maxval);
        }
    }
    break;
        
    case RPGM_FORMAT: {
        unsigned int const bytesPerSample = maxval < 256 ? 1 : 2;
        int          const bytesPerRow    = cols * bytesPerSample;

        unsigned char * rowBuffer;
        ssize_t rc;

        MALLOCARRAY(rowBuffer, bytesPerRow);
        if (rowBuffer == NULL)
            pm_error("Unable to allocate memory for row buffer "
                     "for %u columns", cols);

        rc = fread(rowBuffer, 1, bytesPerRow, file);
        if (rc == 0)
            pm_error("Error reading row.  fread() errno=%d (%s)",
                     errno, strerror(errno));
        else if (rc != bytesPerRow)
            pm_error("Error reading row.  Short read of %u bytes "
                     "instead of %u", rc, bytesPerRow);

        if (maxval < 256) {
            unsigned int col;
            for (col = 0; col < cols; ++col)
                grayrow[col] = (gray)rowBuffer[col];
        } else {
            unsigned int bufferCursor;
            unsigned int col;

            bufferCursor = 0;  /* Start at beginning of rowBuffer[] */

            for (col = 0; col < cols; ++col) {
                gray g;

                g = rowBuffer[bufferCursor++] << 8;
                g |= rowBuffer[bufferCursor++];

                grayrow[col] = g;
            }
        }
        free(rowBuffer);
    }
        break;
    
    case PBM_FORMAT:
    case RPBM_FORMAT: {
        bit * bitrow;
        int col;

        bitrow = pbm_allocrow(cols);
        pbm_readpbmrow( file, bitrow, cols, format );
        for (col = 0; col < cols; ++col)
            grayrow[col] = (bitrow[col] == PBM_WHITE ) ? maxval : 0;
        pbm_freerow(bitrow);
    }
        break;
        
    default:
        pm_error( "can't happen" );
    }
}



gray **
pgm_readpgm(FILE * const file,
            int *  const colsP,
            int *  const rowsP, 
            gray * const maxvalP) {

    gray** grays;
    int row;
    int format;

    pgm_readpgminit( file, colsP, rowsP, maxvalP, &format );
    
    grays = pgm_allocarray( *colsP, *rowsP );
    
    for ( row = 0; row < *rowsP; ++row )
        pgm_readpgmrow( file, grays[row], *colsP, *maxvalP, format );
    
    return grays;
}



void
pgm_check(FILE *               const file, 
          enum pm_check_type   const check_type, 
          int                  const format, 
          int                  const cols, 
          int                  const rows, 
          gray                 const maxval,
          enum pm_check_code * const retval_p) {

    if (rows < 0)
        pm_error("Invalid number of rows passed to pgm_check(): %d", rows);
    if (cols < 0)
        pm_error("Invalid number of columns passed to pgm_check(): %d", cols);
    
    if (check_type != PM_CHECK_BASIC) {
        if (retval_p) *retval_p = PM_CHECK_UNKNOWN_TYPE;
    } else if (PGM_FORMAT_TYPE(format) == PBM_TYPE) {
        pbm_check(file, check_type, format, cols, rows, retval_p);
    } else if (format != RPGM_FORMAT) {
        if (retval_p) *retval_p = PM_CHECK_UNCHECKABLE;
    } else {        
        pm_filepos const bytes_per_row = cols * (maxval > 255 ? 2 : 1);
        pm_filepos const need_raster_size = rows * bytes_per_row;
        
        pm_check(file, check_type, need_raster_size, retval_p);
        
    }
}
