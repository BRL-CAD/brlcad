/* libpnm2.c - pnm utility library part 2
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

#include "pm_c_util.h"

#include "pnm.h"

#include "ppm.h"

#include "pgm.h"

#include "pbm.h"



void
pnm_writepnminit(FILE * const fileP, 
                 int    const cols, 
                 int    const rows, 
                 xelval const maxval, 
                 int    const format, 
                 int    const forceplain) {

    bool const plainFormat = forceplain || pm_plain_output;

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        ppm_writeppminit(fileP, cols, rows, (pixval) maxval, plainFormat);
        break;

    case PGM_TYPE:
        pgm_writepgminit(fileP, cols, rows, (gray) maxval, plainFormat);
        break;

    case PBM_TYPE:
        pbm_writepbminit(fileP, cols, rows, plainFormat);
    break;

    default:
        pm_error("invalid format argument received by pnm_writepnminit(): %d"
                 "PNM_FORMAT_TYPE(format) must be %d, %d, or %d", 
                 format, PBM_TYPE, PGM_TYPE, PPM_TYPE);
    }
}



static void
writepgmrow(FILE *       const fileP, 
            const xel *  const xelrow, 
            unsigned int const cols, 
            xelval       const maxval, 
            int          const format, 
            bool         const plainFormat) {
    
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
        
        for (col = 0; col < cols; ++col)
            grayrow[col] = PNM_GET1(xelrow[col]);
    
        pgm_writepgmrow(fileP, grayrow, cols, (gray) maxval, plainFormat);

        pm_setjmpbuf(origJmpbufP);
    }
    pgm_freerow(grayrow);
}



static void
writepbmrow(FILE *       const fileP,
            const xel *  const xelrow,
            unsigned int const cols,
            bool         const plainFormat) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    bit * bitrow;

    bitrow = pbm_allocrow(cols);
    
    if (setjmp(jmpbuf) != 0) {
        pbm_freerow(bitrow);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int col;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        for (col = 0; col < cols; ++col)
            bitrow[col] = PNM_GET1(xelrow[col]) == 0 ? PBM_BLACK : PBM_WHITE;
    
        pbm_writepbmrow(fileP, bitrow, cols, plainFormat);

        pm_setjmpbuf(origJmpbufP);
    }
    pbm_freerow(bitrow);
}    



void
pnm_writepnmrow(FILE *      const fileP, 
                const xel * const xelrow, 
                int         const cols, 
                xelval      const maxval, 
                int         const format, 
                int         const forceplain) {

    bool const plainFormat = forceplain || pm_plain_output;
    
    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        ppm_writeppmrow(fileP, (pixel*) xelrow, cols, (pixval) maxval, 
                        plainFormat);
        break;

    case PGM_TYPE:
        writepgmrow(fileP, xelrow, cols, maxval, format, plainFormat);
        break;

    case PBM_TYPE:
        writepbmrow(fileP, xelrow, cols, plainFormat);
        break;
    
    default:
        pm_error("invalid format argument received by pnm_writepnmrow(): %d"
                 "PNM_FORMAT_TYPE(format) must be %d, %d, or %d", 
                 format, PBM_TYPE, PGM_TYPE, PPM_TYPE);
    }
}



void
pnm_writepnm(FILE * const fileP,
             xel ** const xels,
             int    const cols,
             int    const rows,
             xelval const maxval,
             int    const format,
             int    const forceplain) {

    unsigned int row;

    pnm_writepnminit(fileP, cols, rows, maxval, format, forceplain);
    
    for (row = 0; row < rows; ++row)
        pnm_writepnmrow(fileP, xels[row], cols, maxval, format, forceplain);
}
