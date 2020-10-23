/* pnm.h - header file for libpnm portable anymap library
*/

#ifndef _PNM_H_
#define _PNM_H_

#include "ppm.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif


typedef pixel xel;
typedef pixval xelval;
#define PNM_OVERALLMAXVAL PPM_OVERALLMAXVAL
#define PNM_MAXMAXVAL PPM_MAXMAXVAL
#define PNM_GET1(x) PPM_GETB(x)
#define PNM_ASSIGN1(x,v) PPM_ASSIGN(x,0,0,v)
#define PNM_ASSIGN(x,r,g,b) PPM_ASSIGN(x,r,g,b)
#define PNM_EQUAL(x,y) PPM_EQUAL(x,y)
#define PNM_FORMAT_TYPE(f) PPM_FORMAT_TYPE(f)
#define PNM_DEPTH(newp,p,oldmaxval,newmaxval) \
    PPM_DEPTH(newp,p,oldmaxval,newmaxval)

/* Declarations of routines. */

void pnm_init ARGS(( int* argcP, char* argv[] ));

void
pnm_nextimage(FILE *file, int * const eofP);

xel *
pnm_allocrow(unsigned int const cols);

#define pnm_freerow(xelrow) free(xelrow)

#define pnm_allocarray( cols, rows ) \
  ((xel**) pm_allocarray( cols, rows, sizeof(xel) ))
#define pnm_freearray( xels, rows ) pm_freearray( (char**) xels, rows )


void
pnm_readpnminit(FILE *   const fileP,
                int *    const colsP,
                int *    const rowsP,
                xelval * const maxvalP,
                int *    const formatP);

void
pnm_readpnmrow(FILE * const fileP,
               xel *  const xelrow,
               int    const cols,
               xelval const maxval,
               int    const format);

xel **
pnm_readpnm(FILE *   const fileP,
            int *    const colsP,
            int *    const rowsP,
            xelval * const maxvalP,
            int *    const formatP);

void
pnm_check(FILE *               const fileP,
          enum pm_check_type   const check_type, 
          int                  const format,
          int                  const cols,
          int                  const rows,
          int                  const maxval,
          enum pm_check_code * const retvalP);


void
pnm_writepnminit(FILE * const fileP, 
                 int    const cols, 
                 int    const rows, 
                 xelval const maxval, 
                 int    const format, 
                 int    const forceplain);

void
pnm_writepnmrow(FILE * const fileP, 
                xel *  const xelrow, 
                int    const cols, 
                xelval const maxval, 
                int    const format, 
                int    const forceplain);

void
pnm_writepnm(FILE * const fileP,
             xel ** const xels,
             int    const cols,
             int    const rows,
             xelval const maxval,
             int    const format,
             int    const forceplain);

xel 
pnm_backgroundxel (xel** xels, int cols, int rows, xelval maxval, int format);

xel 
pnm_backgroundxelrow (xel* xelrow, int cols, xelval maxval, int format);

xel 
pnm_whitexel (xelval maxval, int format);

xel 
pnm_blackxel(xelval maxval, int format);

void 
pnm_invertxel(xel *  const x,
              xelval const maxval,
              int    const format);

void 
pnm_promoteformat(xel** xels, int cols, int rows, xelval maxval, int format, 
                  xelval newmaxval, int newformat);
void 
pnm_promoteformatrow(xel* xelrow, int cols, xelval maxval, int format, 
                     xelval newmaxval, int newformat);

pixel
pnm_xeltopixel(xel const inputxel,
               int const format);

#ifdef __cplusplus
}
#endif


#endif /*_PNM_H_*/
