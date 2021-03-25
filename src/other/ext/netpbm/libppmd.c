/* 
**
** This library module contains the ppmdraw routines.
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
** 
** The character drawing routines are by John Walker
** Copyright (C) 1994 by John Walker, kelvin@fourmilab.ch
*/

#include <assert.h>
#include <stdlib.h>

#include "pm_config.h"
#include "pm_c_util.h"
#include "mallocvar.h"
#include "ppm.h"
#include "ppmdfont.h"
#include "ppmdraw.h"



struct penpos {
    int x;
    int y;
};

static long int const DDA_SCALE = 8192;

#define PPMD_MAXCOORD 32767
/*
  Several factors govern the limit of x, y coordination values.

  The limit must be representable as (signed) int for coordinates to 
  be carried in struct penpos (immediately above).

  The following calculation, done with long ints, must not overflow:
  cy0 = cy0 + (y1 - cy0) * (cols - 1 - cx0) / (x1 - cx0);

  The following must not overflow when DDA_SCALE is set to 8092:
  dy = (y1 - y0) * DDA_SCALE / abs(x1 - x0);

  Overflow conditions for ppmd_text are rather complicated, for commands
  come from an external PPMD font file.  See comments below.  
*/



static void
ppmd_validateCoords(int const x,
                    int const y) {

    if (x < -PPMD_MAXCOORD || x > PPMD_MAXCOORD)
        pm_error("x coordinate out of bounds: %d", x);

    if (y < -PPMD_MAXCOORD || y > PPMD_MAXCOORD)
        pm_error("y coordinate out of bounds: %d", y);
}



static void
drawPoint(ppmd_drawproc       drawproc,
          const void *  const clientdata,
          pixel **      const pixels, 
          int           const cols, 
          int           const rows, 
          pixval        const maxval, 
          int           const x, 
          int           const y) {
/*----------------------------------------------------------------------------
   Draw a single point, assuming that it is within the bounds of the
   image.
-----------------------------------------------------------------------------*/
    if (drawproc == PPMD_NULLDRAWPROC) {
        const pixel * const pixelP = clientdata;
        
        assert(x >= 0); assert(x < cols);
        assert(y >= 0); assert(y < rows);

        pixels[y][x] = *pixelP;
    } else
        drawproc(pixels, cols, rows, maxval, x, y, clientdata);
}



void
ppmd_point_drawproc(pixel**     const pixels, 
                    int         const cols, 
                    int         const rows, 
                    pixval      const maxval, 
                    int         const x, 
                    int         const y, 
                    const void* const clientdata) {

    if (x >= 0 && x < cols && y >= 0 && y < rows)
        pixels[y][x] = *((pixel*)clientdata);
}


/* Simple fill routine. */

void
ppmd_filledrectangle(pixel **      const pixels, 
                     int           const cols, 
                     int           const rows, 
                     pixval        const maxval, 
                     int           const x, 
                     int           const y, 
                     int           const width, 
                     int           const height, 
                     ppmd_drawproc      drawProc,
                     const void *  const clientdata) {

    int cx, cy, cwidth, cheight, row;

    /* Clip. */
    cx = x;
    cy = y;
    cwidth = width;
    cheight = height;

    if (cx < 0) {
        cx = 0;
        cwidth += x;
    }
    if (cy < 0) {
        cy = 0;
        cheight += y;
    }
    if (cx + cwidth > cols)
        cwidth = cols - cx;

    if (cy + cheight > rows)
        cheight = rows - cy;

    /* Draw. */
    for (row = cy; row < cy + cheight; ++row) {
        int col;
        for (col = cx; col < cx + cwidth; ++col)
            drawPoint(drawProc, clientdata,
                      pixels, cols, rows, maxval, col, row);
    }
}


/* Outline drawing stuff. */

static int linetype = PPMD_LINETYPE_NORMAL;

int
ppmd_setlinetype(int const type) {

    int old;

    old = linetype;
    linetype = type;
    return old;
}



static bool lineclip = TRUE;



int
ppmd_setlineclip(int const newSetting) {

    bool previousSetting;

    previousSetting = lineclip;

    lineclip = newSetting;

    return previousSetting;
}



static void
clipEnd0(int    const x0,
         int    const y0,
         int    const x1,
         int    const y1,
         int    const cols,
         int    const rows,
         int *  const cx0P,
         int *  const cy0P,
         bool * const noLineP) {
/*----------------------------------------------------------------------------
   Given a line that goes from (x0, y0) to (x1, y1), where any of
   these coordinates may be anywhere in space -- not just in the frame,
   clip the (x0, y0) end to bring it into the frame.  
   Return the clipped-to location as (*cx0P, *cy0P).

   Iff this is not possible because the entire line described is
   outside the frame, return *nolineP == true.

   The frame is 'cols' columns starting at 0, by 'rows' rows starting
   at 0.
-----------------------------------------------------------------------------*/

    int cx0, cy0;
    bool noLine;

    cx0 = x0;        /* initial value */
    cy0 = y0;        /* initial value */
    noLine = FALSE;  /* initial value */

    /* Clip End 0 of the line horizontally */
    if (cx0 < 0) {
        if (x1 < 0)
            noLine = TRUE;
        else {
            cy0 = cy0 + (y1 - cy0) * (-cx0) / (x1 - cx0);
            cx0 = 0;
        }
    } else if (cx0 >= cols) {
        if (x1 >= cols)
            noLine = TRUE;
        else {
            cy0 = cy0 + (y1 - cy0) * (cols - 1 - cx0) / (x1 - cx0);
            cx0 = cols - 1;
        }
    }

    /* Clip End 0 of the line vertically */
    if (cy0 < 0) {
        if (y1 < 0)
            noLine = TRUE;
        else {
            cx0 = cx0 + (x1 - cx0) * (-cy0) / (y1 - cy0);
            cy0 = 0;
        }
    } else if (cy0 >= rows) {
        if (y1 >= rows)
            noLine = TRUE;
        else {
            cx0 = cx0 + (x1 - cx0) * (rows - 1 - cy0) / (y1 - cy0);
            cy0 = rows - 1;
        }
    }

    /* Clipping vertically may have moved the endpoint out of frame
       horizontally.  If so, we know the other endpoint is also out of
       frame horizontally and the line misses the frame entirely.
    */
    if (cx0 < 0 || cx0 >= cols) {
        assert(x1 < 0 || x1 >= cols);
        noLine = TRUE;
    }
    *cx0P = cx0;
    *cy0P = cy0;
    *noLineP = noLine;
}



static void
clipEnd1(int    const x0,
         int    const y0,
         int    const x1,
         int    const y1,
         int    const cols,
         int    const rows,
         int *  const cx1P,
         int *  const cy1P) {
/*----------------------------------------------------------------------------
   Given a line that goes from (x0, y0) to (x1, y1), where (x0, y0) is
   within the frame, but (x1, y1) can be anywhere in space, clip the
   (x1, y1) end to bring it into the frame.  Return the clipped-to
   location as (*cx1P, *cy1P).

   This is guaranteed to be possible, since we already know at least one
   point (i.e. (x0, y0)) is in the frame.

   The frame is 'cols' columns starting at 0, by 'rows' rows starting
   at 0.
-----------------------------------------------------------------------------*/
    int cx1, cy1;

    assert(x0 >= 0 && y0 < cols);
    assert(y0 >= 0 && y0 < rows);
    
    /* Clip End 1 of the line horizontally */
    cx1 = x1;  /* initial value */
    cy1 = y1;  /* initial value */
    
    if (cx1 < 0) {
        /* We know the line isn't vertical, since End 0 is in the frame
           and End 1 is left of frame.
        */
        cy1 = cy1 + (y0 - cy1) * (-cx1) / (x0 - cx1);
        cx1 = 0;
    } else if (cx1 >= cols) {
        /* We know the line isn't vertical, since End 0 is in the frame
           and End 1 is right of frame.
        */
        cy1 = cy1 + (y0 - cy1) * (cols - 1 - cx1) / (x0 - cx1);
        cx1 = cols - 1;
    }
    
    /* Clip End 1 of the line vertically */
    if (cy1 < 0) {
        /* We know the line isn't horizontal, since End 0 is in the frame
           and End 1 is above frame.
        */
        cx1 = cx1 + (x0 - cx1) * (-cy1) / (y0 - cy1);
        cy1 = 0;
    } else if (cy1 >= rows) {
        /* We know the line isn't horizontal, since End 0 is in the frame
           and End 1 is below frame.
        */
        cx1 = cx1 + (x0 - cx1) * (rows - 1 - cy1) / (y0 - cy1);
        cy1 = rows - 1;
    }

    *cx1P = cx1;
    *cy1P = cy1;
}



static void
clipLine(int    const x0,
         int    const y0,
         int    const x1,
         int    const y1,
         int    const cols,
         int    const rows,
         int *  const cx0P,
         int *  const cy0P,
         int *  const cx1P,
         int *  const cy1P,
         bool * const noLineP) {
/*----------------------------------------------------------------------------
   Clip the line that goes from (x0, y0) to (x1, y1) so that none of it
   is outside the boundaries of the raster with width 'cols' and height
   'rows'

   The clipped line goes from (*cx0P, *cy0P) to (*cx1P, *cy1P).

   But if the entire line is outside the boundaries (i.e. we clip the
   entire line), return *noLineP true and the other values undefined.
-----------------------------------------------------------------------------*/
    int cx0, cy0, cx1, cy1;
        /* The line we successively modify.  Starts out as the input
           line and ends up as the output line.
        */
    bool noLine;

    clipEnd0(x0, y0, x1, y1, cols, rows, &cx0, &cy0, &noLine);

    if (!noLine) {
        /* (cx0, cy0) is in the frame: */
        assert(cx0 >= 0 && cy0 < cols);
        assert(cy0 >= 0 && cy0 < rows);

        clipEnd1(cx0, cy0, x1, y1, cols, rows, &cx1, &cy1);
    }

    *cx0P = cx0;
    *cy0P = cy0;
    *cx1P = cx1;
    *cy1P = cy1;
    *noLineP = noLine;
}



static void
drawShallowLine(ppmd_drawproc       drawProc,
                const void *  const clientdata,
                pixel **      const pixels, 
                int           const cols, 
                int           const rows, 
                pixval        const maxval, 
                int           const x0, 
                int           const y0,
                int           const x1,
                int           const y1) {
/*----------------------------------------------------------------------------
   Draw a line that is more horizontal than vertical.

   Don't clip.

   Assume the line has distinct start and end points (i.e. it's at least
   two points).
-----------------------------------------------------------------------------*/

    /* Loop over X domain. */
    long dy, srow;
    int dx, col, row, prevrow;

    if (x1 > x0)
        dx = 1;
    else
        dx = -1;
    dy = (y1 - y0) * DDA_SCALE / abs(x1 - x0);
    prevrow = row = y0;
    srow = row * DDA_SCALE + DDA_SCALE / 2;
    col = x0;
    for ( ; ; ) {
        if (linetype == PPMD_LINETYPE_NODIAGS && row != prevrow) {
            drawPoint(drawProc, clientdata,
                      pixels, cols, rows, maxval, col, prevrow);
            prevrow = row;
        }
        drawPoint(drawProc, clientdata, pixels, cols, rows, maxval, col, row);
        if (col == x1)
            break;
        srow += dy;
        row = srow / DDA_SCALE;
        col += dx;
    }
}



static void
drawSteepLine(ppmd_drawproc       drawProc,
              const void *  const clientdata,
              pixel **      const pixels, 
              int           const cols, 
              int           const rows, 
              pixval        const maxval, 
              int           const x0, 
              int           const y0,
              int           const x1,
              int           const y1) {
/*----------------------------------------------------------------------------
   Draw a line that is more vertical than horizontal.

   Don't clip.

   Assume the line has distinct start and end points (i.e. it's at least
   two points).
-----------------------------------------------------------------------------*/
    /* Loop over Y domain. */

    long dx, scol;
    int dy, col, row, prevcol;

    if (y1 > y0)
        dy = 1;
    else
        dy = -1;
    dx = (x1 - x0) * DDA_SCALE / abs(y1 - y0);
    row = y0;
    prevcol = col = x0;
    scol = col * DDA_SCALE + DDA_SCALE / 2;
    for ( ; ; ) {
        if (linetype == PPMD_LINETYPE_NODIAGS && col != prevcol) {
            drawPoint(drawProc, clientdata,
                      pixels, cols, rows, maxval, prevcol, row);
            prevcol = col;
        }
        drawPoint(drawProc, clientdata, pixels, cols, rows, maxval, col, row);
        if (row == y1)
            break;
        row += dy;
        scol += dx;
        col = scol / DDA_SCALE;
    }
}



void
ppmd_line(pixel **      const pixels, 
          int           const cols, 
          int           const rows, 
          pixval        const maxval, 
          int           const x0, 
          int           const y0, 
          int           const x1, 
          int           const y1, 
          ppmd_drawproc       drawProc,
          const void *  const clientdata) {

    int cx0, cy0, cx1, cy1;
    bool noLine;  /* There's no line left after clipping */

    ppmd_validateCoords(cols, rows);
    ppmd_validateCoords(x0, y0);
    ppmd_validateCoords(x1, y1);

    if (lineclip) {
        clipLine(x0, y0, x1, y1, cols, rows, &cx0, &cy0, &cx1, &cy1, &noLine);
    } else {
        cx0 = x0;
        cy0 = y0;
        cx1 = x1;
        cy1 = y1;
        noLine = FALSE;
    }

    if (noLine) {
        /* Nothing to draw */
    } else if (cx0 == cx1 && cy0 == cy1) {
        /* This line is just a point.  Because there aren't two
           distinct endpoints, we have a special case.
        */
        drawPoint(drawProc, clientdata, pixels, cols, rows, maxval, cx0, cy0);
    } else {
        /* Draw, using a simple DDA. */
        if (abs(cx1 - cx0) > abs(cy1 - cy0))
            drawShallowLine(drawProc, clientdata, pixels, cols, rows, maxval,
                            cx0, cy0, cx1, cy1);
        else
            drawSteepLine(drawProc, clientdata, pixels, cols, rows, maxval,
                          cx0, cy0, cx1, cy1);
    }
}



#define SPLINE_THRESH 3
void
ppmd_spline3(pixel **      const pixels, 
             int           const cols, 
             int           const rows, 
             pixval        const maxval, 
             int           const x0, 
             int           const y0, 
             int           const x1, 
             int           const y1, 
             int           const x2, 
             int           const y2, 
             ppmd_drawproc       drawProc,
             const void *  const clientdata) {

    register int xa, ya, xb, yb, xc, yc, xp, yp;

    xa = ( x0 + x1 ) / 2;
    ya = ( y0 + y1 ) / 2;
    xc = ( x1 + x2 ) / 2;
    yc = ( y1 + y2 ) / 2;
    xb = ( xa + xc ) / 2;
    yb = ( ya + yc ) / 2;

    xp = ( x0 + xb ) / 2;
    yp = ( y0 + yb ) / 2;
    if ( abs( xa - xp ) + abs( ya - yp ) > SPLINE_THRESH )
        ppmd_spline3(
            pixels, cols, rows, maxval, x0, y0, xa, ya, xb, yb, drawProc,
            clientdata );
    else
        ppmd_line(
            pixels, cols, rows, maxval, x0, y0, xb, yb, drawProc, clientdata);

    xp = ( x2 + xb ) / 2;
    yp = ( y2 + yb ) / 2;
    if ( abs( xc - xp ) + abs( yc - yp ) > SPLINE_THRESH )
        ppmd_spline3(
            pixels, cols, rows, maxval, xb, yb, xc, yc, x2, y2, drawProc,
            clientdata );
    else
        ppmd_line(
            pixels, cols, rows, maxval, xb, yb, x2, y2,
            drawProc, clientdata );
}



void
ppmd_polyspline(pixel **      const pixels, 
                int           const cols, 
                int           const rows, 
                pixval        const maxval, 
                int           const x0, 
                int           const y0, 
                int           const nc, 
                int *         const xc, 
                int *         const yc, 
                int           const x1, 
                int           const y1, 
                ppmd_drawproc       drawProc,
                const void *  const clientdata) {

    register int i, x, y, xn, yn;

    x = x0;
    y = y0;
    for ( i = 0; i < nc - 1; ++i )
    {
        xn = ( xc[i] + xc[i + 1] ) / 2;
        yn = ( yc[i] + yc[i + 1] ) / 2;
        ppmd_spline3(
            pixels, cols, rows, maxval, x, y, xc[i], yc[i], xn, yn, drawProc,
            clientdata );
        x = xn;
        y = yn;
    }
    ppmd_spline3(
        pixels, cols, rows, maxval, x, y, xc[nc - 1], yc[nc - 1], x1, y1,
        drawProc, clientdata );
}



void
ppmd_spline4(pixel **      const pixels, 
             int           const cols, 
             int           const rows, 
             pixval        const maxval, 
             int           const x0, 
             int           const y0, 
             int           const x1, 
             int           const y1, 
             int           const x2, 
             int           const y2, 
             int           const x3, 
             int           const y3, 
             ppmd_drawproc       drawproc,
             const void *  const clientdata) {

    pm_error("ppmd_spline4() has not been written yet!");

}



void
ppmd_circle(pixel **      const pixels, 
            int           const cols, 
            int           const rows, 
            pixval        const maxval, 
            int           const cx, 
            int           const cy, 
            int           const radius, 
            ppmd_drawproc       drawProc,
            const void *  const clientdata) {

    int x0, y0, x, y, prevx, prevy, nopointsyet;
    long sx, sy, e;

    if (radius < 0)
        pm_error("Error drawing circle.  Radius must be positive: %d", radius);
    else if (radius == 0)
        return;
    else if (radius >= DDA_SCALE)
        pm_error("Error drawing circle.  Radius too large: %d", radius);

    ppmd_validateCoords(cx + radius, cy + radius);
    ppmd_validateCoords(cx - radius, cy - radius);

    x0 = x = radius;
    y0 = y = 0;
    sx = x * DDA_SCALE + DDA_SCALE / 2;
    sy = y * DDA_SCALE + DDA_SCALE / 2;
    e = DDA_SCALE / radius;

    /* If lineclip is on, draw only points within pixmap.
       Initial point is 3 o'clock. 
       If lineclip is off, "draw" all points (by designated drawproc).
    */

    if ((x + cx >= 0 && x + cx < cols && y + cy >= 0 && y + cy < rows) ||
        !lineclip)
        drawPoint(drawProc, clientdata,
                  pixels, cols, rows, maxval, x + cx, y + cy);
    nopointsyet = 1;

    do {
        prevx = x;
        prevy = y;
        sx += e * sy / DDA_SCALE;
        sy -= e * sx / DDA_SCALE;
        x = sx / DDA_SCALE;
        y = sy / DDA_SCALE;
        if (x != prevx || y != prevy) {
            nopointsyet = 0;
            if ((x + cx >= 0 && x + cx < cols && y + cy >= 0 && y + cy < rows)
                || !lineclip) 
                drawPoint(drawProc, clientdata,
                          pixels, cols, rows, maxval, x + cx, y + cy);
        }
    }
    while (nopointsyet || x != x0 || y != y0);
}



/* Arbitrary fill stuff. */

typedef struct
{
    int x;
    int y;
    int edge;
} coord;

typedef struct fillobj {
    int n;
    int size;
    int curedge;
    int segstart;
    int ydir;
    int startydir;
    coord * coords;
} fillobj;

#define SOME 1000

static int oldclip;

struct fillobj *
ppmd_fill_create(void) {

    fillobj * fillObjP;

    MALLOCVAR(fillObjP);
    if (fillObjP == NULL)
        pm_error("out of memory allocating a fillhandle");
    fillObjP->n = 0;
    fillObjP->size = SOME;
    MALLOCARRAY(fillObjP->coords, fillObjP->size);
    if (fillObjP->coords == NULL)
        pm_error("out of memory allocating a fillhandle");
    fillObjP->curedge = 0;
    
    /* Turn off line clipping. */
    /* UGGH! We must eliminate this global variable */
    oldclip = ppmd_setlineclip(0);
    
    return fillObjP;
}



char *
ppmd_fill_init(void) {
/*----------------------------------------------------------------------------
   Backward compatibility interface.  This is what was used before
   ppmd_fill_create() existed.

   Note that old programs treat a fill handle as a pointer to char
   rather than a pointer to fillObj, and backward compatibility
   depends upon the fact that these are implemented as identical types
   (an address).
-----------------------------------------------------------------------------*/
    return (char *)ppmd_fill_create();
}



void
ppmd_fill_destroy(struct fillobj * fillObjP) {

    free(fillObjP->coords);
    free(fillObjP);
}


void
ppmd_fill_drawproc(pixel**      const pixels, 
                   int          const cols, 
                   int          const rows, 
                   pixval       const maxval, 
                   int          const x, 
                   int          const y, 
                   const void * const clientdata) {

    fillobj * fh;
    coord * cp;
    coord * ocp;

    fh = (fillobj*) clientdata;

    if (fh->n > 0) {
        /* If these are the same coords we saved last time, don't bother. */
        ocp = &(fh->coords[fh->n - 1]);
        if ( x == ocp->x && y == ocp->y )
            return;
    }

    /* Ok, these are new; check if there's room for two more coords. */
    if (fh->n + 1 >= fh->size) {
        fh->size += SOME;
        REALLOCARRAY(fh->coords, fh->size);
        if (fh->coords == NULL)
            pm_error( "out of memory enlarging a fillhandle" );

        ocp = &(fh->coords[fh->n - 1]);
    }

    /* Check for extremum and set the edge number. */
    if (fh->n == 0) {
        /* Start first segment. */
        fh->segstart = fh->n;
        fh->ydir = 0;
        fh->startydir = 0;
    } else {
        int dx, dy;

        dx = x - ocp->x;
        dy = y - ocp->y;
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1) {
            /* Segment break.  Close off old one. */
            if (fh->startydir != 0 && fh->ydir != 0)
                if (fh->startydir == fh->ydir) {
                    /* Oops, first edge and last edge are the same.
                       Renumber the first edge in the old segment.
                    */
                    const coord * const fcpLast= &(fh->coords[fh->n -1]); 
                    coord * fcp;

                    int oldedge;

                    fcp = &(fh->coords[fh->segstart]);
                    oldedge = fcp->edge;
                    for (; fcp <= fcpLast && fcp->edge == oldedge ; ++fcp)
                        fcp->edge = ocp->edge;
                }
            /* And start new segment. */
            ++fh->curedge;
            fh->segstart = fh->n;
            fh->ydir = 0;
            fh->startydir = 0;
        } else {
            /* Segment continues. */
            if (dy != 0) {
                if (fh->ydir != 0 && fh->ydir != dy) {
                    /* Direction changed.  Insert a fake coord, old
                       position but new edge number.
                    */
                    ++fh->curedge;
                    cp = &fh->coords[fh->n];
                    cp->x = ocp->x;
                    cp->y = ocp->y;
                    cp->edge = fh->curedge;
                    ++fh->n;
                }
                fh->ydir = dy;
                if (fh->startydir == 0)
                    fh->startydir = dy;
            }
        }
    }

    /* Save this coord. */
    cp = &fh->coords[fh->n];
    cp->x = x;
    cp->y = y;
    cp->edge = fh->curedge;
    ++fh->n;
}




#ifndef LITERAL_FN_DEF_MATCH
static qsort_comparison_fn yx_compare;
#endif

static int
yx_compare(const void * const c1Arg,
           const void * const c2Arg) {

    const coord * const c1P = c1Arg;
    const coord * const c2P = c2Arg;

    int retval;
    
    if (c1P->y > c2P->y)
        retval = 1;
    else if (c1P->y < c2P->y)
        retval = -1;
    else if (c1P->x > c2P->x)
        retval = 1;
    else if (c1P->x < c2P->x)
        retval = -1;
    else
        retval = 0;

    return retval;
}



void
ppmd_fill(pixel **         const pixels, 
          int              const cols, 
          int              const rows, 
          pixval           const maxval, 
          struct fillobj * const fh,
          ppmd_drawproc          drawProc,
          const void *     const clientdata) {

    int pedge;
    int i, edge, lx, rx, py;
    coord * cp;
    bool eq;
    bool leftside;

    /* Close off final segment. */
    if (fh->n > 0 && fh->startydir != 0 && fh->ydir != 0) {
        if (fh->startydir == fh->ydir) {
            /* Oops, first edge and last edge are the same. */
            coord * fcp;
            const coord * const fcpLast = & (fh->coords[fh->n - 1]);
            int lastedge, oldedge;

            lastedge = fh->coords[fh->n - 1].edge;
            fcp = &(fh->coords[fh->segstart]);
            oldedge = fcp->edge;
            for ( ; fcp<=fcpLast && fcp->edge == oldedge; ++fcp )
                fcp->edge = lastedge;
        }
    }
    /* Restore clipping now. */
    ppmd_setlineclip(oldclip);

    /* Sort the coords by Y, secondarily by X. */
    qsort((char*) fh->coords, fh->n, sizeof(coord), yx_compare);

    /* Find equal coords with different edge numbers, and swap if necessary. */
    edge = -1;
    for (i = 0; i < fh->n; ++i) {
        cp = &fh->coords[i];
        if (i > 1 && eq && cp->edge != edge && cp->edge == pedge) {
            /* Swap .-1 and .-2. */
            coord t;

            t = fh->coords[i-1];
            fh->coords[i-1] = fh->coords[i-2];
            fh->coords[i-2] = t;
        }
        if (i > 0) {
            if (cp->x == lx && cp->y == py) {
                eq = TRUE;
                if (cp->edge != edge && cp->edge == pedge) {
                    /* Swap . and .-1. */
                    coord t;

                    t = *cp;
                    *cp = fh->coords[i-1];
                    fh->coords[i-1] = t;
                }
            } else
                eq = FALSE;
        }
        lx    = cp->x;
        py    = cp->y;
        pedge = edge;
        edge  = cp->edge;
    }

    /* Ok, now run through the coords filling spans. */
    for (i = 0; i < fh->n; ++i) {
        cp = &fh->coords[i];
        if (i == 0) {
            lx       = rx = cp->x;
            py       = cp->y;
            edge     = cp->edge;
            leftside = TRUE;
        } else {
            if (cp->y != py) {
                /* Row changed.  Emit old span and start a new one. */
                ppmd_filledrectangle(
                    pixels, cols, rows, maxval, lx, py, rx - lx + 1, 1,
                    drawProc, clientdata);
                lx       = rx = cp->x;
                py       = cp->y;
                edge     = cp->edge;
                leftside = TRUE;
            } else {
                if (cp->edge == edge) {
                    /* Continuation of side. */
                    rx = cp->x;
                } else {
                    /* Edge changed.  Is it a span? */
                    if (leftside) {
                        rx       = cp->x;
                        leftside = FALSE;
                    } else {
                        /* Got a span to fill. */
                        ppmd_filledrectangle(
                            pixels, cols, rows, maxval, lx, py, rx - lx + 1,
                            1, drawProc, clientdata);
                        lx       = rx = cp->x;
                        leftside = TRUE;
                    }
                    edge = cp->edge;
                }
            }
        }
    }
}



/* Table used to look up sine of angles from 0 through 90 degrees.
   The value returned is the sine * 65536.  Symmetry is used to
   obtain sine and cosine for arbitrary angles using this table. */

static long sintab[] = {
    0, 1143, 2287, 3429, 4571, 5711, 6850, 7986, 9120, 10252, 11380,
    12504, 13625, 14742, 15854, 16961, 18064, 19160, 20251, 21336,
    22414, 23486, 24550, 25606, 26655, 27696, 28729, 29752, 30767,
    31772, 32768, 33753, 34728, 35693, 36647, 37589, 38521, 39440,
    40347, 41243, 42125, 42995, 43852, 44695, 45525, 46340, 47142,
    47929, 48702, 49460, 50203, 50931, 51643, 52339, 53019, 53683,
    54331, 54963, 55577, 56175, 56755, 57319, 57864, 58393, 58903,
    59395, 59870, 60326, 60763, 61183, 61583, 61965, 62328, 62672,
    62997, 63302, 63589, 63856, 64103, 64331, 64540, 64729, 64898,
    65047, 65176, 65286, 65376, 65446, 65496, 65526, 65536
};

static int extleft, exttop, extright, extbottom;  /* To accumulate extents */

/* LINTLIBRARY */

/*  ISIN  --  Return sine of an angle in integral degrees.  The
          value returned is 65536 times the sine.  */

#if __STDC__
static long isin(int deg)
#else
    static long isin(deg)
    int deg;
#endif
{
    /* Domain reduce to 0 to 360 degrees. */

    if (deg < 0) {
        deg = (360 - ((- deg) % 360)) % 360;
    } else if (deg >= 360) {
        deg = deg % 360;
    }

    /* Now look up from table according to quadrant. */

    if (deg <= 90) {
        return sintab[deg];
    } else if (deg <= 180) {
        return sintab[180 - deg];
    } else if (deg <= 270) {
        return -sintab[deg - 180];
    }
    return -sintab[360 - deg];
}

/*  ICOS  --  Return cosine of an angle in integral degrees.  The
          value returned is 65536 times the cosine.  */

#if __STDC__
static long icos(int deg)
#else
    static long icos(deg)
    int deg;
#endif
{
    return isin(deg + 90);
}  

#define SCHAR(x) (u = (x), (((u) & 0x80) ? ((u) | (-1 ^ 0xFF)) : (u)))

#define Scalef 21       /* Font design size */
#define Descend 9       /* Descender offset */



static void
drawGlyph(const struct ppmd_glyph * const glyphP,
          int *                     const xP,
          int                       const y,
          pixel **                  const pixels,
          unsigned int              const cols,
          unsigned int              const rows,
          pixval                    const maxval,
          int                       const height,
          int                       const xpos,
          int                       const ypos,
          long                      const rotcos,
          long                      const rotsin,
          ppmd_drawproc                   drawProc,
          const void *              const clientdata
          ) {
/*----------------------------------------------------------------------------
   *xP is the column number of the left side of the glyph in the
   output upon entry, and we update it to the left side of the next
   glyph.

   'y' is the row number of either the top or the bottom of the glyph
   (I can't tell which right now) in the output.
-----------------------------------------------------------------------------*/
    struct penpos penPos;
    unsigned int commandNum;
    int x;
    int u;  /* Used by the SCHAR macro */

    x = *xP;  /* initial value */

    x -= SCHAR(glyphP->header.skipBefore);

    penPos.x = x;
    penPos.y = y;

    for (commandNum = 0;
         commandNum < glyphP->header.commandCount;
         ++commandNum) {

        const struct ppmd_glyphCommand * const commandP =
            &glyphP->commandList[commandNum];

        switch (commandP->verb) {
        case CMD_NOOP:
            break;
        case CMD_DRAWLINE:
        {
            int const nx = x + SCHAR(commandP->x);
            int const ny = y + SCHAR(commandP->y);

            int mx1, my1, mx2, my2;
            int tx1, ty1, tx2, ty2;

            /* Note that up until this  moment  we've  been
               working  in  an  arbitrary model co-ordinate
               system with  fixed  size  and  no  rotation.
               Before  drawing  the  stroke,  transform  to
               viewing co-ordinates to  honour  the  height
               and angle specifications.
            */

            mx1 = (penPos.x * height) / Scalef;
            my1 = ((penPos.y - Descend) * height) / Scalef;
            mx2 = (nx * height) / Scalef;
            my2 = ((ny - Descend) * height) / Scalef;
            tx1 = xpos + (mx1 * rotcos - my1 * rotsin) / 65536;
            ty1 = ypos + (mx1 * rotsin + my1 * rotcos) / 65536;
            tx2 = xpos + (mx2 * rotcos - my2 * rotsin) / 65536;
            ty2 = ypos + (mx2 * rotsin + my2 * rotcos) / 65536;

            ppmd_validateCoords(tx1, ty1);
            ppmd_validateCoords(tx2, ty2);
            
            ppmd_line(pixels, cols, rows, maxval, tx1, ty1, tx2, ty2,
                      drawProc, clientdata);

            penPos.x = nx;
            penPos.y = ny;
        }
            break;
        case CMD_MOVEPEN:
            penPos.x = x + SCHAR(commandP->x);
            penPos.y = y + SCHAR(commandP->y);
            break;
        }
    }
    x += glyphP->header.skipAfter; 

    *xP = x;
}


/* PPMD_TEXT  --  Draw the zero-terminated  string  s,  with  its  baseline
          starting  at  point  (x, y), inclined by angle degrees to
          the X axis, with letters height pixels  high  (descenders
          will  extend below the baseline).  The supplied drawproc
          and cliendata are passed to ppmd_line which performs  the
          actual drawing. */

void
ppmd_text(pixel**       const pixels, 
          int           const cols, 
          int           const rows, 
          pixval        const maxval, 
          int           const xpos, 
          int           const ypos, 
          int           const height, 
          int           const angle, 
          const char *  const sArg, 
          ppmd_drawproc       drawProc,
          const void *  const clientdata) {

    const struct ppmd_font * const fontP = ppmd_get_font();
    long rotsin, rotcos;
    int x, y;
    const char * s;

    ppmd_validateCoords(xpos, ypos);

    x = y = 0;
    rotsin = isin(-angle);
    rotcos = icos(-angle);

    s = sArg;
    while (*s) {
        unsigned char const ch = *s++;

        if (ch >= fontP->header.firstCodePoint &&
            ch < fontP->header.firstCodePoint + fontP->header.characterCount) {

            const struct ppmd_glyph * const glyphP =
                &fontP->glyphTable[ch - fontP->header.firstCodePoint];

            ppmd_validateCoords(x, y); 

            drawGlyph(glyphP, &x, y, pixels, cols, rows, maxval,
                      height, xpos, ypos, rotcos, rotsin,
                      drawProc, clientdata);
        } else if (ch == '\n') {
            /* Move to the left edge of the next line down */
            y += Scalef + Descend;
            x = 0;
        }
    }
}

/* EXTENTS_DRAWPROC  --  Drawproc which just accumulates the extents
             rectangle bounding the text. */

static void 
extents_drawproc (pixel**      const pixels, 
                  int          const cols, 
                  int          const rows,
                  pixval       const maxval, 
                  int          const x, 
                  int          const y, 
                  const void * const clientdata)
{
    extleft = MIN(extleft, x);
    exttop = MIN(exttop, y);
    extright = MAX(extright, x);
    extbottom = MAX(extbottom, y);
}


/* PPMD_TEXT_BOX  --  Calculate  extents  rectangle for a given piece of
   text.  For most  applications  where  extents  are
   needed,   angle  should  be  zero  to  obtain  the
   unrotated extents.  If you need  the  extents  box
   for post-rotation text, however, you can set angle
   nonzero and it will be calculated correctly.
*/

void
ppmd_text_box(int const height, 
              int const angle, 
              const char * const s, 
              int * const left, 
              int * const top, 
              int * const right, 
              int * const bottom)
{
    extleft = 32767;
    exttop = 32767;
    extright = -32767;
    extbottom = -32767;
    ppmd_text(NULL, 32767, 32767, 255, 1000, 1000, height, angle, s, 
              extents_drawproc, NULL);
    *left = extleft - 1000; 
    *top = exttop - 1000;
    *right = extright - 1000;
    *bottom = extbottom - 1000;
}
