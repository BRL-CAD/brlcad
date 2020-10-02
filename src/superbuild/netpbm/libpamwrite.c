/*----------------------------------------------------------------------------
                                  libpamwrite.c
------------------------------------------------------------------------------
   These are the library functions, which belong in the libnetpbm library,
   that deal with writing the PAM (Portable Arbitrary Format) image format
   raster (not the header).
-----------------------------------------------------------------------------*/

/* See libpm.c for the complicated explanation of this 32/64 bit file
   offset stuff.
*/
#define _FILE_OFFSET_BITS 64
#define _LARGE_FILES  

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include <math.h>

#include "pam.h"


static __inline__ unsigned int
samplesPerPlainLine(sample       const maxval, 
                    unsigned int const depth, 
                    unsigned int const lineLength) {
/*----------------------------------------------------------------------------
   Return the minimum number of samples that should go in a line
   'lineLength' characters long in a plain format non-PBM PNM image
   with depth 'depth' and maxval 'maxval'.

   Note that this number is just for aesthetics; the Netpbm formats allow
   any number of samples per line.
-----------------------------------------------------------------------------*/
    unsigned int const digitsForMaxval = (unsigned int)
        (log(maxval + 0.1 ) / log(10.0));
        /* Number of digits maxval has in decimal */
        /* +0.1 is an adjustment to overcome precision problems */
    unsigned int const fit = lineLength / (digitsForMaxval + 1);
        /* Number of maxval-sized samples that fit in a line */
    unsigned int const retval = (fit > depth) ? (fit - (fit % depth)) : fit;
        /* 'fit', rounded down to a multiple of depth, if possible */

    return retval;
}



static void
writePamPlainPbmRow(const struct pam *  const pamP,
                    const tuple *       const tuplerow) {

    int col;
    unsigned int const samplesPerLine = 70;

    for (col = 0; col < pamP->width; ++col)
        fprintf(pamP->file,  
                ((col+1) % samplesPerLine == 0 || col == pamP->width-1)
                    ? "%1u\n" : "%1u",
                tuplerow[col][0] == PAM_PBM_BLACK ? PBM_BLACK : PBM_WHITE);
}



static void
writePamPlainRow(const struct pam *  const pamP,
                    const tuple *       const tuplerow) {

    int const samplesPerLine = 
        samplesPerPlainLine(pamP->maxval, pamP->depth, 79);

    int col;
    unsigned int samplesInCurrentLine;
        /* number of samples written from start of line  */
    
    samplesInCurrentLine = 0;

    for (col = 0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane){
            fprintf(pamP->file, "%lu ",tuplerow[col][plane]);

            ++samplesInCurrentLine;

            if (samplesInCurrentLine >= samplesPerLine) {
                fprintf(pamP->file, "\n");
                samplesInCurrentLine = 0;
            }            
        }
    }
    fprintf(pamP->file, "\n");
}



static void
formatPbmRow(const struct pam * const pamP,
             const tuple *      const tuplerow,
             unsigned char *    const outbuf,
             unsigned int *     const rowSizeP) {

    unsigned char accum;
    int col;
    
    accum = 0;  /* initial value */
    
    for (col=0; col < pamP->width; ++col) {
        accum |= 
            (tuplerow[col][0] == PAM_PBM_BLACK ? PBM_BLACK : PBM_WHITE)
                << (7-col%8);
        if (col%8 == 7) {
                outbuf[col/8] = accum;
                accum = 0;
        }
    }
    if (pamP->width % 8 != 0) {
        unsigned int const lastByteIndex = pamP->width/8;
        outbuf[lastByteIndex] = accum;
        *rowSizeP = lastByteIndex + 1;
    } else
        *rowSizeP = pamP->width/8;
}



/* Though it is possible to simplify the sampleToBytesN() and
   formatNBpsRow() functions into a single routine that handles all
   sample widths, we value efficiency higher here.  Earlier versions
   of Netpbm (before 10.25) did that, with a loop, and performance
   suffered visibly.
*/

static __inline__ void
sampleToBytes2(unsigned char       buf[2], 
               sample        const sampleval) {

    buf[0] = (sampleval >> 8) & 0xff;
    buf[1] = (sampleval >> 0) & 0xff;
}



static __inline__ void
sampleToBytes3(unsigned char       buf[3], 
               sample        const sampleval) {

    buf[0] = (sampleval >> 16) & 0xff;
    buf[1] = (sampleval >>  8) & 0xff;
    buf[2] = (sampleval >>  0) & 0xff;
}



static __inline__ void
sampleToBytes4(unsigned char       buf[4], 
               sample        const sampleval) {

    buf[0] = (sampleval >> 24 ) & 0xff;
    buf[1] = (sampleval >> 16 ) & 0xff;
    buf[2] = (sampleval >>  8 ) & 0xff;
    buf[3] = (sampleval >>  0 ) & 0xff;
}



static __inline__ void
format1BpsRow(const struct pam * const pamP,
              const tuple *      const tuplerow,
              unsigned char *    const outbuf,
              unsigned int *     const rowSizeP) {
/*----------------------------------------------------------------------------
   Create the image of a row in the raster of a raw format Netpbm
   image that has one byte per sample (ergo not PBM).

   Put the image at *outbuf; put the number of bytes of it at *rowSizeP.
-----------------------------------------------------------------------------*/
    int col;
    unsigned int bufferCursor;

    bufferCursor = 0;  /* initial value */
    
    for (col = 0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane=0; plane < pamP->depth; ++plane)
            outbuf[bufferCursor++] = (unsigned char)tuplerow[col][plane];
    }
    *rowSizeP = pamP->width * 1 * pamP->depth;
}



static __inline__ void
format2BpsRow(const struct pam * const pamP,
              const tuple *      const tuplerow,
              unsigned char *    const outbuf,
              unsigned int *     const rowSizeP) {
/*----------------------------------------------------------------------------
  Analogous to format1BpsRow().
-----------------------------------------------------------------------------*/
    unsigned char (* const ob)[2] = (unsigned char (*)[2]) outbuf;

    int col;
    unsigned int bufferCursor;

    bufferCursor = 0;  /* initial value */
    
    for (col=0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            sampleToBytes2(ob[bufferCursor++], tuplerow[col][plane]);
    }

    *rowSizeP = pamP->width * 2 * pamP->depth;
}



static __inline__ void
format3BpsRow(const struct pam * const pamP,
              const tuple *      const tuplerow,
              unsigned char *    const outbuf,
              unsigned int *     const rowSizeP) {
/*----------------------------------------------------------------------------
  Analogous to format1BpsRow().
-----------------------------------------------------------------------------*/
    unsigned char (* const ob)[3] = (unsigned char (*)[3]) outbuf;

    int col;
    unsigned int bufferCursor;

    bufferCursor = 0;  /* initial value */
    
    for (col=0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            sampleToBytes3(ob[bufferCursor++], tuplerow[col][plane]);
    }

    *rowSizeP = pamP->width * 3 * pamP->depth;
}



static __inline__ void
format4BpsRow(const struct pam * const pamP,
              const tuple *      const tuplerow,
              unsigned char *    const outbuf,
              unsigned int *     const rowSizeP) {
/*----------------------------------------------------------------------------
  Analogous to format1BpsRow().
-----------------------------------------------------------------------------*/
    unsigned char (* const ob)[4] = (unsigned char (*)[4]) outbuf;

    int col;
    unsigned int bufferCursor;

    bufferCursor = 0;  /* initial value */
    
    for (col=0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            sampleToBytes4(ob[bufferCursor++], tuplerow[col][plane]);
    }

    *rowSizeP = pamP->width * 4 * pamP->depth;
}



void
pnm_formatpamrow(const struct pam * const pamP,
                 const tuple *      const tuplerow,
                 unsigned char *    const outbuf,
                 unsigned int *     const rowSizeP) {
/*----------------------------------------------------------------------------
   Create the image of a row in the raster of a raw (not plain) format
   Netpbm image, as described by *pamP and tuplerow[].  Put the image
   at *outbuf.

   'outbuf' must be the address of space allocated with pnm_allocrowimage().
   
   We return as *rowSizeP the number of bytes in the row image.
-----------------------------------------------------------------------------*/
    if (PAM_FORMAT_TYPE(pamP->format) == PBM_TYPE)
        formatPbmRow(pamP, tuplerow, outbuf, rowSizeP);
    else {
        switch(pamP->bytes_per_sample){
        case 1: format1BpsRow(pamP, tuplerow, outbuf, rowSizeP); break;
        case 2: format2BpsRow(pamP, tuplerow, outbuf, rowSizeP); break;
        case 3: format3BpsRow(pamP, tuplerow, outbuf, rowSizeP); break;
        case 4: format4BpsRow(pamP, tuplerow, outbuf, rowSizeP); break;
        default:
            pm_error("invalid bytes per sample passed to "
                     "pnm_formatpamrow(): %u",  pamP->bytes_per_sample);
        }
    }
}



static void
writePamRawRow(const struct pam * const pamP,
               const tuple *      const tuplerow,
               unsigned int       const count) {
/*----------------------------------------------------------------------------
   Write mutiple ('count') copies of the same row ('tuplerow') to the file,
   in raw (not plain) format.
-----------------------------------------------------------------------------*/
    unsigned int rowImageSize;

    unsigned char * outbuf;  /* malloc'ed */
    unsigned int i;

    outbuf = pnm_allocrowimage(pamP);

    pnm_formatpamrow(pamP, tuplerow, outbuf, &rowImageSize);

    for (i = 0; i < count; ++i) {
        size_t bytesWritten;

        bytesWritten = fwrite(outbuf, 1, rowImageSize, pamP->file);
        if (bytesWritten != rowImageSize)
            pm_error("fwrite() failed to write an image row to the file.  "
                     "errno=%d (%s)", errno, strerror(errno));
    }
    pnm_freerowimage(outbuf);
}



void 
pnm_writepamrow(const struct pam * const pamP, 
                const tuple *      const tuplerow) {

    /* For speed, we don't check any of the inputs for consistency 
       here (unless it's necessary to avoid crashing).  Any consistency
       checking should have been done by a prior call to 
       pnm_writepaminit().
    */
    
    if (pm_plain_output || pamP->plainformat) {
        switch (PAM_FORMAT_TYPE(pamP->format)) {
        case PBM_TYPE:
            writePamPlainPbmRow(pamP, tuplerow);
            break;
        case PGM_TYPE:
        case PPM_TYPE:
            writePamPlainRow(pamP, tuplerow);
            break;
        case PAM_TYPE:
            /* pm_plain_output is impossible here due to assumption stated
               above about pnm_writepaminit() having checked it.  The
               pamP->plainformat is meaningless for PAM.
            */
            writePamRawRow(pamP, tuplerow, 1);
            break;
        default:
            pm_error("Invalid 'format' value %u in pam structure", 
                     pamP->format);
        }
    } else
        writePamRawRow(pamP, tuplerow, 1);
}



void
pnm_writepamrowmult(const struct pam * const pamP, 
                    const tuple *      const tuplerow,
                    unsigned int       const count) {
/*----------------------------------------------------------------------------
   Write mutiple ('count') copies of the same row ('tuplerow') to the file.
-----------------------------------------------------------------------------*/
   if (pm_plain_output || pamP->plainformat) {
       unsigned int i;
       for (i = 0; i < count; ++i)
           pnm_writepamrow(pamP, tuplerow);
   } else
       /* Simple common case - use fastpath */
       writePamRawRow(pamP, tuplerow, count);
}



void 
pnm_writepam(struct pam * const pamP, 
             tuple **     const tuplearray) {

    int row;

    pnm_writepaminit(pamP);
    
    for (row = 0; row < pamP->height; ++row) 
        pnm_writepamrow(pamP, tuplearray[row]);
}
