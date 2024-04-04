/* libpnm3.c - pnm utility library part 3
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "pnm.h"
#include "ppm.h"
#include "pgm.h"
#include "pbm.h"



static xel
mean4(int const format,
      xel const a,
      xel const b,
      xel const c,
      xel const d) {
/*----------------------------------------------------------------------------
   Return cartesian mean of the 4 colors.
-----------------------------------------------------------------------------*/
    xel retval;

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        PPM_ASSIGN(
            retval,
            (PPM_GETR(a) + PPM_GETR(b) + PPM_GETR(c) + PPM_GETR(d)) / 4,
            (PPM_GETG(a) + PPM_GETG(b) + PPM_GETG(c) + PPM_GETG(d)) / 4,
            (PPM_GETB(a) + PPM_GETB(b) + PPM_GETB(c) + PPM_GETB(d)) / 4);
        break;

    case PGM_TYPE:
    case PBM_TYPE:
        PNM_ASSIGN1(
            retval,
            (PNM_GET1(a) + PNM_GET1(b) + PNM_GET1(c) + PNM_GET1(d))/4);
        break;

    default:
        pm_error("Invalid format passed to pnm_backgroundxel()");
    }
    return retval;
}



xel
pnm_backgroundxel(xel**  const xels,
                  int    const cols,
                  int    const rows,
                  xelval const maxval,
                  int    const format) {

    xel bgxel, ul, ur, ll, lr;

    /* Guess a good background value. */
    ul = xels[0][0];
    ur = xels[0][cols-1];
    ll = xels[rows-1][0];
    lr = xels[rows-1][cols-1];

    /* We first recognize three corners equal.  If not, we look for any
       two.  If not, we just average all four.
    */
    if (PNM_EQUAL(ul, ur) && PNM_EQUAL(ur, ll))
        bgxel = ul;
    else if (PNM_EQUAL(ul, ur) && PNM_EQUAL(ur, lr))
        bgxel = ul;
    else if (PNM_EQUAL(ul, ll) && PNM_EQUAL(ll, lr))
        bgxel = ul;
    else if (PNM_EQUAL(ur, ll) && PNM_EQUAL(ll, lr))
        bgxel = ur;
    else if (PNM_EQUAL(ul, ur))
        bgxel = ul;
    else if (PNM_EQUAL(ul, ll))
        bgxel = ul;
    else if (PNM_EQUAL(ul, lr))
        bgxel = ul;
    else if (PNM_EQUAL(ur, ll))
        bgxel = ur;
    else if (PNM_EQUAL(ur, lr))
        bgxel = ur;
    else if (PNM_EQUAL(ll, lr))
        bgxel = ll;
    else
        bgxel = mean4(format, ul, ur, ll, lr);

    return bgxel;
}



xel
pnm_backgroundxelrow(xel *  const xelrow,
                     int    const cols,
                     xelval const maxval,
                     int    const format) {

    xel bgxel, l, r;

    /* Guess a good background value. */
    l = xelrow[0];
    r = xelrow[cols-1];

    if (PNM_EQUAL(l, r))
        /* Both corners are same color, so that's the background color,
           without any extra computation.
        */
        bgxel = l;
    else {
        /* Corners are different, so use cartesian mean of them */
        switch (PNM_FORMAT_TYPE(format)) {
        case PPM_TYPE:
            PPM_ASSIGN(bgxel,
                       (PPM_GETR(l) + PPM_GETR(r)) / 2,
                       (PPM_GETG(l) + PPM_GETG(r)) / 2,
                       (PPM_GETB(l) + PPM_GETB(r)) / 2
                );
            break;

        case PGM_TYPE:
            PNM_ASSIGN1(bgxel, (PNM_GET1(l) + PNM_GET1(r)) / 2);
            break;

        case PBM_TYPE: {
            unsigned int col, blackCnt;

            /* One black, one white.  Gotta count. */
            for (col = 0, blackCnt = 0; col < cols; ++col) {
                if (PNM_GET1(xelrow[col] ) == 0)
                    ++blackCnt;
            }
            if (blackCnt >= cols / 2)
                PNM_ASSIGN1(bgxel, 0);
            else
                PNM_ASSIGN1(bgxel, maxval);
            break;
        }

        default:
            pm_error("Invalid format passed to pnm_backgroundxelrow()");
        }
    }

    return bgxel;
}



xel
pnm_whitexel(xelval const maxval,
             int    const format) {

    xel retval;

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        PPM_ASSIGN(retval, maxval, maxval, maxval);
        break;

    case PGM_TYPE:
    case PBM_TYPE:
        PNM_ASSIGN1(retval, maxval);
        break;

    default:
        pm_error("Invalid format %d passed to pnm_whitexel()", format);
    }

    return retval;
}



xel
pnm_blackxel(xelval const maxval,
             int    const format) {

    xel retval;

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        PPM_ASSIGN(retval, 0, 0, 0);
        break;

    case PGM_TYPE:
    case PBM_TYPE:
        PNM_ASSIGN1(retval, 0);
        break;

    default:
        pm_error("Invalid format %d passed to pnm_blackxel()", format);
    }
    
    return retval;
}



void
pnm_invertxel(xel*   const xP, 
              xelval const maxval, 
              int    const format) {

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        PPM_ASSIGN(*xP, 
                   maxval - PPM_GETR(*xP),
                   maxval - PPM_GETG(*xP), 
                   maxval - PPM_GETB(*xP));
        break;

    case PGM_TYPE:
        PNM_ASSIGN1(*xP, maxval - PNM_GET1(*xP));
        break;

    case PBM_TYPE:
        PNM_ASSIGN1(*xP, (PNM_GET1(*xP) == 0) ? maxval : 0);
        break;

    default:
        pm_error("Invalid format passed to pnm_invertxel()");
    }
}



void
pnm_promoteformat( xel** xels, int cols, int rows, xelval maxval, int format, xelval newmaxval, int newformat )
    {
    int row;

    for ( row = 0; row < rows; ++row )
    pnm_promoteformatrow(
        xels[row], cols, maxval, format, newmaxval, newformat );
    }

void
pnm_promoteformatrow( xel* xelrow, int cols, xelval maxval, int format, xelval newmaxval, int newformat )
    {
    register int col;
    register xel* xP;

    if ( ( PNM_FORMAT_TYPE(format) == PPM_TYPE &&
       ( PNM_FORMAT_TYPE(newformat) == PGM_TYPE ||
         PNM_FORMAT_TYPE(newformat) == PBM_TYPE ) ) ||
     ( PNM_FORMAT_TYPE(format) == PGM_TYPE &&
       PNM_FORMAT_TYPE(newformat) == PBM_TYPE ) )
    pm_error( "pnm_promoteformatrow: can't promote downwards!" );

    /* Are we promoting to the same type? */
    if ( PNM_FORMAT_TYPE(format) == PNM_FORMAT_TYPE(newformat) )
    {
    if ( PNM_FORMAT_TYPE(format) == PBM_TYPE )
        return;
    if ( newmaxval < maxval )
        pm_error(
       "pnm_promoteformatrow: can't decrease maxval - try using pnmdepth" );
    if ( newmaxval == maxval )
        return;
    /* Increase maxval. */
    switch ( PNM_FORMAT_TYPE(format) )
        {
        case PGM_TYPE:
        for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
        PNM_ASSIGN1(
            *xP, (int) PNM_GET1(*xP) * newmaxval / maxval );
        break;

        case PPM_TYPE:
        for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
        PPM_DEPTH( *xP, *xP, maxval, newmaxval );
        break;

        default:
        pm_error( "Invalid old format passed to pnm_promoteformatrow()" );
        }
    return;
    }

    /* We must be promoting to a higher type. */
    switch ( PNM_FORMAT_TYPE(format) )
    {
    case PBM_TYPE:
    switch ( PNM_FORMAT_TYPE(newformat) )
        {
        case PGM_TYPE:
        for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
        if ( PNM_GET1(*xP) == 0 )
            PNM_ASSIGN1( *xP, 0 );
        else
            PNM_ASSIGN1( *xP, newmaxval );
        break;

        case PPM_TYPE:
        for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
        if ( PNM_GET1(*xP) == 0 )
            PPM_ASSIGN( *xP, 0, 0, 0 );
        else
            PPM_ASSIGN( *xP, newmaxval, newmaxval, newmaxval );
        break;

        default:
        pm_error( "Invalid new format passed to pnm_promoteformatrow()" );
        }
    break;

    case PGM_TYPE:
    switch ( PNM_FORMAT_TYPE(newformat) )
        {
        case PPM_TYPE:
        if ( newmaxval < maxval )
        pm_error(
       "pnm_promoteformatrow: can't decrease maxval - try using pnmdepth" );
        if ( newmaxval == maxval )
        {
        for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
            PPM_ASSIGN(
            *xP, PNM_GET1(*xP), PNM_GET1(*xP), PNM_GET1(*xP) );
        }
        else
        { /* Increase maxval. */
        for ( col = 0, xP = xelrow; col < cols; ++col, ++xP )
            PPM_ASSIGN(
            *xP, (int) PNM_GET1(*xP) * newmaxval / maxval,
            (int) PNM_GET1(*xP) * newmaxval / maxval,
            (int) PNM_GET1(*xP) * newmaxval / maxval );
        }
        break;

        default:
        pm_error( "Invalid new format passed to pnm_promoteformatrow()" );
        }
    break;

    default:
        pm_error( "Invalid old format passed to pnm_promoteformatrow()" );
    }
    }


pixel
pnm_xeltopixel(xel const inputxel,
               int const format) {
    
    pixel outputpixel;

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        PPM_ASSIGN(outputpixel,
                   PPM_GETR(inputxel),
                   PPM_GETG(inputxel),
                   PPM_GETB(inputxel));
        break;
    case PGM_TYPE:
    case PBM_TYPE:
        PPM_ASSIGN(outputpixel,
                   PNM_GET1(inputxel),
                   PNM_GET1(inputxel),
                   PNM_GET1(inputxel));
        break;
    default:
        pm_error("Invalid format code %d passed to pnm_xeltopixel()",
                 format);
    }

    return outputpixel;
}



xel
pnm_parsecolorxel(const char * const colorName,
                  xelval       const maxval,
                  int          const format) {

    pixel const bgColor = ppm_parsecolor(colorName, maxval);

    xel retval;

    switch(PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
        PNM_ASSIGN(retval,
                   PPM_GETR(bgColor), PPM_GETG(bgColor), PPM_GETB(bgColor));
        break;
    case PGM_TYPE:
        if (PPM_ISGRAY(bgColor))
            PNM_ASSIGN1(retval, PPM_GETB(bgColor));
        else
            pm_error("Non-gray color '%s' specified for a "
                     "grayscale (PGM) image",
                     colorName);
                   break;
    case PBM_TYPE:
        if (PPM_EQUAL(bgColor, ppm_whitepixel(maxval)))
            PNM_ASSIGN1(retval, maxval);
        else if (PPM_EQUAL(bgColor, ppm_blackpixel()))
            PNM_ASSIGN1(retval, 0);
        else
            pm_error ("Color '%s', which is neither black nor white, "
                      "specified for a black and white (PBM) image",
                      colorName);
        break;
    default:
        pm_error("Invalid format code %d passed to pnm_parsecolorxel()",
                 format);
    }
    
    return retval;
}
