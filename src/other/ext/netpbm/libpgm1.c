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

#include "mallocvar.h"


#include "pgm.h"
#include "libpgm.h"
#include "pbm.h"
#include "libpbm.h"
#include "pam.h"
#include "libpam.h"
#include "fileio.h"


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

    pbm_init(argcP, argv);
}



void
pgm_nextimage(FILE * const file,
              int *  const eofP) {
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

   It is very normal to allocate space for a pixel row, so we make sure
   the size of a pixel row, in bytes, can be represented by an 'int'.

   A common operation is adding 1 or 2 to the highest row or
   column number in the image, so we make sure that's possible.
-----------------------------------------------------------------------------*/
    if (cols > INT_MAX / (sizeof(gray)) || cols > INT_MAX - 2)
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

    case PGM_TYPE:
        *formatP = realFormat;
        pgm_readpgminitrest(fileP, colsP, rowsP, maxvalP);
        break;
        
    case PPM_TYPE:
        pm_error("Input file is a PPM, which this program cannot process.  "
                 "You may want to convert it to PGM with 'ppmtopgm'");

    case PAM_TYPE:
        pnm_readpaminitrestaspnm(fileP, colsP, rowsP, maxvalP, formatP);

        if (PAM_FORMAT_TYPE(*formatP) != PGM_TYPE)
            pm_error("Format of PAM input is not consistent with PGM");

        break;

    default:
        pm_error("bad magic number 0x%x - not a PPM, PGM, PBM, or PAM file",
                 realFormat);
    }
    validateComputableSize(*colsP, *rowsP);
}



static void
validateRpgmRow(gray *         const grayrow, 
                unsigned int   const cols,
                gray           const maxval,
                const char **  const errorP) {
/*----------------------------------------------------------------------------
  Check for sample values above maxval in input.  

  Note: a program that wants to deal with invalid sample values itself can
  simply make sure it uses a sufficiently high maxval on the read function
  call, so this validation never fails.
-----------------------------------------------------------------------------*/
    if (maxval == 255 || maxval == 65535) {
        /* There's no way a sample can be invalid, so we don't need to look at
           the samples individually.
        */
        *errorP = NULL;
    } else {
        unsigned int col;
        for (col = 0; col < cols; ++col) {
            if (grayrow[col] > maxval) {
                pm_error("gray value %u is greater than maxval (%u)",
                            grayrow[col], maxval);
                return;
            }
        }
        *errorP = NULL;
    }
}  



static void
readRpgmRow(FILE * const fileP,
            gray * const grayrow, 
            int    const cols,
            gray   const maxval,
            int    const format) {

    unsigned int const bytesPerSample = maxval < 256 ? 1 : 2;
    int          const bytesPerRow    = cols * bytesPerSample;
    
    unsigned char * rowBuffer;
    const char * error = NULL;
    
    MALLOCARRAY(rowBuffer, bytesPerRow);
    if (rowBuffer == NULL)
        pm_error("Unable to allocate memory for row buffer "
                    "for %u columns", cols);
    else {
        size_t rc;
        rc = fread(rowBuffer, 1, bytesPerRow, fileP);
        if (rc == 0)
            pm_error("Error reading row.  fread() errno=%d (%s)",
                        errno, strerror(errno));
        else if (rc != bytesPerRow)
            pm_error("Error reading row.  Short read of %u bytes "
                        "instead of %u", (unsigned)rc, bytesPerRow);
        else {
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
            validateRpgmRow(grayrow, cols, maxval, &error); 
        }
        free(rowBuffer);
    }
    if (error) {
        pm_error("%s", error);
        free((void *)error);
        pm_longjmp();
    }
} 



static void
readPbmRow(FILE * const fileP,
           gray * const grayrow, 
           int    const cols,
           gray   const maxval,
           int    const format) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    bit * bitrow;
    
    bitrow = pbm_allocrow_packed(cols);
    if (setjmp(jmpbuf) != 0) {
        pbm_freerow(bitrow);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int col;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        pbm_readpbmrow_packed(fileP, bitrow, cols, format);

        for (col = 0; col < cols; ++col) {
            grayrow[col] =
                ((bitrow[col/8] >> (7 - col%8)) & 0x1) == PBM_WHITE ?
                maxval : 0
                ;
        }
        pm_setjmpbuf(origJmpbufP);
    }
    pbm_freerow(bitrow);
}



void
pgm_readpgmrow(FILE * const fileP,
               gray * const grayrow, 
               int    const cols,
               gray   const maxval,
               int    const format) {

    switch (format) {
    case PGM_FORMAT: {
        unsigned int col;
        for (col = 0; col < cols; ++col) {
            grayrow[col] = pm_getuint(fileP);
            if (grayrow[col] > maxval)
                pm_error("value out of bounds (%u > %u)",
                         grayrow[col], maxval);
        }
    }
    break;
        
    case RPGM_FORMAT:
        readRpgmRow(fileP, grayrow, cols, maxval, format);
        break;
    
    case PBM_FORMAT:
    case RPBM_FORMAT:
        readPbmRow(fileP, grayrow, cols, maxval, format);
        break;
        
    default:
        pm_error("can't happen");
    }
}



gray **
pgm_readpgm(FILE * const fileP,
            int *  const colsP,
            int *  const rowsP, 
            gray * const maxvalP) {

    gray ** grays;
    int rows, cols;
    gray maxval;
    int format;
    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;

    pgm_readpgminit(fileP, &cols, &rows, &maxval, &format);
    
    grays = pgm_allocarray(cols, rows);

    if (setjmp(jmpbuf) != 0) {
        pgm_freearray(grays, rows);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int row;
    
        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        for (row = 0; row < rows; ++row)
            pgm_readpgmrow(fileP, grays[row], cols, maxval, format);

        pm_setjmpbuf(origJmpbufP);
    }
    *colsP = cols;
    *rowsP = rows;
    *maxvalP = maxval;
    return grays;
}



void
pgm_check(FILE *               const file, 
          enum pm_check_type   const checkType, 
          int                  const format, 
          int                  const cols, 
          int                  const rows, 
          gray                 const maxval,
          enum pm_check_code * const retvalP) {

    if (rows < 0)
        pm_error("Invalid number of rows passed to pgm_check(): %d", rows);
    if (cols < 0)
        pm_error("Invalid number of columns passed to pgm_check(): %d", cols);
    
    if (checkType != PM_CHECK_BASIC) {
        if (retvalP)
            *retvalP = PM_CHECK_UNKNOWN_TYPE;
    } else if (PGM_FORMAT_TYPE(format) == PBM_TYPE) {
        pbm_check(file, checkType, format, cols, rows, retvalP);
    } else if (format != RPGM_FORMAT) {
        if (retvalP)
            *retvalP = PM_CHECK_UNCHECKABLE;
    } else {        
        pm_filepos const bytesPerRow    = cols * (maxval > 255 ? 2 : 1);
        pm_filepos const needRasterSize = rows * bytesPerRow;
        
        pm_check(file, checkType, needRasterSize, retvalP);
    }
}
