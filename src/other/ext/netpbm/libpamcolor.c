/*============================================================================
                                  libpamcolor.c
==============================================================================
  These are the library functions, which belong in the libnetpbm library,
  that deal with colors in the PAM image format.

  This file was originally written by Bryan Henderson and is contributed
  to the public domain by him and subsequent authors.
=============================================================================*/

/* See libpbm.c for the complicated explanation of this 32/64 bit file
   offset stuff.
*/
#define _FILE_OFFSET_BITS 64
#define _LARGE_FILES  

#ifndef __APPLE__
#define _BSD_SOURCE 1      /* Make sure strdup() is in string.h */
#define _XOPEN_SOURCE 500  /* Make sure strdup() is in string.h */
#endif

#include <string.h>
#include <limits.h>

#include "pm_c_util.h"

#include "pam.h"
#include "ppm.h"



tuple
pnm_parsecolor(const char * const colorname,
               sample       const maxval) {

    tuple retval;
    pixel color;
    struct pam pam;

    pam.len = PAM_STRUCT_SIZE(bytes_per_sample);
    pam.depth = 3;
    pam.maxval = maxval;
    pam.bytes_per_sample = pnm_bytespersample(maxval);

    retval = pnm_allocpamtuple(&pam);

    color = ppm_parsecolor(colorname, maxval);

    retval[PAM_RED_PLANE] = PPM_GETR(color);
    retval[PAM_GRN_PLANE] = PPM_GETG(color);
    retval[PAM_BLU_PLANE] = PPM_GETB(color);

    return retval;
}



const char *
pnm_colorname(struct pam * const pamP,
              tuple        const color,
              int          const hexok) {

    const char * retval;
    pixel colorp;
    char * colorname;

    if (pamP->depth < 3)
        PPM_ASSIGN(colorp, color[0], color[0], color[0]);
    else 
        PPM_ASSIGN(colorp,
                   color[PAM_RED_PLANE],
                   color[PAM_GRN_PLANE],
                   color[PAM_BLU_PLANE]);

    colorname = ppm_colorname(&colorp, pamP->maxval, hexok);

    retval = strdup(colorname);
    if (retval == NULL)
        pm_error("Couldn't get memory for color name string");

    return retval;
}



double pnm_lumin_factor[3] = {PPM_LUMINR, PPM_LUMING, PPM_LUMINB};

void
pnm_YCbCrtuple(tuple    const tuple, 
               double * const YP, 
               double * const CbP, 
               double * const CrP) {
/*----------------------------------------------------------------------------
   Assuming that the tuple 'tuple' is of tupletype RGB, return the 
   Y/Cb/Cr representation of the color represented by the tuple.
-----------------------------------------------------------------------------*/
    int const red = (int)tuple[PAM_RED_PLANE];
    int const grn = (int)tuple[PAM_GRN_PLANE];
    int const blu = (int)tuple[PAM_BLU_PLANE];
    
    *YP  = (+ PPM_LUMINR * red + PPM_LUMING * grn + PPM_LUMINB * blu);
    *CbP = (- 0.16874 * red - 0.33126 * grn + 0.50000 * blu);
    *CrP = (+ 0.50000 * red - 0.41869 * grn - 0.08131 * blu);
}



void 
pnm_YCbCr_to_rgbtuple(const struct pam * const pamP,
                      tuple              const tuple,
                      double             const Y,
                      double             const Cb, 
                      double             const Cr,
                      int *              const overflowP) {

    double rgb[3];
    unsigned int plane;
    bool overflow;

    rgb[PAM_RED_PLANE] = Y + 1.4022 * Cr + .5;
    rgb[PAM_GRN_PLANE] = Y - 0.7145 * Cr - 0.3456 * Cb + .5;
    rgb[PAM_BLU_PLANE] = Y + 1.7710 * Cb + .5;

    overflow = FALSE;  /* initial assumption */

    for (plane = 0; plane < 3; ++plane) {
        if (rgb[plane] > pamP->maxval) {
            overflow = TRUE;
            tuple[plane] = pamP->maxval;
        } else if (rgb[plane] < 0.0) {
            overflow = TRUE;
            tuple[plane] = 0;
        } else 
            tuple[plane] = (sample)rgb[plane];
    }
    if (overflowP)
        *overflowP = overflow;
}



