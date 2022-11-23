/* libpbm1.c - pbm utility library part 1
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

/* See libpm.c for the complicated explanation of this 32/64 bit file
   offset stuff.
*/
#define _FILE_OFFSET_BITS 64
#define _LARGE_FILES  

#include <stdio.h>

#include "pm_c_util.h"
#include "mallocvar.h"


#include "pbm.h"



bit *
pbm_allocrow(unsigned int const cols) {

    bit * bitrow;

    MALLOCARRAY(bitrow, cols);

    if (bitrow == NULL)
        pm_error("Unable to allocate space for a %u-column bit row", cols);

    return bitrow;
}



void
pbm_init(int *   const argcP,
         char ** const argv) {

    pm_proginit(argcP, (const char **)argv);
}



void
pbm_nextimage(FILE *file, int * const eofP) {
    pm_nextimage(file, eofP);
}



void
pbm_check(FILE *               const fileP,
          enum pm_check_type   const checkType, 
          int                  const format,
          int                  const cols,
          int                  const rows,
          enum pm_check_code * const retvalP) {

    if (rows < 0)
        pm_error("Invalid number of rows passed to pbm_check(): %d", rows);
    if (cols < 0)
        pm_error("Invalid number of columns passed to pbm_check(): %d", cols);
    
    if (checkType != PM_CHECK_BASIC) {
        if (retvalP)
            *retvalP = PM_CHECK_UNKNOWN_TYPE;
    } else if (format != RPBM_FORMAT) {
        if (retvalP)
            *retvalP = PM_CHECK_UNCHECKABLE;
    } else {        
        pm_filepos const bytesPerRow    = (cols+7)/8;
        pm_filepos const needRasterSize = rows * bytesPerRow;
        pm_check(fileP, checkType, needRasterSize, retvalP);
    }
}



static unsigned int
bitpop8(unsigned char const x) {
/*----------------------------------------------------------------------------
   Return the number of 1 bits in 'x'
-----------------------------------------------------------------------------*/
static unsigned int const p[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8 };

    return p[x];
}



static int
bitpop(const unsigned char * const packedRow,
       unsigned int          const cols,
       unsigned int          const offset) {
/*----------------------------------------------------------------------------
  Return the number of 1 bits in 'packedRow', ignoring 0 to 7 bits
  at the row start (= on the left edge), indicated by offset.
-----------------------------------------------------------------------------*/
    unsigned int const fullLength = cols + offset;

    unsigned int sum;

    if (fullLength <= 8) {
        /* All bits are in a single byte */
        sum = bitpop8((packedRow[0] << offset ) & (0xff << (8 - cols)));
    } else {
        unsigned int const colByteCnt  = pbm_packed_bytes(fullLength);
        unsigned int const fullByteCnt = fullLength/8;

        unsigned int i;

        /* First byte, whether it is full or not */
        sum = bitpop8(packedRow[0] << offset );

        /* Second byte to last full byte */
        for (i = 1; i < fullByteCnt; ++i)
            sum += bitpop8(packedRow[i]);

        /* Partial byte at the right end */
        if (colByteCnt > fullByteCnt)
            sum += bitpop8(packedRow[i] >> (8 - fullLength%8));
    }

    return sum;
}



bit
pbm_backgroundbitrow(unsigned const char * const packedBits,
                     unsigned int          const cols,
                     unsigned int          const offset) {
/*----------------------------------------------------------------------------
  PBM version of pnm_backgroundxelrow() with additional offset parameter.
  When offset == 0, produces the same return value as does
  pnm_backgroundxelrow(promoted_bitrow, cols, ...)
-----------------------------------------------------------------------------*/
    const unsigned char * const row = &packedBits[offset/8];
    unsigned int const rs = offset % 8;
    unsigned int const last = pbm_packed_bytes(cols + rs) - 1;

    unsigned int retval;

    bool const firstbit = (row[0] >> (7-rs)) & 0x01;
    bool const lastbit  = (row[last] >> (7- (cols+rs-1)%8)) & 0x01;

    if (firstbit == lastbit)
        retval = firstbit;
    else {
        if (bitpop(row, cols, rs) >= cols/2)
            retval = PBM_BLACK;
        else
            retval = PBM_WHITE;
    }
    return retval;
}
