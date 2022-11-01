/* libpbm2.c - pbm utility library part 2
**
** Copyright (C) 1988 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <assert.h>
#include <limits.h>

#include "pbm.h"
#include "libpbm.h"
#include "fileio.h"
#include "pam.h"

static bit 
getbit (FILE * const file) {
    char ch;

    do {
        ch = pm_getc( file );
    } while ( ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' );

    if ( ch != '0' && ch != '1' )
        pm_error( "junk in file where bits should be" );
    
    return ( ch == '1' ) ? 1 : 0;
}



void
pbm_readpbminitrest( FILE * const file,
                     int  * const colsP,
                     int  * const rowsP ) {
    /* Read size. */
    *colsP = (int)pm_getuint( file );
    *rowsP = (int)pm_getuint( file );

    /* *colsP and *rowsP really should be unsigned int, but they come
       from the time before unsigned ints (or at least from a person
       trained in that tradition), so they are int.  We could simply
       consider negative numbers to mean values > INT_MAX/2 and much
       code would just automatically work.  But some code would fail
       miserably.  So we consider values that won't fit in an int to
       be unprocessable.
    */
    if (*colsP < 0)
        pm_error("Number of columns in header is too large.");
    if (*rowsP < 0)
        pm_error("Number of columns in header is too large.");
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
pbm_readpbminit(FILE * const ifP,
                int *  const colsP,
                int *  const rowsP,
                int *  const formatP) {

    int realFormat;

    realFormat = pm_readmagicnumber(ifP);

    switch (PAM_FORMAT_TYPE(realFormat)) {
    case PBM_TYPE:
        *formatP = realFormat;
        pbm_readpbminitrest(ifP, colsP, rowsP);
        break;

    case PGM_TYPE:
        pm_error("The input file is a PGM, not a PBM.  You may want to "
                 "convert it to PBM with 'pamditherbw | pamtopnm' or "
                 "'pamthreshold | pamtopnm'");

    case PPM_TYPE:
        pm_error("The input file is a PPM, not a PBM.  You may want to "
                 "convert it to PBM with 'ppmtopgm', 'pamditherbw', and "
                 "'pamtopnm'");

    case PAM_TYPE:
        pm_error("The input file is a PAM, not a PBM.  "
                 "If it is a black and white image, you can convert it "
                 "to PBM with 'pamtopnm'");
        break;
    default:
        pm_error("bad magic number 0x%x - not a PPM, PGM, PBM, or PAM file",
                 realFormat);
    }
    validateComputableSize(*colsP, *rowsP);
}



void
pbm_readpbmrow( FILE * const file,
                bit * const bitrow,
                int const cols,
                int const format) {

    int col, bitshift;

    switch ( format )
    {
    case PBM_FORMAT:
    for ( col = 0; col < cols; ++col )
        bitrow[col] = getbit( file );
    break;

    case RPBM_FORMAT: {
        unsigned char item;
        bitshift = -1;  item = 0;  /* item's value is meaningless here */
        for ( col = 0; col < cols; ++col ) {
              if ( bitshift == -1 ) {
                    item = pm_getrawbyte( file );
                    bitshift = 7;
                }
              bitrow[col] = ( item >> bitshift ) & 1;
              --bitshift;
          }
    }
    break;

    default:
    pm_error( "can't happen" );
    }
}



void
pbm_readpbmrow_packed(FILE *          const fileP, 
                      unsigned char * const packedBits,
                      int             const cols, 
                      int             const format) {

    switch(format) {
    case PBM_FORMAT: {
        unsigned int col;
        unsigned int byteIndex;

        /* We first clear the return buffer, then set ones where needed */
        for (byteIndex = 0; byteIndex < pbm_packed_bytes(cols); ++byteIndex)
            packedBits[byteIndex] = 0x00;

        for (col = 0; col < cols; ++col) {
            unsigned char mask;
            mask = getbit(fileP) << (7 - col % 8);
            packedBits[col / 8] |= mask;
        }
    }
    break;

    case RPBM_FORMAT: {
        unsigned int bytesReadCt;
        bytesReadCt = fread(packedBits, 1, pbm_packed_bytes(cols), fileP);
             
        if (bytesReadCt < pbm_packed_bytes(cols)) {
            if (feof(fileP)) 
                if (bytesReadCt == 0) 
                    pm_error("Attempt to read a raw PBM image row, but "
                             "no more rows left in file.");
                else
                    pm_error("EOF in the middle of a raw PBM row.");
            else 
                pm_error("I/O error reading raw PBM row");
        }
    }
    break;
    
    default:
        pm_error("Internal error in pbm_readpbmrow_packed.");
    }
}



void
pbm_readpbmrow_bitoffset(FILE *          const ifP,
                         unsigned char * const packedBits, 
                         int             const cols,
                         int             const format,
                         unsigned int    const offset) {
/*----------------------------------------------------------------------------
   Read PBM packed bitrow from file 'ifP' (raster format given by
   'cols' and 'format') and shift right 'offset' bits.

   Read it into packedBits[], preserving surrounding image data.

   Logic not tested for negative offsets.

   Because we are reading in packed mode large cols and offset values are
   acceptable; dividing by 8 prevents overflows.
-----------------------------------------------------------------------------*/
    unsigned int const rsh = offset % 8;
    unsigned int const lsh = (8 - rsh) % 8;
    unsigned char * const window = &packedBits[offset/8];
        /* Area of packed row buffer into which we read the image data.
           Aligned to nearest byte boundary to the left, so the first
           few bits might contain original data, not output.
        */
    unsigned int const last = pbm_packed_bytes((unsigned int)cols + rsh) - 1;
        /* Position within window of rightmost byte after shift */

    /* The original leftmost and rightmost chars. */
    unsigned char const origHead = window[0];
    unsigned char const origEnd  = window[last];

    assert(cols > 0 && pbm_packed_bytes(cols) > 0);

    pbm_readpbmrow_packed(ifP, window, cols, format);

    if (rsh > 0) {
        /* Target slot doesn't start on byte boundary; right-shift. */
        unsigned char carryover;
        unsigned int i;
  
        carryover = (origHead >> lsh) << lsh;

        for (i = 0; i <= last; ++i) {
            unsigned char const t = window[i] << lsh;
            window[i] = carryover | window[i] >> rsh;
            carryover = t;
        }
    }
  
    if ((cols + rsh) % 8 > 0) {
        /* Adjust rightmost char */
        unsigned int  const trs = (cols + rsh) % 8;
        unsigned int  const tls = 8 - trs;
        unsigned char const rightBits =
            ((unsigned char)(origEnd << trs) >> trs);
        unsigned char const leftBits =
            ((unsigned char)(window[last] >> tls) << tls);

        window[last] =  leftBits | rightBits;
    }
}



void
pbm_cleanrowend_packed(unsigned char * const packedBits,
                       unsigned int    const cols) {
/*----------------------------------------------------------------------------
  Set fractional "don't care" bits at end of row to zero.
----------------------------------------------------------------------------*/
    unsigned int const bitsPerChar = 8;

    if (cols % bitsPerChar > 0) {
        unsigned int const last = pbm_packed_bytes(cols) - 1;

        assert(pbm_packed_bytes(cols) > 0);

        packedBits[last] >>= bitsPerChar - cols % bitsPerChar;
        packedBits[last] <<= bitsPerChar - cols % bitsPerChar;
    }
}



bit**
pbm_readpbm( FILE * const file,
             int  * const colsP,
             int  * const rowsP) {

    bit ** bits;
    int format, row;

    pbm_readpbminit( file, colsP, rowsP, &format );

    bits = pbm_allocarray( *colsP, *rowsP );

    for ( row = 0; row < *rowsP; ++row )
        pbm_readpbmrow( file, bits[row], *colsP, format );

    return bits;
}
