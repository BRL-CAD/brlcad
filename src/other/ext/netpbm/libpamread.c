/*=============================================================================
                                  libpamread.c
===============================================================================
   These are the library functions, which belong in the libnetpbm library,
   that deal with reading the PAM (Portable Arbitrary Format) image format
   raster (not the header).

   This file was originally written by Bryan Henderson and is contributed
   to the public domain by him and subsequent authors.
=============================================================================*/

/* See pmfileio.c for the complicated explanation of this 32/64 bit file
   offset stuff.
*/
#define _FILE_OFFSET_BITS 64
#define _LARGE_FILES  

#include <string.h>
#include <limits.h>
#include <assert.h>

#include "pm_config.h"


#include "fileio.h"
#include "pam.h"


static void
readPbmRow(const struct pam * const pamP,
           tuple *            const tuplerow) {

    if (pamP->depth != 1)
        pm_error("Invalid pam structure passed to pnm_readpamrow().  "
                 "It says PBM format, but 'depth' member is not 1.");
    else {
        jmp_buf jmpbuf;
        jmp_buf * origJmpbufP;
        unsigned char * bitrow;

        bitrow = (unsigned char *) pbm_allocrow(pbm_packed_bytes(pamP->width));

        if (setjmp(jmpbuf) != 0) {
            pbm_freerow(bitrow);
            pm_setjmpbuf(origJmpbufP);
            pm_longjmp();
        } else {
            pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

            pbm_readpbmrow_packed(pamP->file, bitrow, pamP->width,
                                  pamP->format);
    
            if (tuplerow) {
                unsigned int col;
                for (col = 0; col < pamP->width; ++col) {
                    tuplerow[col][0] = 
                        ( ((bitrow[col/8] >> (7-col%8)) & 1 ) == PBM_BLACK)
                        ? PAM_PBM_BLACK : PAM_PBM_WHITE
                        ;
                }
            }
            pm_setjmpbuf(origJmpbufP);
        }
        pbm_freerow(bitrow);
    }
}



static void
readPlainNonPbmRow(const struct pam * const pamP,
                   tuple *            const tuplerow) {

    int col;

    for (col = 0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            if (tuplerow) {
                tuplerow[col][plane] = pm_getuint(pamP->file);

                if (tuplerow[col][plane] > pamP->maxval)
                    pm_error("Plane %u sample value %lu exceeds the "
                             "image maxval of %lu",
                             plane, tuplerow[col][plane], pamP->maxval);
            } else
                pm_getuint(pamP->file);  /* read data and discard */
    }
}



/* Though it is possible to simplify the bytesToSampleN() and
   parsexNBpsRow() functions into a single routine that handles all
   sample widths, we value efficiency higher here.  Earlier versions
   of Netpbm (before 10.25) did that, with a loop, and performance
   suffered visibly.
*/

static __inline__ sample
bytes2ToSample(unsigned char buff[2]) {

    return (buff[0] << 8) + buff[1];
}



static __inline__ sample
bytes3ToSample(unsigned char buff[3]) {
    return (buff[0] << 16) + (buff[1] << 8) + buff[2];
}



static __inline__ sample
bytes4ToSample(unsigned char buff[4]) {

    return (buff[0] << 24) + (buff[1] << 16) + (buff[2] << 8) + buff[3];
}



static void
parse1BpsRow(const struct pam *    const pamP,
             tuple *               const tuplerow,
             const unsigned char * const inbuf) {

    int col;
    unsigned int bufferCursor;

    bufferCursor = 0;  /* initial value */

    for (col = 0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            tuplerow[col][plane]= inbuf[bufferCursor++];
    }
}



static void
parse2BpsRow(const struct pam *    const pamP,
             tuple *               const tuplerow,
             const unsigned char * const inbuf) {

    unsigned char (* const ib)[2] = (unsigned char (*)[2]) inbuf;

    int col;
    unsigned int bufferCursor;

    bufferCursor = 0;  /* initial value */

    for (col=0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            tuplerow[col][plane] = bytes2ToSample(ib[bufferCursor++]);
    }
}



static void
parse3BpsRow(const struct pam *    const pamP,
             tuple *               const tuplerow,
             const unsigned char * const inbuf) {

    unsigned char (* const ib)[3] = (unsigned char (*)[3]) inbuf;

    int col;
    unsigned int bufferCursor;

    bufferCursor = 0;  /* initial value */

    for (col=0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            tuplerow[col][plane] = bytes3ToSample(ib[bufferCursor++]);
    }
}



static void
parse4BpsRow(const struct pam *    const pamP,
             tuple *               const tuplerow,
             const unsigned char * const inbuf) {

    unsigned char (* const ib)[4] = (unsigned char (*)[4]) inbuf;

    int col;
    unsigned int bufferCursor;

    bufferCursor = 0;  /* initial value */

    for (col=0; col < pamP->width; ++col) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane)
            tuplerow[col][plane] = bytes4ToSample(ib[bufferCursor++]);
    }
}



static void
validatePamRow(const struct pam * const pamP,
               tuple *            const tuplerow,
               const char **      const errorP) {
/*----------------------------------------------------------------------------
  Check for sample values above maxval in input.  

  Note: a program that wants to deal with invalid sample values itself can
  simply make sure it sets pamP->maxval sufficiently high, so this validation
  never fails.
-----------------------------------------------------------------------------*/
    /* To save time, skip the test for if the maxval is a saturated value
       (255, 65535) or format is PBM.

       This is an expensive test, but is skipped in most cases: in practice
       maxvals other than 255 or 65535 are uncommon.  Thus we do this in a
       separate pass through the row rather than while reading in the row.
    */

    if (pamP->maxval == (((sample) 0x1) << pamP->bytes_per_sample*8) - 1 ||
        PAM_FORMAT_TYPE(pamP->format) == PBM_FORMAT) {
        /* There's no way a sample can be invalid, so we don't need to
           look at the samples individually.
        */
        *errorP = NULL;
    } else {
        unsigned int col;
        for (col = 0; col < pamP->width; ++col) {
            unsigned int plane;
            for (plane = 0; plane < pamP->depth; ++plane) {
                if (tuplerow[col][plane] > pamP->maxval) {
                    pm_error("Plane %u sample value %lu exceeds the image maxval of %lu",
                                plane, tuplerow[col][plane], pamP->maxval);
                    return;
                }
            }
        }
        *errorP = NULL;
    }
}



static void
readRawNonPbmRow(const struct pam * const pamP,
                 tuple *            const tuplerow) {

    unsigned int const rowImageSize = 
        pamP->width * pamP->bytes_per_sample * pamP->depth;

    unsigned char * inbuf;
    size_t bytesRead;
    const char * error = NULL;

    inbuf = pnm_allocrowimage(pamP);
    
    bytesRead = fread(inbuf, 1, rowImageSize, pamP->file);

    if (bytesRead != rowImageSize) {
        if (feof(pamP->file))
            pm_error("End of file encountered "
                        "when trying to read a row from input file.");
        else 
            pm_error("Error reading a row from input file.  "
                        "fread() fails with errno=%d (%s)",
                        errno, strerror(errno));
    } else {
        error = NULL;  /* initial assumption */
        if (tuplerow) {
            switch (pamP->bytes_per_sample) {
            case 1: parse1BpsRow(pamP, tuplerow, inbuf); break;
            case 2: parse2BpsRow(pamP, tuplerow, inbuf); break;
            case 3: parse3BpsRow(pamP, tuplerow, inbuf); break;
            case 4: parse4BpsRow(pamP, tuplerow, inbuf); break;
            default:
                pm_error("invalid bytes per sample passed to "
                            "pnm_formatpamrow(): %u", pamP->bytes_per_sample);
            }
            if (error == NULL)
                validatePamRow(pamP, tuplerow, &error);
        }
    }
    pnm_freerowimage(inbuf);

    if (error) {
        pm_error("%s", error);
        free((void *)error);
        pm_longjmp();
    }
}



void 
pnm_readpamrow(const struct pam * const pamP, 
               tuple *            const tuplerow) {
/*----------------------------------------------------------------------------
   Read a row from the Netpbm image file into tuplerow[], at the
   current file position.  If 'tuplerow' is NULL, advance the file
   pointer to the next row, but don't return the contents of the
   current one.
   
   We assume the file is positioned to the beginning of a row of the
   image's raster.
-----------------------------------------------------------------------------*/
    /* For speed, we don't check any of the inputs for consistency 
       here (unless it's necessary to avoid crashing).  Any consistency
       checking should have been done by a prior call to 
       pnm_readpaminit().
    */  

    /* Need a special case for raw PBM because it has multiple tuples (8)
       packed into one byte.
    */

    switch (pamP->format) {
    case PAM_FORMAT:
    case RPPM_FORMAT:
    case RPGM_FORMAT:
        readRawNonPbmRow(pamP, tuplerow);
        break;
    case PPM_FORMAT:
    case PGM_FORMAT:
        readPlainNonPbmRow(pamP, tuplerow);
        break;
    case RPBM_FORMAT:
    case PBM_FORMAT:
        readPbmRow(pamP, tuplerow);
        break;
    default:
        pm_error("Invalid 'format' member in PAM structure: %u", pamP->format);
    }
}



tuple ** 
pnm_readpam(FILE *       const fileP,
            struct pam * const pamP, 
            int          const size) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    tuple ** tuplearray;

    pnm_readpaminit(fileP, pamP, size);
    
    tuplearray = pnm_allocpamarray(pamP);
    
    if (setjmp(jmpbuf) != 0) {
        pnm_freepamarray(tuplearray, pamP);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int row;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);
            
        for (row = 0; row < pamP->height; ++row) 
            pnm_readpamrow(pamP, tuplearray[row]);

        pm_setjmpbuf(origJmpbufP);
    }
    return tuplearray;
}
