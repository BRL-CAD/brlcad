/*                         P L O T 3 . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @addtogroup plot
 *
 * @brief
 * The basic UNIX-plot routines.
 *
 * The calling sequence is the same as
 * the original Bell Labs routines, with the exception of the pl_
 * prefix on the name.
 *
 * Of interest:  the Plan 9 sources (recently MIT licensed) appear
 * to be related to the original code that would have formed the
 * conceptual basis for these routines:
 *
 * https://plan9.io/sources/plan9/sys/src/cmd/plot/libplot/
 *
 * Don't know if there would be any improvements that could be retrofitted
 * onto this version, but might be worth looking.  In particular, curious
 * if the spline routine might be useful...
 */
/** @{ */
/** @file plot3.h */

#ifndef BN_PLOT3_H
#define BN_PLOT3_H

#include "common.h"

#include "vmath.h"
#include "bu/defines.h"
#include "bu/color.h"
#include "bu/file.h"
#include "bn/defines.h"

__BEGIN_DECLS

#define	pl_mat_idn( _mat )		MAT_IDN( _mat )
#define pl_mat_zero( _mat )		MAT_ZERO( _mat )
#define pl_mat_copy( _mat1, _mat2 )	MAT_COPY( _mat1, _mat2 )

#define PL_OUTPUT_MODE_BINARY 0
#define PL_OUTPUT_MODE_TEXT 1


BN_EXPORT extern int pl_getOutputMode(void);
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
BN_EXPORT extern void pl_color_buc(FILE *plotfp,
			       struct bu_color *c);
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


BN_EXPORT extern int plot3_invalid(FILE *fp, int mode);

__END_DECLS

#endif /* BN_PLOT3_H */

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
