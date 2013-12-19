/*                         P L O T 3 . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup plot */
/** @{ */
/** @file plot3.h
 *
 *  The basic UNIX-plot routines.  The calling sequence is the same as
 *  the original Bell Labs routines, with the exception of the pl_
 *  prefix on the name.
 *
 */

#ifndef PLOT3_H
#define PLOT3_H

#include "common.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"

__BEGIN_DECLS

#define	pl_mat_idn( _mat )		MAT_IDN( _mat )
#define pl_mat_zero( _mat )		MAT_ZERO( _mat )
#define pl_mat_copy( _mat1, _mat2 )	MAT_COPY( _mat1, _mat2 )

#define PL_OUTPUT_MODE_BINARY 0
#define PL_OUTPUT_MODE_TEXT 1


BN_EXPORT extern int pl_getOutputMode();
BN_EXPORT extern void pl_setOutputMode(int mode);
BN_EXPORT extern void pl_point(FILE *plotfp,
		     int x,
		     int y);
BN_EXPORT extern void pl_line(FILE *plotfp,
		     int fx,
		     int fy,
		     int tx,
		     int ty);
BN_EXPORT extern void pl_linmod(FILE *plotfp,
		     const char *s);
BN_EXPORT extern void pl_move(FILE *plotfp,
		     int x,
		     int y);
BN_EXPORT extern void pl_cont(FILE *plotfp,
		     int x,
		     int y);
BN_EXPORT extern void pl_label(FILE *plotfp,
		     const char *s);
BN_EXPORT extern void pl_space(FILE *plotfp,
		     int x_1,
		     int y_1,
		     int x_2,
		     int y_2);
BN_EXPORT extern void pl_erase(FILE *plotfp);
BN_EXPORT extern void pl_circle(FILE *plotfp,
		     int x,
		     int y,
		     int r);
BN_EXPORT extern void pl_arc(FILE *plotfp,
		     int xc,
		     int yc,
		     int x_1,
		     int y_1,
		     int x_2,
		     int y_2);
BN_EXPORT extern void pl_box(FILE *plotfp,
		     int x_1,
		     int y_1,
		     int x_2,
		     int y_2);

/*
 * BRL extensions to the UNIX-plot file format.
 */
BN_EXPORT extern void pl_color(FILE *plotfp,
		     int r,
		     int g,
		     int b);
BN_EXPORT extern void pl_flush(FILE *plotfp);
BN_EXPORT extern void pl_3space(FILE *plotfp,
		     int x_1,
		     int y_1,
		     int z_1,
		     int x_2,
		     int y_2,
		     int z_2);
BN_EXPORT extern void pl_3point(FILE *plotfp,
		     int x,
		     int y,
		     int z);
BN_EXPORT extern void pl_3move(FILE *plotfp,
		     int x,
		     int y,
		     int z);
BN_EXPORT extern void pl_3cont(FILE *plotfp,
		     int x,
		     int y,
		     int z);
BN_EXPORT extern void pl_3line(FILE *plotfp,
		     int x_1,
		     int y_1,
		     int z_1,
		     int x_2,
		     int y_2,
		     int z_2);
BN_EXPORT extern void pl_3box(FILE *plotfp,
		     int x_1,
		     int y_1,
		     int z_1,
		     int x_2,
		     int y_2,
		     int z_2);

/* Double floating point versions */
BN_EXPORT extern void pd_point(FILE *plotfp,
		     double x,
		     double y);
BN_EXPORT extern void pd_line(FILE *plotfp,
		     double fx,
		     double fy,
		     double tx,
		     double ty);
BN_EXPORT extern void pd_move(FILE *plotfp,
		     double x,
		     double y);
BN_EXPORT extern void pd_cont(FILE *plotfp,
		     double x,
		     double y);
BN_EXPORT extern void pd_space(FILE *plotfp,
		     double x_1,
		     double y_1,
		     double x_2,
		     double y_2);
BN_EXPORT extern void pd_circle(FILE *plotfp,
		     double x,
		     double y,
		     double r);
BN_EXPORT extern void pd_arc(FILE *plotfp,
		     double xc,
		     double yc,
		     double x_1,
		     double y_1,
		     double x_2,
		     double y_2);
BN_EXPORT extern void pd_box(FILE *plotfp,
		     double x_1,
		     double y_1,
		     double x_2,
		     double y_2);

/* Double 3-D both in vector and enumerated versions */
#ifdef VMATH_H
BN_EXPORT extern void pdv_3space(FILE *plotfp,
		     const vect_t min,
		     const vect_t max);
BN_EXPORT extern void pdv_3point(FILE *plotfp,
		     const vect_t pt);
BN_EXPORT extern void pdv_3move(FILE *plotfp,
		     const vect_t pt);
BN_EXPORT extern void pdv_3cont(FILE *plotfp,
		     const vect_t pt);
BN_EXPORT extern void pdv_3line(FILE *plotfp,
		     const vect_t a,
		     const vect_t b);
BN_EXPORT extern void pdv_3box(FILE *plotfp,
		     const vect_t a,
		     const vect_t b);
#endif /* VMATH_H */
BN_EXPORT extern void pd_3space(FILE *plotfp,
		     double x_1,
		     double y_1,
		     double z_1,
		     double x_2,
		     double y_2,
		     double z_2);
BN_EXPORT extern void pd_3point(FILE *plotfp,
		     double x,
		     double y,
		     double z);
BN_EXPORT extern void pd_3move(FILE *plotfp,
		     double x,
		     double y,
		     double z);
BN_EXPORT extern void pd_3cont(FILE *plotfp,
		     double x,
		     double y,
		     double z);
BN_EXPORT extern void pd_3line(FILE *plotfp,
		     double x_1,
		     double y_1,
		     double z_1,
		     double x_2,
		     double y_2,
		     double z_2);
BN_EXPORT extern void pd_3box(FILE *plotfp,
		     double x_1,
		     double y_1,
		     double z_1,
		     double x_2,
		     double y_2,
		     double z_2);
BN_EXPORT extern void pdv_3ray(FILE *fp,
		     const point_t pt,
		     const vect_t dir,
		     double t);

/*
 *  The following routines are taken from the BRL TIG-PACK
 *  (Terminal Independent Plotting Package).
 *  These routines create plots by using the pl_() and pd_() routines
 *  declared above.
 */

#define PL_FORTRAN(lc, uc)	BU_FORTRAN(lc, uc)

/**
 * Take a set of x, y coordinates, and plot them as a polyline, i.e.,
 * connect them with line segments.  For markers, use tp_mlist(),
 * below.  This "C" interface expects arrays of INTs.
 */
BN_EXPORT extern void tp_i2list(FILE *fp,
		     int *x,
		     int *y,
		     int npoints);

/**
 * Take a set of x, y coordinates, and plot them as a polyline, i.e.,
 * connect them with line segments.  For markers, use tp_mlist(),
 * below.  This "C" interface expects arrays of DOUBLES.
 *
 * NOTE: tp_2list() and tp_3list() are good candidates to become
 * intrinsic parts of plot3.c, for efficiency reasons.
 *
 * Originally written in August 04, 1978
 */
BN_EXPORT extern void tp_2list(FILE *fp,
		     double *x,
		     double *y,
		     int npoints);
BN_EXPORT extern void BU_FORTRAN(f2list, F2LIST)(FILE **fpp,
		     float *x,
		     float *y,
		     int *n);

/**
 * NOTE: tp_2list() and tp_3list() are good candidates to become
 * intrinsic parts of plot3.c, for efficiency reasons.
 *
 * Originally written in August 04, 1978
 */
BN_EXPORT extern void tp_3list(FILE *fp,
		     double *x,
		     double *y,
		     double *z,
		     int npoints);
BN_EXPORT extern void BU_FORTRAN(f3list, F3LIST)(FILE **fpp,
		     float *x,
		     float *y,
		     float *z,
		     int *n);

/**
 * Take a set of x, y co-ordinates and plots them, with a combination
 * of connecting lines and/or place markers.  It is important to note
 * that the arrays are arrays of doubles, and express UNIX-plot
 * coordinates in the current pl_space().
 *
 * tp_scale(TIG) may be called first to optionally re-scale the data.
 *
 * The 'mark' character to be used for marking points off can be any
 * printing ASCII character, or 001 to 005 for the special marker
 * characters.
 *
 * In addition, the value of the 'flag' variable determines the type
 * of line to be drawn, as follows:
 *
 *@li	0	Draw nothing (rather silly)
 *@li	1	Marks only, no connecting lines.  Suggested interval=1.
 *@li	2	Draw connecting lines only.
 *@li	3	Draw line and marks
 */
BN_EXPORT extern void tp_2mlist(FILE *fp,
		     double *x,
		     double *y,
		     int npoints,
		     int flag,
		     int mark,
		     int interval,
		     double size);

/**
 * This FORTRAN interface expects arrays of REALs (single precision).
 */
BN_EXPORT extern void BU_FORTRAN(f2mlst, F2MLST)(FILE **fp,
		     float *x,
		     float *y,
		     int *np,
		     int *flag,
		     int *mark,
		     int *interval,
		     float *size);
BN_EXPORT extern void tp_2marker(FILE *fp,
		     int c,
		     double x,
		     double y,
		     double scale);
BN_EXPORT extern void BU_FORTRAN(f2mark, F2MARK)(FILE **fp,
		     int *c,
		     float *x,
		     float *y,
		     float *scale);
BN_EXPORT extern void tp_3marker(FILE *fp,
		     int c,
		     double x,
		     double y,
		     double z,
		     double scale);
BN_EXPORT extern void BU_FORTRAN(f3mark, F3MARK)(FILE **fp,
		     int *c,
		     float *x,
		     float *y,
		     float *z,
		     float *scale);
BN_EXPORT extern void tp_2number(FILE *fp,
		     double input,
		     int x,
		     int y,
		     int cscale,
		     double theta,
		     int digits);
BN_EXPORT extern void BU_FORTRAN(f2numb, F2NUMB)(FILE **fp,
		     float *input,
		     int *x,
		     int *y,
		     float *cscale,
		     float *theta,
		     int *digits);
BN_EXPORT extern void tp_scale(int idata[],
		     int elements,
		     int mode,
		     int length,
		     int odata[],
		     double *min,
		     double *dx);

BN_EXPORT extern void BU_FORTRAN(fscale, FSCALE)(int idata[],
		     int *elements,
		     char *mode,
		     int *length,
		     int odata[],
		     double *min,
		     double *dx);
BN_EXPORT extern void tp_2symbol(FILE *fp,
		     char *string,
		     double x,
		     double y,
		     double scale,
		     double theta);
BN_EXPORT extern void BU_FORTRAN(f2symb, F2SYMB)(FILE **fp,
		     char *string,
		     float *x,
		     float *y,
		     float *scale,
		     float *theta);
BN_EXPORT extern void tp_plot(FILE *fp,
		     int xp,
		     int yp,
		     int xl,
		     int yl,
		     char xtitle[],
		     char ytitle[],
		     float x[],
		     float y[],
		     int n,
		     double cscale);
BN_EXPORT extern void BU_FORTRAN(fplot, FPLOT)(FILE **fp,
		     int *xp,
		     int *yp,
		     int *xl,
		     int *yl,
		     char *xtitle,
		     char *ytitle,
		     float *x,
		     float *y,
		     int *n,
		     float *cscale);
BN_EXPORT extern void tp_ftoa(float x, char *s);
BN_EXPORT extern void tp_fixsc(float *x,
		     int npts,
		     float size,
		     float *xs,
		     float *xmin,
		     float *xmax,
		     float *dx);
BN_EXPORT extern void tp_sep(float x,
		     float *coef,
		     int *ex);
BN_EXPORT extern double tp_ipow(double x,
		     int n);


/**
 * This routine is used to generate an axis for a graph.  It draws an
 * axis with a linear scale, places tic marks every inch, labels the
 * tics, and uses the supplied title for the axis.
 *
 * The strategy behind this routine is to split the axis into
 * SEGMENTS, which run from one tick to the next.  The origin of the
 * first segment (x, y), the origin of the bottom of the first tick
 * (xbott, ybott), and the origin of the first tick label (xnum, ynum)
 * are computed along with the delta x and delta y (xincr, yincr)
 * which describes the interval to the start of the next tick.
 *
 * Originally written on August 01, 1978
 */
BN_EXPORT extern void tp_3axis(FILE *fp,
		     char *string,
		     point_t origin,
		     mat_t rot,
		     double length,
		     int ccw,
		     int ndigits,
		     double label_start,
		     double label_incr,
		     double tick_separation,
		     double char_width);
BN_EXPORT extern void BU_FORTRAN(f3axis, F3AXIS)(FILE **fp,
		     char *string,
		     float *x,
		     float *y,
		     float *z,
		     float *length,
		     float *theta,
		     int *ccw,
		     int *ndigits,
		     float *label_start,
		     float *label_incr,
		     float *tick_separation,
		     float *char_width);
BN_EXPORT extern void tp_3symbol(FILE *fp,
		     char *string,
		     point_t origin,
		     mat_t rot,
		     double scale);
BN_EXPORT extern void tp_3vector(FILE *plotfp,
		     point_t from,
		     point_t to,
		     double fromheadfract,
		     double toheadfract);
BN_EXPORT extern void BU_FORTRAN(f3vect, F3VECT)(FILE **fp,
		     float *fx,
		     float *fy,
		     float *fz,
		     float *tx,
		     float *ty,
		     float *tz,
		     float *fl,
		     float *tl);

__END_DECLS

#endif /* PLOT3_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
