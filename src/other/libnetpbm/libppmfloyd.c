/* 
These functions were taken from Ingo Wilken's ilbm package by Bryan
Henderson on 01.03.10.  Because ppmtoilbm and ilbmtoppm are the only
programs that will use these in the foreseeable future, they remain
lightly documented and tested. 

But they look like they would be useful in other Netpbm programs that
do Floyd-Steinberg.
*/



/* libfloyd.c - generic Floyd-Steinberg error distribution routines for PBMPlus
**
** Copyright (C) 1994 Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "ppm.h"
#include "ppmfloyd.h"
#include "mallocvar.h"



static void
fs_adjust(ppm_fs_info * const fi, 
          int           const col) {

    int     const errcol = col+1;
    pixel * const pP     = &(fi->pixrow[col]);
    pixval  const maxval = fi->maxval;

    long r, g, b;

    /* Use Floyd-Steinberg errors to adjust actual color. */
    r = fi->thisrederr  [errcol]; if( r < 0 ) r -= 8; else r += 8; r /= 16;
    g = fi->thisgreenerr[errcol]; if( g < 0 ) g -= 8; else g += 8; g /= 16;
    b = fi->thisblueerr [errcol]; if( b < 0 ) b -= 8; else b += 8; b /= 16;

    r += PPM_GETR(*pP); if ( r < 0 ) r = 0; else if ( r > maxval ) r = maxval;
    g += PPM_GETG(*pP); if ( g < 0 ) g = 0; else if ( g > maxval ) g = maxval;
    b += PPM_GETB(*pP); if ( b < 0 ) b = 0; else if ( b > maxval ) b = maxval;

    PPM_ASSIGN(*pP, r, g, b);
    fi->red = r; fi->green = g; fi->blue = b;
}



static ppm_fs_info *
allocateFi(int const cols) {

    ppm_fs_info * fi;

    MALLOCVAR(fi);
    
    if (fi != NULL) {
        MALLOCARRAY(fi->thisrederr  , cols + 2);
        MALLOCARRAY(fi->thisgreenerr, cols + 2);
        MALLOCARRAY(fi->thisblueerr , cols + 2);
        MALLOCARRAY(fi->nextrederr  , cols + 2);
        MALLOCARRAY(fi->nextgreenerr, cols + 2);
        MALLOCARRAY(fi->nextblueerr , cols + 2);
        
        if (fi->thisrederr   == NULL || 
            fi->thisgreenerr == NULL || 
            fi->thisblueerr  == NULL ||
            fi->nextrederr   == NULL || 
            fi->nextgreenerr == NULL || 
            fi->nextblueerr  == NULL)
            pm_error("out of memory allocating "
                     "Floyd-Steinberg control structure");
    } else
        pm_error("out of memory allocating Floyd-Steinberg control structure");

    return(fi);
}



ppm_fs_info *
ppm_fs_init(int cols, pixval maxval, int flags) {

    ppm_fs_info *fi;
    
    fi = allocateFi(cols);

    fi->lefttoright = 1;
    fi->cols = cols;
    fi->maxval = maxval;
    fi->flags = flags;
    
    if( flags & FS_RANDOMINIT ) {
        unsigned int i;
        srand((int)(time(0) ^ getpid()));
        for( i = 0; i < cols +2; i++ ) {
            /* random errors in [-1..+1] */
            fi->thisrederr[i]   = rand() % 32 - 16;
            fi->thisgreenerr[i] = rand() % 32 - 16;
            fi->thisblueerr[i]  = rand() % 32 - 16;
        }
    }
    else {
        unsigned int i;

        for( i = 0; i < cols + 2; i++ )
            fi->thisrederr[i] = fi->thisgreenerr[i] = 
                fi->thisblueerr[i] = 0;
    }
    return fi;
}



void
ppm_fs_free(fi)
    ppm_fs_info *fi;
{
    if( fi ) {
        free(fi->thisrederr); free(fi->thisgreenerr); free(fi->thisblueerr);
        free(fi->nextrederr); free(fi->nextgreenerr); free(fi->nextblueerr);
        free(fi);
    }
}


int
ppm_fs_startrow(fi, pixrow)
    ppm_fs_info *fi;
    pixel *pixrow;
{
    register int col;

    if( !fi )
        return 0;

    fi->pixrow = pixrow;

    for( col = 0; col < fi->cols + 2; col++ )
        fi->nextrederr[col] = fi->nextgreenerr[col] = fi->nextblueerr[col] = 0;

    if( fi->lefttoright ) {
        fi->col_end = fi->cols;
        col = 0;
    }
    else {
        fi->col_end = -1;
        col = fi->cols - 1;
    }
    fs_adjust(fi, col);
    return col;
}


int
ppm_fs_next(fi, col)
    ppm_fs_info *fi;
    int col;
{
    if( !fi )
        ++col;
    else {
        if( fi->lefttoright )
            ++col;
        else
            --col;
        if( col == fi->col_end )
            col = fi->cols;
        else
            fs_adjust(fi, col);
    }
    return col;
}


void
ppm_fs_endrow(fi)
    ppm_fs_info *fi;
{
    long *tmp;

    if( fi ) {
        tmp = fi->thisrederr;   fi->thisrederr   = fi->nextrederr;   fi->nextrederr   = tmp;
        tmp = fi->thisgreenerr; fi->thisgreenerr = fi->nextgreenerr; fi->nextgreenerr = tmp;
        tmp = fi->thisblueerr;  fi->thisblueerr  = fi->nextblueerr;  fi->nextblueerr  = tmp;
        if( fi->flags & FS_ALTERNATE )
            fi->lefttoright = !(fi->lefttoright);
    }
}


void
ppm_fs_update(fi, col, pP)
    ppm_fs_info *fi;
    int col;
    pixel *pP;
{
    if( fi )
        ppm_fs_update3(fi, col, PPM_GETR(*pP), PPM_GETG(*pP), PPM_GETB(*pP));
}


void
ppm_fs_update3(ppm_fs_info * const fi, 
               int           const col, 
               pixval        const r, 
               pixval        const g, 
               pixval        const b) {

    int const errcol = col + 1;
    long err;

    if (fi) {
        long const rerr = (long)(fi->red)   - (long)r;
        long const gerr = (long)(fi->green) - (long)g;
        long const berr = (long)(fi->blue)  - (long)b;
    
        if ( fi->lefttoright ) {
            long two_err;

            two_err = 2*rerr;
            err = rerr;     fi->nextrederr[errcol+1] += err;    /* 1/16 */
            err += two_err; fi->nextrederr[errcol-1] += err;    /* 3/16 */
            err += two_err; fi->nextrederr[errcol  ] += err;    /* 5/16 */
            err += two_err; fi->thisrederr[errcol+1] += err;    /* 7/16 */

            two_err = 2*gerr;
            err = gerr;     fi->nextgreenerr[errcol+1] += err;    /* 1/16 */
            err += two_err; fi->nextgreenerr[errcol-1] += err;    /* 3/16 */
            err += two_err; fi->nextgreenerr[errcol  ] += err;    /* 5/16 */
            err += two_err; fi->thisgreenerr[errcol+1] += err;    /* 7/16 */

            two_err = 2*berr;
            err = berr;     fi->nextblueerr[errcol+1] += err;    /* 1/16 */
            err += two_err; fi->nextblueerr[errcol-1] += err;    /* 3/16 */
            err += two_err; fi->nextblueerr[errcol  ] += err;    /* 5/16 */
            err += two_err; fi->thisblueerr[errcol+1] += err;    /* 7/16 */
        }
        else {
            long two_err;

            two_err = 2*rerr;
            err = rerr;     fi->nextrederr[errcol-1] += err;    /* 1/16 */
            err += two_err; fi->nextrederr[errcol+1] += err;    /* 3/16 */
            err += two_err; fi->nextrederr[errcol  ] += err;    /* 5/16 */
            err += two_err; fi->thisrederr[errcol-1] += err;    /* 7/16 */

            two_err = 2*gerr;
            err = gerr;     fi->nextgreenerr[errcol-1] += err;    /* 1/16 */
            err += two_err; fi->nextgreenerr[errcol+1] += err;    /* 3/16 */
            err += two_err; fi->nextgreenerr[errcol  ] += err;    /* 5/16 */
            err += two_err; fi->thisgreenerr[errcol-1] += err;    /* 7/16 */

            two_err = 2*berr;
            err = berr;     fi->nextblueerr[errcol-1] += err;    /* 1/16 */
            err += two_err; fi->nextblueerr[errcol+1] += err;    /* 3/16 */
            err += two_err; fi->nextblueerr[errcol  ] += err;    /* 5/16 */
            err += two_err; fi->thisblueerr[errcol-1] += err;    /* 7/16 */
        }
    }
}



