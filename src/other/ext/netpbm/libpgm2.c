/* libpgm2.c - pgm utility library part 2
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

#include "pm_c_util.h"
#include "mallocvar.h"
#include "pgm.h"



void
pgm_writepgminit(FILE * const fileP, 
                 int    const cols, 
                 int    const rows, 
                 gray   const maxval, 
                 int    const forceplain) {

    bool const plainFormat = forceplain || pm_plain_output;

    if (maxval > PGM_OVERALLMAXVAL && !plainFormat) 
        pm_error("too-large maxval passed to ppm_writepgminit(): %d.\n"
                 "Maximum allowed by the PGM format is %d.",
                 maxval, PGM_OVERALLMAXVAL);

    fprintf(fileP, "%c%c\n%d %d\n%d\n", 
            PGM_MAGIC1, 
            plainFormat || maxval >= 1<<16 ? PGM_MAGIC2 : RPGM_MAGIC2, 
            cols, rows, maxval );
}



static void
putus(unsigned short const n, 
      FILE *         const fileP) {

    if (n >= 10)
        putus(n / 10, fileP);
    putc('0' + n % 10, fileP);
}



static void
format1bpsRow(const gray *    const grayrow,
              unsigned int    const cols,
              unsigned char * const rowBuffer) {

    /* single byte samples. */

    unsigned int col;
    unsigned int bufferCursor;

    bufferCursor = 0;

    for (col = 0; col < cols; ++col)
        rowBuffer[bufferCursor++] = grayrow[col];
}




static void
format2bpsRow(const gray    * const grayrow,
              unsigned int    const cols,
              unsigned char * const rowBuffer) {

    /* two byte samples. */

    unsigned int col;
    unsigned int bufferCursor;

    bufferCursor = 0;

    for (col = 0; col < cols; ++col) {
        gray const val = grayrow[col];
        
        rowBuffer[bufferCursor++] = val >> 8;
        rowBuffer[bufferCursor++] = (unsigned char) val;
    }
}



static void
writepgmrowraw(FILE *       const fileP,
               const gray * const grayrow,
               unsigned int const cols,
               gray         const maxval) {

    unsigned int const bytesPerSample = maxval < 256 ? 1 : 2;
    unsigned int const bytesPerRow    = cols * bytesPerSample;

    unsigned char * rowBuffer;
    ssize_t rc;

    MALLOCARRAY(rowBuffer, bytesPerRow);

    if (rowBuffer == NULL)
        pm_error("Unable to allocate memory for row buffer "
                 "for %u columns", cols);

    if (maxval < 256)
        format1bpsRow(grayrow, cols, rowBuffer);
    else
        format2bpsRow(grayrow, cols, rowBuffer);

    rc = fwrite(rowBuffer, 1, bytesPerRow, fileP);

    if (rc < 0)
        pm_error("Error writing row.  fwrite() errno=%d (%s)",
                 errno, strerror(errno));
    else {
        size_t const bytesWritten = rc;
        if (bytesWritten != bytesPerRow)
            pm_error("Error writing row.  Short write of %u bytes "
                     "instead of %u", (unsigned)bytesWritten, bytesPerRow);
    }
    free(rowBuffer);
}



static void
writepgmrowplain(FILE *       const fileP,
                 const gray * const grayrow, 
                 unsigned int const cols, 
                 gray         const maxval) {

    int col, charcount;

    charcount = 0;
    for (col = 0; col < cols; ++col) {
        if (charcount >= 65) {
            putc('\n', fileP);
            charcount = 0;
        } else if (charcount > 0) {
            putc(' ', fileP);
            ++charcount;
        }
#ifdef DEBUG
        if (grayrow[col] > maxval)
            pm_error("value out of bounds (%u > %u)", grayrow[col], maxval);
#endif /*DEBUG*/
        putus((unsigned short)grayrow[col], fileP);
        charcount += 3;
    }
    if (charcount > 0)
        putc('\n', fileP);
}



void
pgm_writepgmrow(FILE *       const fileP, 
                const gray * const grayrow, 
                int          const cols, 
                gray         const maxval, 
                int          const forceplain) {

    if (forceplain || pm_plain_output || maxval >= 1<<16)
        writepgmrowplain(fileP, grayrow, cols, maxval);
    else
        writepgmrowraw(fileP, grayrow, cols, maxval);
}



void
pgm_writepgm(FILE *  const fileP,
             gray ** const grays,
             int     const cols,
             int     const rows,
             gray    const maxval,
             int     const forceplain) {

    unsigned int row;

    pgm_writepgminit(fileP, cols, rows, maxval, forceplain);

    for (row = 0; row < rows; ++row)
        pgm_writepgmrow(fileP, grays[row], cols, maxval, forceplain);
}
