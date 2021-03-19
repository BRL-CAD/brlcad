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
#include "libppm.h"

#include "pgm.h"
#include "libpgm.h"

#include "pbm.h"
#include "libpbm.h"

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



void
pnm_writepnmrow(FILE * const fileP, 
                xel *  const xelrow, 
                int    const cols, 
                xelval const maxval, 
                int    const format, 
                int    const forceplain) {

    bool const plainFormat = forceplain || pm_plain_output;
    
    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        ppm_writeppmrow(fileP, (pixel*) xelrow, cols, (pixval) maxval, 
                        plainFormat);
        break;

    case PGM_TYPE: {
        gray* grayrow;
        unsigned int col;

        grayrow = pgm_allocrow(cols);

        for (col = 0; col < cols; ++col)
            grayrow[col] = PNM_GET1(xelrow[col]);

        pgm_writepgmrow(fileP, grayrow, cols, (gray) maxval, plainFormat);

        pgm_freerow( grayrow );
    }
    break;

    case PBM_TYPE: {
        bit* bitrow;
        unsigned int col;

        bitrow = pbm_allocrow(cols);

        for (col = 0; col < cols; ++col)
            bitrow[col] = PNM_GET1(xelrow[col]) == 0 ? PBM_BLACK : PBM_WHITE;

        pbm_writepbmrow(fileP, bitrow, cols, plainFormat);

        pbm_freerow(bitrow);
    }    
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
