/*                         P L O T 3 . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @addtogroup bv_plot
 *
 * @brief A public-domain UNIX plot library, for 2-D and 3-D plotting in 16-bit
 * VAX signed integer spaces, or 64-bit IEEE floating point.
 *
 * These routines generate "UNIX plot" output (with the addition of 3-D
 * commands).  They behave almost exactly like the regular libplot routines,
 * except:
 *
 * -# These all take a stdio file pointer, and can thus be used to create
 *    multiple plot files simultaneously.
 * -# There are 3-D versions of most commands.
 * -# There are IEEE floating point versions of the commands.
 * -# The names have been changed.
 *
 * The 3-D extensions are those of Doug Gwyn, from his System V extensions.
 * Plot3 also incorporates routines from the BRL TIG-PACK (Terminal
 * Independent Plotting Package), originally written in 1978 and 1979 for
 * generating polylines, markers, axes, simple XY plots, vectors, and plotted
 * symbols using the plot3 primitives.
 *
 * The built-in vector font support descends from the Terminal Independent
 * Graphics Display Package by Mike Muuss, July 31, 1978.  The vector font
 * table was provided courtesy of Dr. Bruce Henrikson and Dr. Stephen Wolff,
 * U.S. Army Ballistic Research Laboratory, Summer 1978, based on work for a
 * remote Houston Instruments pen plotter package on the GE Tymeshare system.
 * The basic font resides in a 10x10 unit square; callers scale and transform
 * those stroke lists into plot coordinates.
 *
 * These are the ascii command letters allocated to various actions.  Care has
 * been taken to consistently match lowercase and uppercase letters.
 *
 * @code
 2d	3d	2df	3df
 space		s	S	w	W
 move		m	M	o	O
 cont		n	N	q	Q
 point		p	P	x	X
 line		l	L	v	V
 circle		c		i
 arc		a		r
 linmod		f
 label		t
 erase		e
 color		C
 flush		F

 bd gh jk  uyz
 ABDEGHIJKRTUYZ

 @endcode
 *
 * The calling sequence is the same as the original Bell Labs routines, with
 * the exception of the pl_ prefix on the name.
 *
 * NOTE:  from libbv's perspective, plot3.h is a stand-alone header that does
 * not use any other library functionality.  To use this header without libbv,
 * define PLOT3_IMPLEMENTATION before including the plot3.h header.
 * The PLOT_PREFIX_STR mechanism below also can be used to allow
 * PLOT3_IMPLEMENTATION to be included multiple times without conflict.  (TODO
 * - check if we can use inline instead of static to achieve a similar
 * result...)
 *
 * Of interest:  the Plan 9 sources (recently MIT licensed) appear to be
 * related to the original code that would have formed the conceptual basis for
 * these routines:
 *
 * https://plan9.io/sources/plan9/sys/src/cmd/plot/libplot/
 *
 * Don't know if there would be any improvements that could be retrofitted onto
 * this version, but might be worth looking.  In particular, curious if the
 * spline routine might be useful...
 */
/** @{ */
/** @file plot3.h */

#ifndef PLOT3_H
#define PLOT3_H

#include "common.h"

#include "vmath.h"
#include "bu/color.h"
#include "bu/file.h"

#ifndef PLOT3_EXPORT
#  if defined(PLOT3_DLL_EXPORTS) && defined(PLOT3_DLL_IMPORTS)
#    error "Only PLOT3_DLL_EXPORTS or PLOT3_DLL_IMPORTS can be defined, not both."
#  elif defined(PLOT3_DLL_EXPORTS)
#    define PLOT3_EXPORT COMPILER_DLLEXPORT
#  elif defined(PLOT3_DLL_IMPORTS)
#    define PLOT3_EXPORT COMPILER_DLLIMPORT
#  else
#    define PLOT3_EXPORT
#  endif
#endif

__BEGIN_DECLS
#define PL_OUTPUT_MODE_BINARY 0
#define PL_OUTPUT_MODE_TEXT 1

#if !defined(PLOT_PREFIX_STR)
#  define PLOT_PREFIX_STR plot3_
#endif
#define PL_CONCAT2(a, b) a ## b
#define PL_CONCAT(a, b) PL_CONCAT2(a,b)
#define PL_ADD_PREFIX(b) PL_CONCAT(PLOT_PREFIX_STR,b)

/* All linked symbols */
#define pd_3box          PL_ADD_PREFIX(pd_3box)
#define pd_3cont         PL_ADD_PREFIX(pd_3cont)
#define pd_3line         PL_ADD_PREFIX(pd_3line)
#define pd_3move         PL_ADD_PREFIX(pd_3move)
#define pd_3point        PL_ADD_PREFIX(pd_3point)
#define pd_3space        PL_ADD_PREFIX(pd_3space)
#define pd_arc           PL_ADD_PREFIX(pd_arc)
#define pd_box           PL_ADD_PREFIX(pd_box)
#define pd_circle        PL_ADD_PREFIX(pd_circle)
#define pd_cont          PL_ADD_PREFIX(pd_cont)
#define pd_line          PL_ADD_PREFIX(pd_line)
#define pd_move          PL_ADD_PREFIX(pd_move)
#define pd_point         PL_ADD_PREFIX(pd_point)
#define pd_space         PL_ADD_PREFIX(pd_space)
#define pdv_3box         PL_ADD_PREFIX(pdv_3box)
#define pdv_3cont        PL_ADD_PREFIX(pdv_3cont)
#define pdv_3line        PL_ADD_PREFIX(pdv_3line)
#define pdv_3move        PL_ADD_PREFIX(pdv_3move)
#define pdv_3point       PL_ADD_PREFIX(pdv_3point)
#define pdv_3ray         PL_ADD_PREFIX(pdv_3ray)
#define pdv_3space       PL_ADD_PREFIX(pdv_3space)
#define pl_3box          PL_ADD_PREFIX(pl_3box)
#define pl_3cont         PL_ADD_PREFIX(pl_3cont)
#define pl_3line         PL_ADD_PREFIX(pl_3line)
#define pl_3move         PL_ADD_PREFIX(pl_3move)
#define pl_3point        PL_ADD_PREFIX(pl_3point)
#define pl_3space        PL_ADD_PREFIX(pl_3space)
#define pl_arc           PL_ADD_PREFIX(pl_arc)
#define pl_box           PL_ADD_PREFIX(pl_box)
#define pl_circle        PL_ADD_PREFIX(pl_circle)
#define pl_color         PL_ADD_PREFIX(pl_color)
#define pl_color_buc     PL_ADD_PREFIX(pl_color_buc)
#define pl_cont          PL_ADD_PREFIX(pl_cont)
#define pl_erase         PL_ADD_PREFIX(pl_erase)
#define pl_flush         PL_ADD_PREFIX(pl_flush)
#define pl_getOutputMode PL_ADD_PREFIX(pl_getOutputMode)
#define pl_label         PL_ADD_PREFIX(pl_label)
#define pl_line          PL_ADD_PREFIX(pl_line)
#define pl_linmod        PL_ADD_PREFIX(pl_linmod)
#define pl_move          PL_ADD_PREFIX(pl_move)
#define pl_point         PL_ADD_PREFIX(pl_point)
#define pl_setOutputMode PL_ADD_PREFIX(pl_setOutputMode)
#define pl_space         PL_ADD_PREFIX(pl_space)
#define pl_list          PL_ADD_PREFIX(pl_list)
#define plot3_invalid    PL_ADD_PREFIX(plot3_invalid)
#define plot3_data_scale PL_ADD_PREFIX(plot3_data_scale)
#define plot3_float_split PL_ADD_PREFIX(plot3_float_split)
#define plot3_float_to_ascii PL_ADD_PREFIX(plot3_float_to_ascii)
#define plot3_font_getchar PL_ADD_PREFIX(plot3_font_getchar)
#define plot3_ipow       PL_ADD_PREFIX(plot3_ipow)
#define plot3_scale_fit  PL_ADD_PREFIX(plot3_scale_fit)
#define plot3_xy_plot    PL_ADD_PREFIX(plot3_xy_plot)
#define pd_3list         PL_ADD_PREFIX(pd_3list)
#define pd_3marker       PL_ADD_PREFIX(pd_3marker)
#define pd_list          PL_ADD_PREFIX(pd_list)
#define pd_marked_list   PL_ADD_PREFIX(pd_marked_list)
#define pd_marker        PL_ADD_PREFIX(pd_marker)
#define pd_symbol        PL_ADD_PREFIX(pd_symbol)
#define pdv_3axis        PL_ADD_PREFIX(pdv_3axis)
#define pdv_3symbol      PL_ADD_PREFIX(pdv_3symbol)
#define pdv_3vector      PL_ADD_PREFIX(pdv_3vector)

#define PLOT3_FONT_LAST -128
#define PLOT3_FONT_NEGY -127

PLOT3_EXPORT extern int pl_getOutputMode(void);
PLOT3_EXPORT extern void pl_setOutputMode(int mode);
PLOT3_EXPORT extern void pl_point(FILE *plotfp,
			       int x,
			       int y);
PLOT3_EXPORT extern void pl_line(FILE *plotfp,
			      int fx,
			      int fy,
			      int tx,
			      int ty);
PLOT3_EXPORT extern void pl_linmod(FILE *plotfp,
				const char *s);
PLOT3_EXPORT extern void pl_move(FILE *plotfp,
			      int x,
			      int y);
PLOT3_EXPORT extern void pl_cont(FILE *plotfp,
			      int x,
			      int y);
PLOT3_EXPORT extern void pl_label(FILE *plotfp,
			       const char *s);
PLOT3_EXPORT extern void pl_space(FILE *plotfp,
			       int x_1,
			       int y_1,
			       int x_2,
			       int y_2);
PLOT3_EXPORT extern void pl_erase(FILE *plotfp);
PLOT3_EXPORT extern void pl_circle(FILE *plotfp,
				int x,
				int y,
				int r);
PLOT3_EXPORT extern void pl_arc(FILE *plotfp,
			     int xc,
			     int yc,
			     int x_1,
			     int y_1,
			     int x_2,
			     int y_2);
PLOT3_EXPORT extern void pl_box(FILE *plotfp,
			     int x_1,
			     int y_1,
			     int x_2,
			     int y_2);

/*
 * BRL extensions to the UNIX-plot file format.
 */
PLOT3_EXPORT extern void pl_color(FILE *plotfp,
			       int r,
			       int g,
			       int b);
PLOT3_EXPORT extern void pl_color_buc(FILE *plotfp,
			       struct bu_color *c);
PLOT3_EXPORT extern void pl_flush(FILE *plotfp);
PLOT3_EXPORT extern void pl_3space(FILE *plotfp,
				int x_1,
				int y_1,
				int z_1,
				int x_2,
				int y_2,
				int z_2);
PLOT3_EXPORT extern void pl_3point(FILE *plotfp,
				int x,
				int y,
				int z);
PLOT3_EXPORT extern void pl_3move(FILE *plotfp,
			       int x,
			       int y,
			       int z);
PLOT3_EXPORT extern void pl_3cont(FILE *plotfp,
			       int x,
			       int y,
			       int z);
PLOT3_EXPORT extern void pl_3line(FILE *plotfp,
			       int x_1,
			       int y_1,
			       int z_1,
			       int x_2,
			       int y_2,
			       int z_2);
PLOT3_EXPORT extern void pl_3box(FILE *plotfp,
			      int x_1,
			      int y_1,
			      int z_1,
			      int x_2,
			      int y_2,
			      int z_2);

/* Double floating point versions */
PLOT3_EXPORT extern void pd_point(FILE *plotfp,
			       double x,
			       double y);
PLOT3_EXPORT extern void pd_line(FILE *plotfp,
			      double fx,
			      double fy,
			      double tx,
			      double ty);
PLOT3_EXPORT extern void pd_move(FILE *plotfp,
			      double x,
			      double y);
PLOT3_EXPORT extern void pd_cont(FILE *plotfp,
			      double x,
			      double y);
PLOT3_EXPORT extern void pd_space(FILE *plotfp,
			       double x_1,
			       double y_1,
			       double x_2,
			       double y_2);
PLOT3_EXPORT extern void pd_circle(FILE *plotfp,
				double x,
				double y,
				double r);
PLOT3_EXPORT extern void pd_arc(FILE *plotfp,
			     double xc,
			     double yc,
			     double x_1,
			     double y_1,
			     double x_2,
			     double y_2);
PLOT3_EXPORT extern void pd_box(FILE *plotfp,
			     double x_1,
			     double y_1,
			     double x_2,
			     double y_2);

/* Double 3-D both in vector and enumerated versions */
#ifdef VMATH_H
PLOT3_EXPORT extern void pdv_3space(FILE *plotfp,
				 const vect_t min,
				 const vect_t max);
PLOT3_EXPORT extern void pdv_3point(FILE *plotfp,
				 const vect_t pt);
PLOT3_EXPORT extern void pdv_3move(FILE *plotfp,
				const vect_t pt);
PLOT3_EXPORT extern void pdv_3cont(FILE *plotfp,
				const vect_t pt);
PLOT3_EXPORT extern void pdv_3line(FILE *plotfp,
				const vect_t a,
				const vect_t b);
PLOT3_EXPORT extern void pdv_3box(FILE *plotfp,
			       const vect_t a,
			       const vect_t b);
#endif /* VMATH_H */
PLOT3_EXPORT extern void pd_3space(FILE *plotfp,
				double x_1,
				double y_1,
				double z_1,
				double x_2,
				double y_2,
				double z_2);
PLOT3_EXPORT extern void pd_3point(FILE *plotfp,
				double x,
				double y,
				double z);
PLOT3_EXPORT extern void pd_3move(FILE *plotfp,
			       double x,
			       double y,
			       double z);
PLOT3_EXPORT extern void pd_3cont(FILE *plotfp,
			       double x,
			       double y,
			       double z);
PLOT3_EXPORT extern void pd_3line(FILE *plotfp,
			       double x_1,
			       double y_1,
			       double z_1,
			       double x_2,
			       double y_2,
			       double z_2);
PLOT3_EXPORT extern void pd_3box(FILE *plotfp,
			      double x_1,
			      double y_1,
			      double z_1,
			      double x_2,
			      double y_2,
			      double z_2);
PLOT3_EXPORT extern void pdv_3ray(FILE *fp,
			       const point_t pt,
			       const vect_t dir,
			       double t);

PLOT3_EXPORT extern void pl_list(FILE *fp,
			      int *x,
			      int *y,
			      int npoints);
PLOT3_EXPORT extern void pd_list(FILE *fp,
			      double *x,
			      double *y,
			      int npoints);
PLOT3_EXPORT extern void pd_3list(FILE *fp,
			       double *x,
			       double *y,
			       double *z,
			       int npoints);
PLOT3_EXPORT extern void pd_marked_list(FILE *fp,
				     double *x,
				     double *y,
				     int npoints,
				     int flag,
				     int mark,
				     int interval,
				     double size);
PLOT3_EXPORT extern void pd_marker(FILE *fp,
				int c,
				double x,
				double y,
				double scale);
PLOT3_EXPORT extern void pd_3marker(FILE *fp,
				 int c,
				 double x,
				 double y,
				 double z,
				 double scale);
PLOT3_EXPORT extern void plot3_data_scale(int idata[],
				       int elements,
				       int mode,
				       int length,
				       int odata[],
				       double *min,
				       double *dx);
PLOT3_EXPORT extern void pd_symbol(FILE *fp,
				char *string,
				double x,
				double y,
				double scale,
				double theta);
PLOT3_EXPORT extern void plot3_xy_plot(FILE *fp,
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
PLOT3_EXPORT extern void plot3_float_to_ascii(float x, char *s);
PLOT3_EXPORT extern void plot3_scale_fit(float *x,
				     int npts,
				     float size,
				     float *xs,
				     float *xmin,
				     float *xmax,
				     float *dx);
PLOT3_EXPORT extern void plot3_float_split(float x,
				       float *coef,
				       int *ex);
PLOT3_EXPORT extern double plot3_ipow(double x,
				  int n);
PLOT3_EXPORT extern void pdv_3axis(FILE *fp,
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
PLOT3_EXPORT extern void pdv_3symbol(FILE *fp,
				  char *string,
				  point_t origin,
				  mat_t rot,
				  double scale);
PLOT3_EXPORT extern void pdv_3vector(FILE *plotfp,
				  point_t from,
				  point_t to,
				  double fromheadfract,
				  double toheadfract);
PLOT3_EXPORT extern int *plot3_font_getchar(const unsigned char *c);

PLOT3_EXPORT extern int plot3_invalid(FILE *fp, int mode);

__END_DECLS

#if defined(PLOT3_IMPLEMENTATION)
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bu/cv.h"

static int pl_outputMode = PL_OUTPUT_MODE_BINARY;

/* For the sake of efficiency, we trust putc() to write only one byte */
#define putsi(a)	putc(a, plotfp); putc((a>>8), plotfp)

/* Making a common pd_3 to be used in pd_3cont and pd_3move */
static void
pd_3(FILE *plotfp, double x, double y, double z, char c)
{
    size_t ret;
    double in[3];
    unsigned char out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	in[2] = z;
	bu_cv_htond(&out[1], (unsigned char *)in, 3);

	out[0] = c;
	ret = fwrite(out, 1, 3*8+1, plotfp);
	if (ret != 3*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "%c %g %g %g\n", c, x, y, z);
    }
}

/* Making a common pdv_3 to be used in pdv_3cont and pdv_3move */
static void
pdv_3(FILE *plotfp, const fastf_t *pt, char c)
{
    size_t ret;
    unsigned char out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	bu_cv_htond(&out[1], (unsigned char *)pt, 3);

	out[0] = c;
	ret = fwrite(out, 1, 3*8+1, plotfp);
	if (ret != 3*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "%c %g %g %g\n", c, V3ARGS(pt));
    }
}

/* Making a common pd to be used in pd_cont and pd_move */
static void
common_pd(FILE *plotfp, double x, double y, char c)
{
    size_t ret;
    double in[2];
    unsigned char out[2*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	bu_cv_htond(&out[1], (unsigned char *)in, 2);

	out[0] = c;
	ret = fwrite(out, 1, 2*8+1, plotfp);
	if (ret != 2*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "%c %g %g\n", c, x, y);
    }
}

/* Making a common pl_3 to be used in pl_3cont and pl_3move */
static void
pl_3(FILE *plotfp, int x, int y, int z, char c)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc( c, plotfp);
	putsi(x);
	putsi(y);
	putsi(z);
    } else {
	fprintf(plotfp, "%c %d %d %d\n", c, x, y, z);
    }
}

/*
 * These interfaces provide the standard UNIX-Plot functionality
 */

int
pl_getOutputMode(void) {
    return pl_outputMode;
}

void
pl_setOutputMode(int mode) {
    pl_outputMode = mode;
}

/**
 * @brief
 * plot a point
 */
void
pl_point(FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('p', plotfp);
	putsi(x);
	putsi(y);
    } else {
	fprintf(plotfp, "p %d %d\n", x, y);
    }
}

void
pl_line(FILE *plotfp, int px1, int py1, int px2, int py2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('l', plotfp);
	putsi(px1);
	putsi(py1);
	putsi(px2);
	putsi(py2);
    } else {
	fprintf(plotfp, "l %d %d %d %d\n", px1, py1, px2, py2);
    }
}

void
pl_linmod(FILE *plotfp, const char *s)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('f', plotfp);
	while (*s)
	    putc(*s++, plotfp);
	putc('\n', plotfp);
    } else {
	fprintf(plotfp, "f %s\n", s);
    }
}

void
pl_move(FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('m', plotfp);
	putsi(x);
	putsi(y);
    } else {
	fprintf(plotfp, "m %d %d\n", x, y);
    }
}

void
pl_cont(FILE *plotfp, int x, int y)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('n', plotfp);
	putsi(x);
	putsi(y);
    } else {
	fprintf(plotfp, "n %d %d\n", x, y);
    }
}

void
pl_label(FILE *plotfp, const char *s)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('t', plotfp);
	while (*s)
	    putc(*s++, plotfp);
	putc('\n', plotfp);
    } else {
	fprintf(plotfp, "t %s\n", s);
    }
}

void
pl_space(FILE *plotfp, int px1, int py1, int px2, int py2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('s', plotfp);
	putsi(px1);
	putsi(py1);
	putsi(px2);
	putsi(py2);
    } else {
	fprintf(plotfp, "s %d %d %d %d\n", px1, py1, px2, py2);
    }
}

void
pl_erase(FILE *plotfp)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY)
	putc('e', plotfp);
    else
	fprintf(plotfp, "e\n");
}

void
pl_circle(FILE *plotfp, int x, int y, int r)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('c', plotfp);
	putsi(x);
	putsi(y);
	putsi(r);
    } else {
	fprintf(plotfp, "c %d %d %d\n", x, y, r);
    }
}

void
pl_arc(FILE *plotfp, int xc, int yc, int px1, int py1, int px2, int py2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('a', plotfp);
	putsi(xc);
	putsi(yc);
	putsi(px1);
	putsi(py1);
	putsi(px2);
	putsi(py2);
    } else {
	fprintf(plotfp, "a %d %d %d %d %d %d\n", xc, yc, px1, py1, px2, py2);
    }
}

void
pl_box(FILE *plotfp, int px1, int py1, int px2, int py2)
{
    pl_move(plotfp, px1, py1);
    pl_cont(plotfp, px1, py2);
    pl_cont(plotfp, px2, py2);
    pl_cont(plotfp, px2, py1);
    pl_cont(plotfp, px1, py1);
    pl_move(plotfp, px2, py2);
}

/*
 * Here lie the BRL 3-D extensions.
 */

/* Warning: r, g, b are ints.  The output is chars. */
void
pl_color(FILE *plotfp, int r, int g, int b)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('C', plotfp);
	putc(r, plotfp);
	putc(g, plotfp);
	putc(b, plotfp);
    } else {
	fprintf(plotfp, "C %d %d %d\n", r, g, b);
    }
}

void
pl_color_buc(FILE *plotfp, struct bu_color *c)
{
    int r = 0;
    int g = 0;
    int b = 0;
    (void)bu_color_to_rgb_ints(c, &r, &g, &b);
    pl_color(plotfp, r, g, b);
}

void
pl_flush(FILE *plotfp)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('F', plotfp);
    } else {
	fprintf(plotfp, "F\n");
    }

    fflush(plotfp);
}

void
pl_3space(FILE *plotfp, int px1, int py1, int pz1, int px2, int py2, int pz2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('S', plotfp);
	putsi(px1);
	putsi(py1);
	putsi(pz1);
	putsi(px2);
	putsi(py2);
	putsi(pz2);
    } else {
	fprintf(plotfp, "S %d %d %d %d %d %d\n", px1, py1, pz1, px2, py2, pz2);
    }
}

void
pl_3point(FILE *plotfp, int x, int y, int z)
{
    pl_3(plotfp, x, y, z, 'P'); /* calling common function pl_3 */
}

void
pl_3move(FILE *plotfp, int x, int y, int z)
{
    pl_3(plotfp, x, y, z, 'M'); /* calling common function pl_3 */
}

void
pl_3cont(FILE *plotfp, int x, int y, int z)
{
    pl_3(plotfp, x, y, z, 'N'); /* calling common function pl_3 */
}

void
pl_3line(FILE *plotfp, int px1, int py1, int pz1, int px2, int py2, int pz2)
{
    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	putc('L', plotfp);
	putsi(px1);
	putsi(py1);
	putsi(pz1);
	putsi(px2);
	putsi(py2);
	putsi(pz2);
    } else {
	fprintf(plotfp, "L %d %d %d %d %d %d\n", px1, py1, pz1, px2, py2, pz2);
    }
}

void
pl_3box(FILE *plotfp, int px1, int py1, int pz1, int px2, int py2, int pz2)
{
    pl_3move(plotfp, px1, py1, pz1);
    /* first side */
    pl_3cont(plotfp, px1, py2, pz1);
    pl_3cont(plotfp, px1, py2, pz2);
    pl_3cont(plotfp, px1, py1, pz2);
    pl_3cont(plotfp, px1, py1, pz1);
    /* across */
    pl_3cont(plotfp, px2, py1, pz1);
    /* second side */
    pl_3cont(plotfp, px2, py2, pz1);
    pl_3cont(plotfp, px2, py2, pz2);
    pl_3cont(plotfp, px2, py1, pz2);
    pl_3cont(plotfp, px2, py1, pz1);
    /* front edge */
    pl_3move(plotfp, px1, py2, pz1);
    pl_3cont(plotfp, px2, py2, pz1);
    /* bottom back */
    pl_3move(plotfp, px1, py1, pz2);
    pl_3cont(plotfp, px2, py1, pz2);
    /* top back */
    pl_3move(plotfp, px1, py2, pz2);
    pl_3cont(plotfp, px2, py2, pz2);
}

/*
 * Double floating point versions
 */

void
pd_point(FILE *plotfp, double x, double y)
{
    common_pd( plotfp, x, y, 'x'); /* calling common function pd */
}

void
pd_line(FILE *plotfp, double px1, double py1, double px2, double py2)
{
    size_t ret;
    double in[4];
    unsigned char out[4*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = px1;
	in[1] = py1;
	in[2] = px2;
	in[3] = py2;
	bu_cv_htond(&out[1], (unsigned char *)in, 4);

	out[0] = 'v';
	ret = fwrite(out, 1, 4*8+1, plotfp);
	if (ret != 4*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "v %g %g %g %g\n", px1, py1, px2, py2);
    }
}

/* Note: no pd_linmod(), just use pl_linmod() */

void
pd_move(FILE *plotfp, double x, double y)
{
    common_pd( plotfp, x, y, 'o'); /* calling common function pd */
}

void
pd_cont(FILE *plotfp, double x, double y)
{
    common_pd( plotfp, x, y, 'q'); /* calling common function pd */
}

void
pd_space(FILE *plotfp, double px1, double py1, double px2, double py2)
{
    size_t ret;
    double in[4];
    unsigned char out[4*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = px1;
	in[1] = py1;
	in[2] = px2;
	in[3] = py2;
	bu_cv_htond(&out[1], (unsigned char *)in, 4);

	out[0] = 'w';
	ret = fwrite(out, 1, 4*8+1, plotfp);
	if (ret != 4*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "w %g %g %g %g\n", px1, py1, px2, py2);
    }
}

void
pd_circle(FILE *plotfp, double x, double y, double r)
{
    size_t ret;
    double in[3];
    unsigned char out[3*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = x;
	in[1] = y;
	in[2] = r;
	bu_cv_htond(&out[1], (unsigned char *)in, 3);

	out[0] = 'i';
	ret = fwrite(out, 1, 3*8+1, plotfp);
	if (ret != 3*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "i %g %g %g\n", x, y, r);
    }
}

void
pd_arc(FILE *plotfp, double xc, double yc, double px1, double py1, double px2, double py2)
{
    size_t ret;
    double in[6];
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = xc;
	in[1] = yc;
	in[2] = px1;
	in[3] = py1;
	in[4] = px2;
	in[5] = py2;
	bu_cv_htond(&out[1], (unsigned char *)in, 6);

	out[0] = 'r';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "r %g %g %g %g %g %g\n", xc, yc, px1, py1, px2, py2);
    }
}

void
pd_box(FILE *plotfp, double px1, double py1, double px2, double py2)
{
    pd_move(plotfp, px1, py1);
    pd_cont(plotfp, px1, py2);
    pd_cont(plotfp, px2, py2);
    pd_cont(plotfp, px2, py1);
    pd_cont(plotfp, px1, py1);
    pd_move(plotfp, px2, py2);
}

/* Double 3-D, both in vector and enumerated versions */
void
pdv_3space(FILE *plotfp, const vect_t min, const vect_t max)
{
    size_t ret;
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	bu_cv_htond(&out[1], (unsigned char *)min, 3);
	bu_cv_htond(&out[3*8+1], (unsigned char *)max, 3);

	out[0] = 'W';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "W %g %g %g %g %g %g\n", V3ARGS(min), V3ARGS(max));
    }
}

void
pd_3space(FILE *plotfp, double px1, double py1, double pz1, double px2, double py2, double pz2)
{
    size_t ret;
    double in[6];
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = px1;
	in[1] = py1;
	in[2] = pz1;
	in[3] = px2;
	in[4] = py2;
	in[5] = pz2;
	bu_cv_htond(&out[1], (unsigned char *)in, 6);

	out[0] = 'W';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "W %g %g %g %g %g %g\n", px1, py1, pz1, px2, py2, pz2);
    }
}

void
pdv_3point(FILE *plotfp, const point_t pt)
{
    pdv_3(plotfp, pt, 'X'); /* calling common function pdv_3 */
}

void
pd_3point(FILE *plotfp, double x, double y, double z)
{
    pd_3(plotfp, x, y, z, 'X'); /* calling common function pd_3 */
}

void
pdv_3move(FILE *plotfp, const point_t pt)
{
    pdv_3(plotfp, pt, 'O'); /* calling common function pdv_3 */
}

void
pd_3move(FILE *plotfp, double x, double y, double z)
{
    pd_3(plotfp, x, y, z, 'O'); /* calling common function pd_3 */
}

void
pdv_3cont(FILE *plotfp, const point_t pt)
{
    pdv_3(plotfp, pt, 'Q'); /* calling common function pdv_3 */
}

void
pd_3cont(FILE *plotfp, double x, double y, double z)
{
    pd_3(plotfp, x, y, z, 'Q'); /* calling common function pd_3 */
}

void
pdv_3line(FILE *plotfp, const vect_t a, const vect_t b)
{
    size_t ret;
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	bu_cv_htond(&out[1], (unsigned char *)a, 3);
	bu_cv_htond(&out[3*8+1], (unsigned char *)b, 3);

	out[0] = 'V';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "V %g %g %g %g %g %g\n", V3ARGS(a), V3ARGS(b));
    }
}

void
pd_3line(FILE *plotfp, double px1, double py1, double pz1, double px2, double py2, double pz2)
{
    size_t ret;
    double in[6];
    unsigned char out[6*8+1];

    if (pl_outputMode == PL_OUTPUT_MODE_BINARY) {
	in[0] = px1;
	in[1] = py1;
	in[2] = pz1;
	in[3] = px2;
	in[4] = py2;
	in[5] = pz2;
	bu_cv_htond(&out[1], (unsigned char *)in, 6);

	out[0] = 'V';
	ret = fwrite(out, 1, 6*8+1, plotfp);
	if (ret != 6*8+1) {
	    perror("fwrite");
	}
    } else {
	fprintf(plotfp, "V %g %g %g %g %g %g\n", px1, py1, pz1, px2, py2, pz2);
    }
}

void
pdv_3box(FILE *plotfp, const vect_t a, const vect_t b)
{
    pd_3move(plotfp, a[X], a[Y], a[Z]);
    /* first side */
    pd_3cont(plotfp, a[X], b[Y], a[Z]);
    pd_3cont(plotfp, a[X], b[Y], b[Z]);
    pd_3cont(plotfp, a[X], a[Y], b[Z]);
    pd_3cont(plotfp, a[X], a[Y], a[Z]);
    /* across */
    pd_3cont(plotfp, b[X], a[Y], a[Z]);
    /* second side */
    pd_3cont(plotfp, b[X], b[Y], a[Z]);
    pd_3cont(plotfp, b[X], b[Y], b[Z]);
    pd_3cont(plotfp, b[X], a[Y], b[Z]);
    pd_3cont(plotfp, b[X], a[Y], a[Z]);
    /* front edge */
    pd_3move(plotfp, a[X], b[Y], a[Z]);
    pd_3cont(plotfp, b[X], b[Y], a[Z]);
    /* bottom back */
    pd_3move(plotfp, a[X], a[Y], b[Z]);
    pd_3cont(plotfp, b[X], a[Y], b[Z]);
    /* top back */
    pd_3move(plotfp, a[X], b[Y], b[Z]);
    pd_3cont(plotfp, b[X], b[Y], b[Z]);
}

void
pd_3box(FILE *plotfp, double px1, double py1, double pz1, double px2, double py2, double pz2)
{
    pd_3move(plotfp, px1, py1, pz1);
    /* first side */
    pd_3cont(plotfp, px1, py2, pz1);
    pd_3cont(plotfp, px1, py2, pz2);
    pd_3cont(plotfp, px1, py1, pz2);
    pd_3cont(plotfp, px1, py1, pz1);
    /* across */
    pd_3cont(plotfp, px2, py1, pz1);
    /* second side */
    pd_3cont(plotfp, px2, py2, pz1);
    pd_3cont(plotfp, px2, py2, pz2);
    pd_3cont(plotfp, px2, py1, pz2);
    pd_3cont(plotfp, px2, py1, pz1);
    /* front edge */
    pd_3move(plotfp, px1, py2, pz1);
    pd_3cont(plotfp, px2, py2, pz1);
    /* bottom back */
    pd_3move(plotfp, px1, py1, pz2);
    pd_3cont(plotfp, px2, py1, pz2);
    /* top back */
    pd_3move(plotfp, px1, py2, pz2);
    pd_3cont(plotfp, px2, py2, pz2);
}

/**
 * Draw a ray
 */
void
pdv_3ray(FILE *fp, const point_t pt, const vect_t dir, double t)
{
    point_t tip;

    VJOIN1(tip, pt, t, dir);
    pdv_3move(fp, pt);
    pdv_3cont(fp, tip);
}


/*
 * Routines to validate a plot file
 */

static int
read_short(FILE *fp, int cnt, int mode)
{
    if (mode == PL_OUTPUT_MODE_BINARY) {
	for (int i = 0; i < cnt * 2; i++) {
	    if (getc(fp) == EOF)
		return 1;
	}
	return 0;
    }
    if (mode == PL_OUTPUT_MODE_TEXT) {
	int ret;
	double val;
	for (int i = 0; i < cnt; i++) {
	    ret = fscanf(fp, "%lf", &val);
	    if (ret != 1)
		return 1;
	}
	return 0;
    }

    return 1;
}

static int
read_ieee(FILE *fp, int cnt, int mode)
{
    size_t ret;
    if (mode == PL_OUTPUT_MODE_BINARY) {
	for (int i = 0; i < cnt; i++) {
	    char inbuf[SIZEOF_NETWORK_DOUBLE];
	    ret = fread(inbuf, SIZEOF_NETWORK_DOUBLE, 1, fp);
	    if (ret != 1)
		return 1;
	}
	return 0;
    }
    if (mode == PL_OUTPUT_MODE_TEXT) {
	double val;
	for (int i = 0; i < cnt; i++) {
	    ret = (size_t)fscanf(fp, "%lf", &val);
	    if (ret != 1)
		return 1;
	}
	return 0;
    }
    return 1;
}

static int
read_tstring(FILE *fp, int mode)
{
    int ret;
    if (mode == PL_OUTPUT_MODE_BINARY) {
	int str_done = 0;
	int cc;
	while (!feof(fp) && !str_done) {
	    cc = getc(fp);
	    if (cc == '\n')
		str_done = 1;
	}
	return 0;
    }
    if (mode == PL_OUTPUT_MODE_TEXT) {
	char carg[256] = {0};
	ret = fscanf(fp, "%255s\n", &carg[0]);
	if (ret != 1)
	    return 1;
	return 0;
    }
    return 1;
}

#define PLOT3_MARK 1
#define PLOT3_LINE 2
#define PLOT3_NUM_SYMBOLS 8
#define PLOT3_BRT(_x, _y) (11*(_x)+(_y))
#define PLOT3_DRK(_x, _y) -(11*(_x)+(_y))
#define PLOT3_BNEG(_x, _y) PLOT3_FONT_NEGY, PLOT3_BRT((_x), (_y))
#define PLOT3_DNEG(_x, _y) PLOT3_FONT_NEGY, PLOT3_DRK((_x), (_y))
#define PLOT3_TICK_YLEN (char_width)
#define PLOT3_NUM_YOFF (3*char_width)
#define PLOT3_TITLE_YOFF (5*char_width)
#define PLOT3_TIC 100
#define PLOT3_REF_WIDTH 0.857143
#define PLOT3_NUM_DISTANCE 250
#define PLOT3_LAB_LNGTH 860

static int *plot3_font_index[256];

static int plot3_font_table[] = {

/*	+	*/
    PLOT3_DRK(0, 5),
    PLOT3_BRT(8, 5),
    PLOT3_DRK(4, 8),
    PLOT3_BRT(4, 2),
    PLOT3_FONT_LAST,

/*	x	*/
    PLOT3_DRK(0, 2),
    PLOT3_BRT(8, 8),
    PLOT3_DRK(0, 8),
    PLOT3_BRT(8, 2),
    PLOT3_FONT_LAST,

/*	triangle	*/
    PLOT3_DRK(0, 2),
    PLOT3_BRT(4, 8),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(0, 2),
    PLOT3_FONT_LAST,

/*	square	*/
    PLOT3_DRK(0, 2),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(0, 2),
    PLOT3_FONT_LAST,

/*	hourglass	*/
    PLOT3_DRK(0, 2),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(0, 2),
    PLOT3_FONT_LAST,

/*	plus-minus	*/
    PLOT3_DRK(5, 7),
    PLOT3_BRT(5, 2),
    PLOT3_DRK(2, 2),
    PLOT3_BRT(8, 2),
    PLOT3_DRK(2, 5),
    PLOT3_BRT(8, 5),
    PLOT3_FONT_LAST,

/*	centerline symbol	*/
    PLOT3_DRK(8, 4),
    PLOT3_BRT(7, 6),
    PLOT3_BRT(4, 7),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(0, 4),
    PLOT3_BRT(1, 2),
    PLOT3_BRT(4, 1),
    PLOT3_BRT(7, 2),
    PLOT3_BRT(8, 4),
    PLOT3_DRK(1, 1),
    PLOT3_BRT(7, 7),
    PLOT3_FONT_LAST,

/*	degree symbol	*/
    PLOT3_DRK(1, 9),
    PLOT3_BRT(2, 9),
    PLOT3_BRT(3, 8),
    PLOT3_BRT(3, 7),
    PLOT3_BRT(2, 6),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(0, 7),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(1, 9),
    PLOT3_FONT_LAST,

/*	table for ascii 040, ' '	*/
    PLOT3_FONT_LAST,

/*	table for !	*/
    PLOT3_DRK(3, 0),
    PLOT3_BRT(5, 2),
    PLOT3_BRT(5, 0),
    PLOT3_BRT(3, 2),
    PLOT3_BRT(3, 0),
    PLOT3_DRK(4, 4),
    PLOT3_BRT(3, 10),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(4, 4),
    PLOT3_BRT(4, 10),
    PLOT3_FONT_LAST,

/*	table for "	*/
    PLOT3_DRK(1, 10),
    PLOT3_BRT(3, 10),
    PLOT3_BRT(2, 7),
    PLOT3_BRT(1, 10),
    PLOT3_DRK(5, 10),
    PLOT3_BRT(7, 10),
    PLOT3_BRT(6, 7),
    PLOT3_BRT(5, 10),
    PLOT3_FONT_LAST,


/*	table for #	*/
    PLOT3_DRK(1, 0),
    PLOT3_BRT(3, 9),
    PLOT3_DRK(6, 9),
    PLOT3_BRT(4, 0),
    PLOT3_DRK(6, 3),
    PLOT3_BRT(0, 3),
    PLOT3_DRK(1, 6),
    PLOT3_BRT(7, 6),
    PLOT3_FONT_LAST,

/*	table for $	*/
    PLOT3_DRK(1, 2),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(7, 5),
    PLOT3_BRT(1, 5),
    PLOT3_BRT(1, 9),
    PLOT3_BRT(7, 9),
    PLOT3_BRT(7, 8),
    PLOT3_DRK(4, 10),
    PLOT3_BRT(4, 0),
    PLOT3_FONT_LAST,

/*	table for %	*/
    PLOT3_DRK(3, 10),
    PLOT3_BRT(3, 7),
    PLOT3_BRT(0, 7),
    PLOT3_BRT(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(0, 0),
    PLOT3_DRK(8, 0),
    PLOT3_BRT(5, 0),
    PLOT3_BRT(5, 3),
    PLOT3_BRT(8, 3),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for &	*/
    PLOT3_DRK(7, 3),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(1, 0),
    PLOT3_BRT(0, 3),
    PLOT3_BRT(5, 8),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(3, 10),
    PLOT3_BRT(1, 8),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for '	*/
    PLOT3_DRK(4, 6),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(4, 6),
    PLOT3_FONT_LAST,

/*	table for (	*/
    PLOT3_DRK(5, 0),
    PLOT3_BRT(3, 1),
    PLOT3_BRT(2, 4),
    PLOT3_BRT(2, 6),
    PLOT3_BRT(3, 9),
    PLOT3_BRT(5, 10),
    PLOT3_FONT_LAST,

/*	table for)	*/
    PLOT3_DRK(3, 0),
    PLOT3_BRT(5, 1),
    PLOT3_BRT(6, 4),
    PLOT3_BRT(6, 6),
    PLOT3_BRT(5, 9),
    PLOT3_BRT(3, 10),
    PLOT3_FONT_LAST,

/*	table for *	*/
    PLOT3_DRK(4, 2),
    PLOT3_BRT(4, 8),
    PLOT3_DRK(6, 7),
    PLOT3_BRT(2, 3),
    PLOT3_DRK(6, 3),
    PLOT3_BRT(2, 7),
    PLOT3_DRK(1, 5),
    PLOT3_BRT(7, 5),
    PLOT3_FONT_LAST,

/*	table for +	*/
    PLOT3_DRK(1, 5),
    PLOT3_BRT(7, 5),
    PLOT3_DRK(4, 8),
    PLOT3_BRT(4, 2),
    PLOT3_FONT_LAST,

/*	table for, 	*/
    PLOT3_DRK(5, 0),
    PLOT3_BRT(3, 2),
    PLOT3_BRT(3, 0),
    PLOT3_BRT(5, 2),
    PLOT3_BRT(5, 0),
    PLOT3_BNEG(2, 2),
    PLOT3_BRT(4, 0),
    PLOT3_FONT_LAST,

/*	table for -	*/
    PLOT3_DRK(1, 5),
    PLOT3_BRT(7, 5),
    PLOT3_FONT_LAST,

/*	table for .	*/
    PLOT3_DRK(5, 0),
    PLOT3_BRT(3, 2),
    PLOT3_BRT(3, 0),
    PLOT3_BRT(5, 2),
    PLOT3_BRT(5, 0),
    PLOT3_FONT_LAST,

/*	table for /	*/
    PLOT3_BRT(8, 10),
    PLOT3_FONT_LAST,

/*	table for 0	*/
    PLOT3_DRK(8, 10),
    PLOT3_BRT(0, 0),
    PLOT3_BRT(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(8, 0),
    PLOT3_BRT(0, 0),
    PLOT3_FONT_LAST,

/*	table for 1	*/
    PLOT3_DRK(4, 0),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(2, 8),
    PLOT3_FONT_LAST,

/*	table for 2	*/
    PLOT3_DRK(0, 6),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(3, 10),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 7),
    PLOT3_BRT(0, 2),
    PLOT3_BRT(0, 0),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for 3	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(8, 0),
    PLOT3_BRT(0, 0),
    PLOT3_FONT_LAST,

/*	table for 4	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(8, 5),
    PLOT3_DRK(8, 10),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for 5	*/
    PLOT3_DRK(8, 10),
    PLOT3_BRT(0, 10),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(8, 0),
    PLOT3_BRT(0, 0),
    PLOT3_FONT_LAST,

/*	table for 6	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(0, 0),
    PLOT3_BRT(8, 0),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(0, 5),
    PLOT3_FONT_LAST,

/*	table for 7	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(6, 0),
    PLOT3_FONT_LAST,

/*	table for 8	*/
    PLOT3_DRK(0, 5),
    PLOT3_BRT(0, 0),
    PLOT3_BRT(8, 0),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(8, 5),
    PLOT3_FONT_LAST,

/*	table for 9	*/
    PLOT3_DRK(8, 5),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for :	*/
    PLOT3_DRK(5, 6),
    PLOT3_BRT(3, 8),
    PLOT3_BRT(3, 6),
    PLOT3_BRT(5, 8),
    PLOT3_BRT(5, 6),
    PLOT3_DRK(5, 0),
    PLOT3_BRT(3, 2),
    PLOT3_BRT(3, 0),
    PLOT3_BRT(5, 2),
    PLOT3_BRT(5, 0),
    PLOT3_FONT_LAST,

/*	table for ;	*/
    PLOT3_DRK(5, 6),
    PLOT3_BRT(3, 8),
    PLOT3_BRT(3, 6),
    PLOT3_BRT(5, 8),
    PLOT3_BRT(5, 6),
    PLOT3_DRK(5, 0),
    PLOT3_BRT(3, 2),
    PLOT3_BRT(3, 0),
    PLOT3_BRT(5, 2),
    PLOT3_BRT(5, 0),
    PLOT3_BNEG(2, 2),
    PLOT3_BRT(4, 0),
    PLOT3_FONT_LAST,

/*	table for <	*/
    PLOT3_DRK(8, 8),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(8, 2),
    PLOT3_FONT_LAST,

/*	table for =	*/
    PLOT3_DRK(0, 7),
    PLOT3_BRT(8, 7),
    PLOT3_DRK(0, 3),
    PLOT3_BRT(8, 3),
    PLOT3_FONT_LAST,

/*	table for >	*/
    PLOT3_DRK(0, 8),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(0, 2),
    PLOT3_FONT_LAST,

/*	table for ?	*/
    PLOT3_DRK(3, 0),
    PLOT3_BRT(5, 2),
    PLOT3_BRT(5, 0),
    PLOT3_BRT(3, 2),
    PLOT3_BRT(3, 0),
    PLOT3_DRK(1, 7),
    PLOT3_BRT(1, 9),
    PLOT3_BRT(3, 10),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(7, 9),
    PLOT3_BRT(7, 7),
    PLOT3_BRT(4, 5),
    PLOT3_BRT(4, 3),
    PLOT3_FONT_LAST,

/*	table for @	*/
    PLOT3_DRK(0, 8),
    PLOT3_BRT(2, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(1, 4),
    PLOT3_BRT(2, 5),
    PLOT3_BRT(4, 5),
    PLOT3_BRT(5, 4),
    PLOT3_BRT(5, 0),
    PLOT3_FONT_LAST,

/*	table for A	*/
    PLOT3_BRT(0, 8),
    PLOT3_BRT(2, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 0),
    PLOT3_DRK(0, 5),
    PLOT3_BRT(8, 5),
    PLOT3_FONT_LAST,

/*	table for B	*/
    PLOT3_BRT(0, 10),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(8, 9),
    PLOT3_BRT(8, 6),
    PLOT3_BRT(5, 5),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(5, 5),
    PLOT3_BRT(8, 4),
    PLOT3_BRT(8, 1),
    PLOT3_BRT(5, 0),
    PLOT3_BRT(0, 0),
    PLOT3_FONT_LAST,

/*	table for C	*/
    PLOT3_DRK(8, 2),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(0, 2),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(2, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 8),
    PLOT3_FONT_LAST,

/*	table for D	*/
    PLOT3_BRT(0, 10),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(5, 0),
    PLOT3_BRT(0, 0),
    PLOT3_FONT_LAST,

/*	table for E	*/
    PLOT3_DRK(8, 0),
    PLOT3_BRT(0, 0),
    PLOT3_BRT(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_DRK(0, 5),
    PLOT3_BRT(5, 5),
    PLOT3_FONT_LAST,

/*	table for F	*/
    PLOT3_BRT(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_DRK(0, 5),
    PLOT3_BRT(5, 5),
    PLOT3_FONT_LAST,

/*	table for G	*/
    PLOT3_DRK(5, 5),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(0, 2),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(2, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 8),
    PLOT3_FONT_LAST,

/*	table for H	*/
    PLOT3_BRT(0, 10),
    PLOT3_DRK(8, 10),
    PLOT3_BRT(8, 0),
    PLOT3_DRK(0, 6),
    PLOT3_BRT(8, 6),
    PLOT3_FONT_LAST,

/*	table for I	*/
    PLOT3_DRK(4, 0),
    PLOT3_BRT(6, 0),
    PLOT3_DRK(5, 0),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(6, 10),
    PLOT3_FONT_LAST,

/*	table for J	*/
    PLOT3_DRK(0, 2),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(5, 0),
    PLOT3_BRT(7, 2),
    PLOT3_BRT(7, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 10),
    PLOT3_FONT_LAST,

/*	table for K	*/
    PLOT3_BRT(0, 10),
    PLOT3_DRK(0, 5),
    PLOT3_BRT(8, 10),
    PLOT3_DRK(3, 7),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for L	*/
    PLOT3_DRK(8, 0),
    PLOT3_BRT(0, 0),
    PLOT3_BRT(0, 10),
    PLOT3_FONT_LAST,

/*	table for M	*/
    PLOT3_BRT(0, 10),
    PLOT3_BRT(4, 5),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for N	*/
    PLOT3_BRT(0, 10),
    PLOT3_BRT(8, 0),
    PLOT3_BRT(8, 10),
    PLOT3_FONT_LAST,

/*	table for O	*/
    PLOT3_DRK(0, 2),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(2, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(0, 2),
    PLOT3_FONT_LAST,

/*	table for P	*/
    PLOT3_BRT(0, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 9),
    PLOT3_BRT(8, 6),
    PLOT3_BRT(6, 5),
    PLOT3_BRT(0, 5),
    PLOT3_FONT_LAST,

/*	table for Q	*/
    PLOT3_DRK(0, 2),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(2, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(0, 2),
    PLOT3_DRK(5, 3),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for R	*/
    PLOT3_BRT(0, 10),
    PLOT3_BRT(6, 10),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 6),
    PLOT3_BRT(6, 5),
    PLOT3_BRT(0, 5),
    PLOT3_DRK(5, 5),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for S	*/
    PLOT3_DRK(0, 1),
    PLOT3_BRT(1, 0),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(8, 4),
    PLOT3_BRT(6, 6),
    PLOT3_BRT(2, 6),
    PLOT3_BRT(0, 7),
    PLOT3_BRT(0, 9),
    PLOT3_BRT(1, 10),
    PLOT3_BRT(7, 10),
    PLOT3_BRT(8, 9),
    PLOT3_FONT_LAST,

/*	table for T	*/
    PLOT3_DRK(4, 0),
    PLOT3_BRT(4, 10),
    PLOT3_DRK(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_FONT_LAST,

/*	table for U	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(0, 2),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(8, 10),
    PLOT3_FONT_LAST,

/*	table for V	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(8, 10),
    PLOT3_FONT_LAST,

/*	table for W	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(1, 0),
    PLOT3_BRT(4, 4),
    PLOT3_BRT(7, 0),
    PLOT3_BRT(8, 10),
    PLOT3_FONT_LAST,

/*	table for X	*/
    PLOT3_BRT(8, 10),
    PLOT3_DRK(0, 10),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for Y	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(4, 4),
    PLOT3_BRT(8, 10),
    PLOT3_DRK(4, 4),
    PLOT3_BRT(4, 0),
    PLOT3_FONT_LAST,

/*	table for Z	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(8, 10),
    PLOT3_BRT(0, 0),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for [	*/
    PLOT3_DRK(6, 0),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(6, 10),
    PLOT3_FONT_LAST,

/*	table for \	*/
    PLOT3_DRK(0, 10),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for ]	*/
    PLOT3_DRK(2, 0),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(2, 10),
    PLOT3_FONT_LAST,

/*	table for ^	*/
    PLOT3_DRK(4, 0),
    PLOT3_BRT(4, 10),
    PLOT3_DRK(2, 8),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(6, 8),
    PLOT3_FONT_LAST,

/*	table for _	*/
    PLOT3_DNEG(0, 1),
    PLOT3_BNEG(11, 1),
    PLOT3_FONT_LAST,

/*	table for ascii 96: accent	*/
    PLOT3_DRK(3, 10),
    PLOT3_BRT(5, 6),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(3, 10),
    PLOT3_FONT_LAST,

/*	table for a	*/
    PLOT3_DRK(0, 5),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(6, 6),
    PLOT3_BRT(7, 5),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(8, 0),
    PLOT3_DRK(7, 1),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(1, 0),
    PLOT3_BRT(0, 1),
    PLOT3_BRT(0, 2),
    PLOT3_BRT(1, 3),
    PLOT3_BRT(6, 3),
    PLOT3_BRT(7, 2),
    PLOT3_FONT_LAST,

/*	table for b	*/
    PLOT3_BRT(0, 10),
    PLOT3_DRK(8, 3),
    PLOT3_BRT(7, 5),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(1, 5),
    PLOT3_BRT(0, 3),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(8, 3),
    PLOT3_FONT_LAST,

/*	table for c	*/
    PLOT3_DRK(8, 5),
    PLOT3_BRT(7, 6),
    PLOT3_BRT(2, 6),
    PLOT3_BRT(0, 4),
    PLOT3_BRT(0, 4),
    PLOT3_BRT(0, 2),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(7, 0),
    PLOT3_BRT(8, 1),
    PLOT3_FONT_LAST,

/*	table for d	*/
    PLOT3_DRK(8, 0),
    PLOT3_BRT(8, 10),
    PLOT3_DRK(8, 3),
    PLOT3_BRT(7, 5),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(1, 5),
    PLOT3_BRT(0, 3),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(8, 3),
    PLOT3_FONT_LAST,

/*	table for e	*/
    PLOT3_DRK(0, 4),
    PLOT3_BRT(1, 3),
    PLOT3_BRT(7, 3),
    PLOT3_BRT(8, 4),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(7, 6),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(0, 1),
    PLOT3_BRT(1, 0),
    PLOT3_BRT(7, 0),
    PLOT3_BRT(8, 1),
    PLOT3_FONT_LAST,

/*	table for f	*/
    PLOT3_DRK(2, 0),
    PLOT3_BRT(2, 9),
    PLOT3_BRT(3, 10),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(6, 9),
    PLOT3_DRK(1, 5),
    PLOT3_BRT(4, 5),
    PLOT3_FONT_LAST,

/*	table for g	*/
    PLOT3_DRK(8, 6),
    PLOT3_DRK(8, 3),
    PLOT3_BRT(7, 5),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(1, 5),
    PLOT3_BRT(0, 3),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(8, 3),
    PLOT3_BNEG(8, 2),
    PLOT3_BNEG(7, 3),
    PLOT3_BNEG(1, 3),
    PLOT3_BNEG(0, 2),
    PLOT3_FONT_LAST,

/*	table for h	*/
    PLOT3_BRT(0, 10),
    PLOT3_DRK(0, 4),
    PLOT3_BRT(2, 6),
    PLOT3_BRT(6, 6),
    PLOT3_BRT(8, 4),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for i	*/
    PLOT3_DRK(4, 0),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(3, 6),
    PLOT3_DRK(4, 9),
    PLOT3_BRT(4, 8),
    PLOT3_DRK(3, 0),
    PLOT3_BRT(5, 0),
    PLOT3_FONT_LAST,

/*	table for j	*/
    PLOT3_DRK(5, 6),
    PLOT3_BRT(6, 6),
    PLOT3_BNEG(6, 2),
    PLOT3_BNEG(5, 3),
    PLOT3_BNEG(3, 3),
    PLOT3_BNEG(2, 2),
    PLOT3_FONT_LAST,

/*	table for k	*/
    PLOT3_BRT(2, 0),
    PLOT3_BRT(2, 10),
    PLOT3_BRT(0, 10),
    PLOT3_DRK(2, 4),
    PLOT3_BRT(4, 4),
    PLOT3_BRT(8, 6),
    PLOT3_DRK(4, 4),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for l	*/
    PLOT3_DRK(3, 10),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(4, 2),
    PLOT3_BRT(5, 0),
    PLOT3_FONT_LAST,

/*	table for m	*/
    PLOT3_BRT(0, 6),
    PLOT3_DRK(0, 5),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(3, 6),
    PLOT3_BRT(4, 5),
    PLOT3_BRT(4, 0),
    PLOT3_DRK(4, 5),
    PLOT3_BRT(5, 6),
    PLOT3_BRT(7, 6),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for n	*/
    PLOT3_BRT(0, 6),
    PLOT3_DRK(0, 4),
    PLOT3_BRT(2, 6),
    PLOT3_BRT(6, 6),
    PLOT3_BRT(8, 4),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for o	*/
    PLOT3_DRK(8, 3),
    PLOT3_BRT(7, 5),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(1, 5),
    PLOT3_BRT(0, 3),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(8, 3),
    PLOT3_FONT_LAST,

/*	table for p	*/
    PLOT3_DRK(0, 6),
    PLOT3_BNEG(0, 3),
    PLOT3_DRK(8, 3),
    PLOT3_BRT(7, 5),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(1, 5),
    PLOT3_BRT(0, 3),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(8, 3),
    PLOT3_FONT_LAST,

/*	table for q	*/
    PLOT3_DRK(8, 6),
    PLOT3_DRK(8, 3),
    PLOT3_BRT(7, 5),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(1, 5),
    PLOT3_BRT(0, 3),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(8, 3),
    PLOT3_BNEG(8, 3),
    PLOT3_BNEG(9, 3),
    PLOT3_FONT_LAST,

/*	table for r	*/
    PLOT3_BRT(1, 0),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(0, 6),
    PLOT3_DRK(1, 4),
    PLOT3_BRT(3, 6),
    PLOT3_BRT(6, 6),
    PLOT3_BRT(8, 4),
    PLOT3_FONT_LAST,

/*	table for s	*/
    PLOT3_DRK(0, 1),
    PLOT3_BRT(1, 0),
    PLOT3_BRT(7, 0),
    PLOT3_BRT(8, 1),
    PLOT3_BRT(7, 2),
    PLOT3_BRT(1, 4),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(7, 6),
    PLOT3_BRT(8, 5),
    PLOT3_FONT_LAST,

/*	table for t	*/
    PLOT3_DRK(7, 1),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(3, 1),
    PLOT3_BRT(3, 10),
    PLOT3_BRT(2, 10),
    PLOT3_DRK(1, 5),
    PLOT3_BRT(5, 5),
    PLOT3_FONT_LAST,

/*	table for u	*/
    PLOT3_DRK(0, 6),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(1, 1),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(7, 1),
    PLOT3_BRT(7, 6),
    PLOT3_DRK(7, 1),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for v	*/
    PLOT3_DRK(0, 6),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(8, 6),
    PLOT3_FONT_LAST,

/*	table for w	*/
    PLOT3_DRK(0, 6),
    PLOT3_BRT(0, 5),
    PLOT3_BRT(2, 0),
    PLOT3_BRT(4, 5),
    PLOT3_BRT(6, 0),
    PLOT3_BRT(8, 5),
    PLOT3_BRT(8, 6),
    PLOT3_FONT_LAST,

/*	table for x	*/
    PLOT3_BRT(8, 6),
    PLOT3_DRK(0, 6),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for y	*/
    PLOT3_DRK(0, 6),
    PLOT3_BRT(0, 1),
    PLOT3_BRT(1, 0),
    PLOT3_BRT(7, 0),
    PLOT3_BRT(8, 1),
    PLOT3_DRK(8, 6),
    PLOT3_BNEG(8, 2),
    PLOT3_BNEG(7, 3),
    PLOT3_BNEG(1, 3),
    PLOT3_BNEG(0, 2),
    PLOT3_FONT_LAST,

/*	table for z	*/
    PLOT3_DRK(0, 6),
    PLOT3_BRT(8, 6),
    PLOT3_BRT(0, 0),
    PLOT3_BRT(8, 0),
    PLOT3_FONT_LAST,

/*	table for ascii 123, left brace	*/
    PLOT3_DRK(6, 10),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(4, 9),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(3, 5),
    PLOT3_BRT(4, 4),
    PLOT3_BRT(4, 1),
    PLOT3_BRT(5, 0),
    PLOT3_BRT(6, 0),
    PLOT3_FONT_LAST,

/*	table for ascii 124, vertical bar	*/
    PLOT3_DRK(4, 4),
    PLOT3_BRT(4, 0),
    PLOT3_BRT(5, 0),
    PLOT3_BRT(5, 4),
    PLOT3_BRT(4, 4),
    PLOT3_DRK(4, 6),
    PLOT3_BRT(4, 10),
    PLOT3_BRT(5, 10),
    PLOT3_BRT(5, 6),
    PLOT3_BRT(4, 6),
    PLOT3_FONT_LAST,

/*	table for ascii 125, right brace	*/
    PLOT3_DRK(2, 0),
    PLOT3_BRT(3, 0),
    PLOT3_BRT(4, 1),
    PLOT3_BRT(4, 4),
    PLOT3_BRT(5, 5),
    PLOT3_BRT(4, 6),
    PLOT3_BRT(4, 9),
    PLOT3_BRT(3, 10),
    PLOT3_BRT(2, 10),
    PLOT3_FONT_LAST,

/*	table for ascii 126, tilde	*/
    PLOT3_DRK(0, 5),
    PLOT3_BRT(1, 6),
    PLOT3_BRT(3, 6),
    PLOT3_BRT(5, 4),
    PLOT3_BRT(7, 4),
    PLOT3_BRT(8, 5),
    PLOT3_FONT_LAST,

/*	table for ascii 127, rubout	*/
    PLOT3_DRK(0, 2),
    PLOT3_BRT(0, 8),
    PLOT3_BRT(8, 8),
    PLOT3_BRT(8, 2),
    PLOT3_BRT(0, 2),
    PLOT3_FONT_LAST
};

static void
plot3_mat_mul(mat_t o, const mat_t a, const mat_t b)
{
    mat_t t;
    int row;
    int col;

    for (row = 0; row < 4; row++) {
	for (col = 0; col < 4; col++) {
	    t[row*4 + col] = a[row*4 + 0] * b[col + 0]
		+ a[row*4 + 1] * b[col + 4]
		+ a[row*4 + 2] * b[col + 8]
		+ a[row*4 + 3] * b[col + 12];
	}
    }

    MAT_COPY(o, t);
}

static void
plot3_mat_zrot(mat_t mat, double theta)
{
    double rad = theta * DEG2RAD;
    double s = sin(rad);
    double c = cos(rad);

    MAT_IDN(mat);
    mat[0] = c;
    mat[1] = -s;
    mat[4] = s;
    mat[5] = c;
}

static void
plot3_vec_ortho(vect_t out, const vect_t in)
{
    vect_t a;

    if (fabs(in[X]) <= fabs(in[Y]) && fabs(in[X]) <= fabs(in[Z])) {
	VSET(a, 1, 0, 0);
    } else if (fabs(in[Y]) <= fabs(in[Z])) {
	VSET(a, 0, 1, 0);
    } else {
	VSET(a, 0, 0, 1);
    }

    VCROSS(out, in, a);
    VUNITIZE(out);
}

static void
plot3_font_setup(void)
{
    int *p = plot3_font_table;
    int i;

    for (i = 040 - PLOT3_NUM_SYMBOLS; i < 128; i++) {
	plot3_font_index[i+128] = plot3_font_index[i] = p;
	while ((*p++) != PLOT3_FONT_LAST)
	    ;
    }
    for (i = 1; i <= PLOT3_NUM_SYMBOLS; i++) {
	plot3_font_index[i+128] = plot3_font_index[i] = plot3_font_index[040-PLOT3_NUM_SYMBOLS-1+i];
    }
    for (i = PLOT3_NUM_SYMBOLS + 1; i < 040; i++) {
	plot3_font_index[i+128] = plot3_font_index[i] = plot3_font_index['?'];
    }
}

int *
plot3_font_getchar(const unsigned char *c)
{
    if (plot3_font_index[040] == 0)
	plot3_font_setup();
    return plot3_font_index[*c];
}

void
pl_list(FILE *fp, int *x, int *y, int npoints)
{
    if (npoints <= 0)
	return;

    pl_move(fp, *x++, *y++);
    while (--npoints > 0)
	pl_cont(fp, *x++, *y++);
}

void
pd_list(FILE *fp, double *x, double *y, int npoints)
{
    if (npoints <= 0)
	return;

    pd_move(fp, *x++, *y++);
    while (--npoints > 0)
	pd_cont(fp, *x++, *y++);
}

void
pd_3list(FILE *fp, double *x, double *y, double *z, int npoints)
{
    if (npoints <= 0)
	return;

    pd_3move(fp, *x++, *y++, *z++);
    while (--npoints > 0)
	pd_3cont(fp, *x++, *y++, *z++);
}

void
pd_marked_list(FILE *fp, double *x, double *y, int npoints, int flag, int mark, int interval, double size)
{
    int i;
    int counter;

    if (npoints <= 0)
	return;

    if (flag & PLOT3_LINE)
	pd_list(fp, x, y, npoints);
    if (flag & PLOT3_MARK) {
	pd_marker(fp, mark, *x++, *y++, size);
	counter = 1;
	for (i = 1; i < npoints; i++) {
	    if (counter >= interval) {
		pd_marker(fp, mark, *x, *y, size);
		counter = 0;
	    }
	    x++;
	    y++;
	    counter++;
	}
    }
}

void
pd_marker(FILE *fp, int c, double x, double y, double scale)
{
    char mark_str[4];

    mark_str[0] = (char)c;
    mark_str[1] = '\0';
    pd_symbol(fp, mark_str, x - scale*0.5, y - scale*0.5, scale, 0.0);
}

void
pd_3marker(FILE *fp, int c, double x, double y, double z, double scale)
{
    char mark_str[4];
    mat_t mat;
    vect_t p;

    mark_str[0] = (char)c;
    mark_str[1] = '\0';
    MAT_IDN(mat);
    VSET(p, x - scale*0.5, y - scale*0.5, z);
    pdv_3symbol(fp, mark_str, p, mat, scale);
}

void
pdv_3symbol(FILE *fp, char *string, point_t origin, mat_t rot, double scale)
{
    unsigned char *cp;
    double offset;
    int ysign;
    vect_t temp;
    vect_t loc;
    mat_t xlate_to_origin;
    mat_t mat;

    if (string == NULL || *string == '\0')
	return;

    MAT_IDN(xlate_to_origin);
    MAT_DELTAS_VEC(xlate_to_origin, origin);
    plot3_mat_mul(mat, xlate_to_origin, rot);

    offset = 0;
    for (cp = (unsigned char *)string; *cp; cp++, offset += scale) {
	int *p;
	int stroke;

	VSET(temp, offset, 0, 0);
	MAT4X3PNT(loc, mat, temp);
	pdv_3move(fp, loc);

	for (p = plot3_font_getchar(cp); (stroke = *p) != PLOT3_FONT_LAST; p++) {
	    int draw;

	    if (stroke == PLOT3_FONT_NEGY) {
		ysign = -1;
		stroke = *++p;
	    } else {
		ysign = 1;
	    }

	    if (stroke < 0) {
		stroke = -stroke;
		draw = 0;
	    } else {
		draw = 1;
	    }

	    VSET(temp, (stroke/11) * 0.1 * scale + offset,
		 ysign * (stroke%11) * 0.1 * scale, 0);
	    MAT4X3PNT(loc, mat, temp);
	    if (draw)
		pdv_3cont(fp, loc);
	    else
		pdv_3move(fp, loc);
	}
    }
}

void
pd_symbol(FILE *fp, char *string, double x, double y, double scale, double theta)
{
    mat_t mat;
    vect_t p;

    plot3_mat_zrot(mat, theta);
    VSET(p, x, y, 0);
    pdv_3symbol(fp, string, p, mat, scale);
}

void
pdv_3vector(FILE *plotfp, point_t from, point_t to, double fromheadfract, double toheadfract)
{
    fastf_t len;
    fastf_t hooklen;
    vect_t diff;
    vect_t c1, c2;
    vect_t h1, h2;
    vect_t backup;
    point_t tip;

    pdv_3line(plotfp, from, to);

    VSUB2(diff, to, from);
    if ((len = MAGNITUDE(diff)) < SQRT_SMALL_FASTF)
	return;
    VSCALE(diff, diff, 1/len);
    plot3_vec_ortho(c1, diff);
    VCROSS(c2, c1, diff);

    if (!ZERO(fromheadfract)) {
	hooklen = fromheadfract*len;
	VSCALE(backup, diff, -hooklen);

	VSCALE(h1, c1, hooklen);
	VADD3(tip, from, h1, backup);
	pdv_3move(plotfp, from);
	pdv_3cont(plotfp, tip);

	VSCALE(h2, c2, hooklen);
	VADD3(tip, from, h2, backup);
	pdv_3move(plotfp, tip);
    }
    if (!ZERO(toheadfract)) {
	hooklen = toheadfract*len;
	VSCALE(backup, diff, -hooklen);

	VSCALE(h1, c1, hooklen);
	VADD3(tip, to, h1, backup);
	pdv_3move(plotfp, to);
	pdv_3cont(plotfp, tip);

	VSCALE(h2, c2, hooklen);
	VADD3(tip, to, h2, backup);
	pdv_3move(plotfp, tip);
    }
    if (!ZERO(fromheadfract) || !ZERO(toheadfract))
	pdv_3cont(plotfp, to);
}

void
pdv_3axis(FILE *fp, char *string, point_t origin, mat_t rot, double length, int ccw, int ndigits, double label_start, double label_incr, double tick_separation, double char_width)
{
    int i;
    int nticks;
    point_t tick_bottom;
    vect_t axis_incr;
    vect_t axis_dir;
    point_t title_left;
    point_t cur_point;
    point_t num_start;
    point_t num_center;
    point_t num_last_end;
    vect_t temp;
    vect_t diff;
    mat_t xlate_to_0;
    mat_t mat;
    char fmt[32];
    char str[64];

    if (ccw)
	ccw = -1;
    else
	ccw = 1;

    if (ZERO(tick_separation))
	tick_separation = 1;

    MAT_IDN(xlate_to_0);
    MAT_DELTAS_VEC(xlate_to_0, origin);
    plot3_mat_mul(mat, rot, xlate_to_0);
    VMOVE(cur_point, origin);

    VSET(temp, 0, -PLOT3_TICK_YLEN * ccw, 0);
    MAT4X3PNT(tick_bottom, mat, temp);

    VSET(temp, 0, -PLOT3_NUM_YOFF * ccw, 0);
    MAT4X3PNT(num_center, mat, temp);
    temp[X] = -char_width*ndigits;
    MAT4X3PNT(num_last_end, mat, temp);

    VSET(temp, 1, 0, 0);
    MAT4X3VEC(axis_dir, mat, temp);
    VSCALE(axis_incr, axis_dir, tick_separation);

    VSET(temp, 0.5*(length - strlen(string)*char_width), -PLOT3_TITLE_YOFF*ccw, 0);
    MAT4X3PNT(title_left, mat, temp);
    pdv_3symbol(fp, string, title_left, rot, char_width);

    nticks = length/tick_separation+0.5;
    pdv_3move(fp, cur_point);
    for (i = 0; i <= nticks; i++) {
	pdv_3cont(fp, tick_bottom);

	if (ndigits > 0) {
	    double f;
	    if (ndigits > 63)
		ndigits = 63;
	    snprintf(fmt, 32, "%%%dg", ndigits);
	    snprintf(str, 64, fmt, label_start);
	    f = strlen(str) * char_width * 0.5;
	    VJOIN1(num_start, num_center, -f, axis_dir);

	    VSUB2(diff, num_start, num_last_end);
	    if (VDOT(diff, axis_dir) >= 0) {
		pdv_3symbol(fp, str, num_start, rot, char_width);
		VJOIN1(num_last_end, num_center, f, axis_dir);
	    }
	}

	if (i == nticks)
	    break;

	pdv_3move(fp, cur_point);
	VADD2(cur_point, cur_point, axis_incr);
	VADD2(tick_bottom, tick_bottom, axis_incr);
	VADD2(num_center, num_center, axis_incr);

	label_start += label_incr;
	pdv_3cont(fp, cur_point);
    }
}

void
plot3_float_split(float x, float *coef, int *ex)
{
    int i, isv;
    float xx;

    isv = 1;
    if (x < 0.0) {
	isv = -1;
	x = -x;
    }

    if (x > 1.0) {
	xx = x;
	*ex = 0;
	*coef = 0.0;

	if (xx < 10.0) {
	    *coef = xx*isv;
	    return;
	}

	for (i = 1; i < 39; ++i) {
	    *ex += 1;
	    xx = xx/10.0;
	    if (xx < 10.0)
		break;
	}
	*coef = xx*isv;
	return;
    }

    xx = x;
    *ex = 0;
    *coef = 0.0;
    for (i = 1; i < 39; ++i) {
	*ex -= 1;
	xx *= 10.0;
	if (xx >= 1.0)
	    break;
    }
    *coef = xx*isv;
}

double
plot3_ipow(double x, int n)
{
    return n > 0 ? x*plot3_ipow(x, n-1) : 1;
}

void
plot3_scale_fit(float *x, int npts, float size, float *xs, float *xmin, float *xmax, float *dx)
{
    float txmi, txma, coef, delta, diff;
    int i, ex;

    txmi = txma = x[0];
    i = 0;
    while (i <= npts) {
	V_MIN(txmi, x[i]);
	V_MAX(txma, x[i]);
	i++;
    }

    diff = txma - txmi;
    V_MAX(diff, 0.000001f);

    plot3_float_split(diff, &coef, &ex);
    if (coef < 2.0f)
	delta = .1f;
    else if (coef < 4.0f)
	delta = .2f;
    else
	delta = .5f;

    i = 0;
    if (ex < 0) {
	ex = -ex;
	i = 12;
    }

    delta *= plot3_ipow(10.0, ex);
    if (i == 12)
	delta = 1.0/delta;
    *dx = delta;

    i = (fabs(txmi)/delta);
    *xmin = i*delta;
    if (txmi < 0.0f)
	*xmin = -(*xmin+delta);

    i = (fabs(txma)/delta);
    *xmax = i*delta;
    if (txma < 0.0f)
	*xmax = -*xmax;
    else
	*xmax = *xmax+delta;
    *xs = 1000.0f*size/(*xmax - *xmin);
}

void
plot3_xy_plot(FILE *fp, int xp, int yp, int xl, int yl, char *xtitle, char *ytitle, float *x, float *y, int n, double cscale)
{
    int ddx = 0, ddy = 0, xend = 0, yend = 0, xpen = 0, ypen = 0;
    float fxl = 0.0, fyl = 0.0, xs = 0.0, ys = 0.0, xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0, dx = 0.0, dy = 0.0;
    float lab = 0.0;
    int xtics = 0, ytics = 0, i = 0, xtl = 0, ytl = 0, j = 0;
    int ix[101] = {0}, iy[101] = {0}, isave = 0;
    char str[32] = {0};

    if (xl == 0) {
	j = 0;
	goto loop;
    }
    fxl = xl/1000.0;
    fyl = yl/1000.0;
    n -= 1;
    plot3_scale_fit(x, n, fxl, &xs, &xmin, &xmax, &dx);
    plot3_scale_fit(y, n, fyl, &ys, &ymin, &ymax, &dy);
    ddx = dx*xs;
    ddy = dy*ys;
    xtics = PLOT3_LAB_LNGTH / ddx + 1.0;
    ytics = 500/ddy + 1.0;
    xend = xl+xp;
    xpen = xp;

    pl_move(fp, xpen, yp-PLOT3_TIC);
    pl_cont(fp, xpen, yp);

    lab = xmin;
    snprintf(str, 32, "%3.3g", xmin);
    pd_symbol(fp, str, (double)(xpen-171), (double)(yp-PLOT3_TIC-PLOT3_NUM_DISTANCE), cscale, 0.0);

    i = 0;
    while ((xpen+ddx) <= xend) {
	i++;
	xpen += ddx;
	pl_line(fp, xpen, yp, xpen, yp-PLOT3_TIC);
	lab += dx;
	if ((i%xtics) == 0) {
	    snprintf(str, 32, "%3.3g", lab);
	    pd_symbol(fp, str, (double)(xpen-171), (double)(yp-PLOT3_TIC-PLOT3_NUM_DISTANCE), cscale, 0.0);
	}
    }

    xtl = xp+(xl - strlen(xtitle)*cscale)/2;
    ytl = yp - 8 * cscale;
    pd_symbol(fp, xtitle, (double)xtl, (double)ytl, 100.0, 0.0);
    yend = yl+yp;
    ypen = yp;
    pl_line(fp, xp-PLOT3_TIC, ypen, xp, ypen);

    lab = ymin;
    snprintf(str, 32, "%3.3g", lab);
    pd_symbol(fp, str, (double)(xp-PLOT3_TIC-PLOT3_LAB_LNGTH-PLOT3_NUM_DISTANCE), (double)ypen, cscale, 0.0);

    i = 0;
    while ((ypen+ddy) <= yend) {
	i++;
	ypen += ddy;
	pl_line(fp, xp, ypen, xp-PLOT3_TIC, ypen);
	lab += dy;
	if ((i%ytics) == 0) {
	    snprintf(str, 32, "%3.3g", lab);
	    pd_symbol(fp, str, (double)(xp-PLOT3_TIC-PLOT3_LAB_LNGTH-PLOT3_NUM_DISTANCE), (double)ypen, cscale, 0.0);
	}
    }

    xtl = xp-1500;
    ytl = yp + (yl - strlen(ytitle)*cscale)/2;
    pd_symbol(fp, ytitle, (double)xtl, (double)ytl, 100.0, 90.0);

    j = 0;

loop:
    if (n <= 100) {
	isave = n-1;
    } else {
	isave = 100;
	n -= 101;
    }

    if (j == 0) {
	ix[0] = (x[j] - xmin)*xs + xp;
	iy[0] = (y[j] - ymin)*ys + yp;
	j++;
    } else {
	ix[0] = (x[j-1] - xmin)*xs + xp;
	iy[0] = (y[j-1] - ymin)*ys + yp;
    }

    i = 1;
    while (i <= isave) {
	ix[i] = (x[j] - xmin)*xs + xp;
	iy[i] = (y[j] - ymin)*ys + yp;
	i++;
	j++;
    }
    pl_list(fp, ix, iy, isave+1);
    if (isave == 100)
	goto loop;
}

void
plot3_float_to_ascii(float x, char *s)
{
    int ex, tmp;
    float coef;
    char esgn, nsgn;
    char i;

    plot3_float_split(x, &coef, &ex);
    if (ex < -15) {
	ex = 0;
	*s++ = '0';
	*s++ = '.';
	*s++ = '0';
	*s++ = '0';
	*s++ = '0';
	*s++ = 'e';
	*s++ = '+';
	*s++ = '0';
	*s++ = '0';
	*s = 0;
	return;
    }

    if (ex < 0) {
	esgn = '-';
	ex = -ex;
    } else {
	esgn = '+';
    }

    if (coef < 0.0) {
	nsgn = '-';
	coef = -coef;
    } else {
	nsgn = ' ';
    }
    *s++ = nsgn;

    tmp = coef;
    *s++ = tmp + '0';
    coef = (coef - tmp)*10.0;
    *s++ = '.';

    for (i = 1; i <= 3; ++i) {
	tmp = coef;
	coef = (coef - tmp)*10.0;
	*s++ = tmp + '0';
    }

    *s++ = 'e';
    *s++ = esgn;

    if (ex < 0)
	ex = -ex;

    if (ex < 10) {
	*s++ = '0';
	*s++ = ex + '0';
    } else {
	tmp = ex/10;
	*s++ = tmp + '0';
	ex = ex - tmp*10;
	*s++ = ex +'0';
    }
    *s = 0;
}

void
plot3_data_scale(int *idata, int elements, int mode, int length, int *odata, double *min, double *dx)
{
    double xmax, xmin, x, workdx;
    int i;
    static double log_10;
    float *ifloatp;
    double *idoublep;
    double fractional;
    int integral;

    ifloatp = (float *)idata;
    idoublep = (double *)idata;
    xmax = xmin = 0.0;
    for (i = 0; i < elements; i++) {
	x = (mode == 'f') ? ifloatp[i] : ((mode == 'd') ? idoublep[i] : idata[i]);
	V_MAX(xmax, x);
	V_MIN(xmin, x);
    }

    if (log_10 <= 0.0)
	log_10 = log(10.0);

    fractional = log((xmax-xmin)/length) / log_10;
    integral = fractional;
    fractional -= integral;

    if (fractional < 0.0) {
	fractional += 1.0;
	integral -= 1;
    }

    fractional = pow(10.0, fractional);
    i = fractional - 0.01;
    switch (i) {
	case 1:
	    fractional = 2.0;
	    break;
	case 2:
	case 3:
	    fractional = 4.0;
	    break;
	case 4:
	    fractional = 5.0;
	    break;
	case 5:
	case 6:
	case 7:
	    fractional = 8.0;
	    break;
	case 8:
	case 9:
	    fractional = 10.0;
    }

    workdx = pow(10.0, (double)integral) * fractional;

    for (i = 0; i < elements; i++) {
	if (mode == 'f')
	    odata[i] = (ifloatp[i] - xmin) / workdx;
	else if (mode == 'd')
	    odata[i] = (idoublep[i] - xmin) / workdx;
	else
	    odata[i] = (idata[i] - xmin) / workdx;
    }

    *min = xmin;
    *dx = workdx;
}


int
plot3_invalid(FILE *fp, int mode)
{

    /* Only two valid modes */
    if (mode != PL_OUTPUT_MODE_BINARY && mode != PL_OUTPUT_MODE_TEXT) {
	return 1;
    }

    /* A non-readable file isn't a valid file */
    if (!fp) {
	return 1;
    }

    int i = 0;
    int c;
    unsigned int tchar = 0;

    while (!feof(fp) && (c=getc(fp)) != EOF) {
	if (c < 'A' || c > 'z') {
	    return 1;
	}
	switch (c) {
	    case 'C':
		// TCHAR, 3, "color"
		if (mode == PL_OUTPUT_MODE_BINARY) {
		    for (i = 0; i < 3; i++) {
			if (getc(fp) == EOF)
			    return 1;
		    }
		}
		if (mode == PL_OUTPUT_MODE_TEXT) {
		    i = fscanf(fp, "%u", &tchar);
		    if (i != 1)
			return 1;
		}
		break;
	    case 'F':
		// TNONE, 0, "flush"
		break;
	    case 'L':
		// TSHORT, 6, "3line"
		if (read_short(fp, 6, mode))
		    return 1;
		break;
	    case 'M':
		// TSHORT, 3, "3move"
		if (read_short(fp, 3, mode))
		    return 1;
		break;
	    case 'N':
		// TSHORT, 3, "3cont"
		if (read_short(fp, 3, mode))
		    return 1;
		break;
	    case 'O':
		// TIEEE, 3, "d_3move"
		if (read_ieee(fp, 3, mode))
		    return 1;
		break;
	    case 'P':
		// TSHORT, 3, "3point"
		if (read_short(fp, 3, mode))
		    return 1;
		break;
	    case 'Q':
		// TIEEE, 3, "d_3cont"
		if (read_ieee(fp, 3, mode))
		    return 1;
		break;
	    case 'S':
		// TSHORT, 6, "3space"
		if (read_short(fp, 6, mode))
		    return 1;
			break;
	    case 'V':
		// TIEEE, 6, "d_3line"
		if (read_ieee(fp, 6, mode))
		    return 1;
		break;
	    case 'W':
		// TIEEE, 6, "d_3space"
		if (read_ieee(fp, 6, mode))
		    return 1;
		break;
	    case 'X':
		// TIEEE, 3, "d_3point"
		if (read_ieee(fp, 3, mode))
		    return 1;
		break;
	    case 'a':
		// TSHORT, 6, "arc"
		if (read_short(fp, 6, mode))
		    return 1;
			break;
	    case 'c':
		// TSHORT, 3, "circle"
			if (read_short(fp, 3, mode))
			    return 1;
			break;
	    case 'e':
		// TNONE, 0, "erase"
		break;
	    case 'f':
		// TSTRING, 1, "linmod"
		if (read_tstring(fp, mode))
		    return 1;
		break;
	    case 'i':
		// TIEEE, 3, "d_circle"
		if (read_ieee(fp, 3, mode))
		    return 1;
		break;
	    case 'l':
		// TSHORT, 4, "line"
		if (read_short(fp, 4, mode))
		    return 1;
			break;
	    case 'm':
		// TSHORT, 2, "move"
		if (read_short(fp, 2, mode))
		    return 1;
			break;
	    case 'n':
		// TSHORT, 2, "cont"
		if (read_short(fp, 2, mode))
		    return 1;
			break;
	    case 'o':
		// TIEEE, 2, "d_move"
		if (read_ieee(fp, 2, mode))
		    return 1;
		break;
	    case 'p':
		// TSHORT, 2, "point"
		if (read_short(fp, 2, mode))
		    return 1;
		break;
	    case 'q':
		// TIEEE, 2, "d_cont"
		if (read_ieee(fp, 2, mode))
		    return 1;
		break;
	    case 'r':
		// TIEEE, 6, "d_arc"
		if (read_ieee(fp, 6, mode))
		    return 1;
		break;
	    case 's':
		// TSHORT, 4, "space"
		if (read_short(fp, 4, mode))
		    return 1;
		break;
	    case 't':
		// TSTRING, 1, "label"
		if (read_tstring(fp, mode))
		    return 1;
		break;
	    case 'v':
		// TIEEE, 4, "d_line"
		if (read_ieee(fp, 4, mode))
		    return 1;
		break;
	    case 'w':
		// TIEEE, 4, "d_space"
		if (read_ieee(fp, 4, mode))
		    return 1;
		break;
	    case 'x':
		// TIEEE, 2, "d_point"
		if (read_ieee(fp, 2, mode))
		    return 1;
		break;
	    default:
		return 1;
		break;
	};
    }

    return 0;
}

#endif // PLOT3_IMPLEMENTATION

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
