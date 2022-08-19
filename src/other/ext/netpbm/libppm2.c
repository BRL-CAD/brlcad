/* libppm2.c - ppm utility library part 2
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
#include "ppm.h"

void
ppm_writeppminit(FILE*  const fileP, 
                 int    const cols, 
                 int    const rows, 
                 pixval const maxval, 
                 int    const forceplain) {

    bool const plainFormat = forceplain || pm_plain_output;

    if (maxval > PPM_OVERALLMAXVAL && !plainFormat) 
        pm_error("too-large maxval passed to ppm_writeppminit(): %d."
                 "Maximum allowed by the PPM format is %d.",
                 maxval, PPM_OVERALLMAXVAL);

    fprintf(fileP, "%c%c\n%d %d\n%d\n", 
            PPM_MAGIC1, 
            plainFormat || maxval >= 1<<16 ? PPM_MAGIC2 : RPPM_MAGIC2, 
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
format1bpsRow(const pixel *   const pixelrow,
              unsigned int    const cols,
              unsigned char * const rowBuffer) {

    /* single byte samples. */

    unsigned int col;
    unsigned int bufferCursor;

    bufferCursor = 0;

    for (col = 0; col < cols; ++col) {
        rowBuffer[bufferCursor++] = PPM_GETR(pixelrow[col]);
        rowBuffer[bufferCursor++] = PPM_GETG(pixelrow[col]);
        rowBuffer[bufferCursor++] = PPM_GETB(pixelrow[col]);
    }
}




static void
format2bpsRow(const pixel *   const pixelrow,
              unsigned int    const cols,
              unsigned char * const rowBuffer) {
    
    /* two byte samples. */

    unsigned int col;
    unsigned int bufferCursor;

    bufferCursor = 0;

    for (col = 0; col < cols; ++col) {
        pixval const r = PPM_GETR(pixelrow[col]);
        pixval const g = PPM_GETG(pixelrow[col]);
        pixval const b = PPM_GETB(pixelrow[col]);
        
        rowBuffer[bufferCursor++] = r >> 8;
        rowBuffer[bufferCursor++] = (unsigned char)r;
        rowBuffer[bufferCursor++] = g >> 8;
        rowBuffer[bufferCursor++] = (unsigned char)g;
        rowBuffer[bufferCursor++] = b >> 8;
        rowBuffer[bufferCursor++] = (unsigned char)b;
    }
}



static void
ppm_writeppmrowraw(FILE *        const fileP,
                   const pixel * const pixelrow,
                   unsigned int  const cols,
                   pixval        const maxval ) {

    unsigned int const bytesPerSample = maxval < 256 ? 1 : 2;
    unsigned int const bytesPerRow    = cols * 3 * bytesPerSample;

    unsigned char * rowBuffer;
    ssize_t rc;

    MALLOCARRAY(rowBuffer, bytesPerRow);

    if (rowBuffer == NULL)
        pm_error("Unable to allocate memory for row buffer "
                 "for %u columns", cols);

    if (maxval < 256)
        format1bpsRow(pixelrow, cols, rowBuffer);
    else
        format2bpsRow(pixelrow, cols, rowBuffer);

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
ppm_writeppmrowplain(FILE *        const fileP,
                     const pixel * const pixelrow,
                     unsigned int  const cols,
                     pixval        const maxval) {

    unsigned int col;
    unsigned int charcount;

    charcount = 0;

    for (col = 0; col < cols; ++col) {
        if (charcount >= 65) {
            putc('\n', fileP);
            charcount = 0;
        } else if (charcount > 0) {
            putc(' ', fileP);
            putc(' ', fileP);
            charcount += 2;
        }
        putus(PPM_GETR(pixelrow[col]), fileP);
        putc(' ', fileP);
        putus(PPM_GETG(pixelrow[col]), fileP);
        putc(' ', fileP);
        putus(PPM_GETB(pixelrow[col]), fileP);
        charcount += 11;
    }
    if (charcount > 0)
        putc('\n', fileP);
}



void
ppm_writeppmrow(FILE *        const fileP, 
                const pixel * const pixelrow, 
                int           const cols, 
                pixval        const maxval, 
                int           const forceplain) {

    if (forceplain || pm_plain_output || maxval >= 1<<16) 
        ppm_writeppmrowplain(fileP, pixelrow, cols, maxval);
    else 
        ppm_writeppmrowraw(fileP, pixelrow, cols, maxval);
}



void
ppm_writeppm(FILE *  const file, 
             pixel** const pixels, 
             int     const cols, 
             int     const rows, 
             pixval  const maxval, 
             int     const forceplain)  {
    int row;
    
    ppm_writeppminit(file, cols, rows, maxval, forceplain);
    
    for (row = 0; row < rows; ++row)
        ppm_writeppmrow(file, pixels[row], cols, maxval, forceplain);
}
