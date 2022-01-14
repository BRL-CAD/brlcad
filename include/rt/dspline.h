/*                      D S P L I N E . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file dspline.h
 *
 */

#ifndef RT_DSPLINE_H
#define RT_DSPLINE_H

#include "common.h"

#include "vmath.h"

#include "rt/defines.h"

__BEGIN_DECLS

/**
 * Initialize a spline matrix for a particular spline type.
 */
RT_EXPORT extern void rt_dspline_matrix(mat_t m, const char *type,
					const double    tension,
					const double    bias);

/**
 * m            spline matrix (see rt_dspline4_matrix)
 * a, b, c, d   knot values
 * alpha:       0..1    interpolation point
 *
 * Evaluate a 1-dimensional spline at a point "alpha" on knot values
 * a, b, c, d.
 */
RT_EXPORT extern double rt_dspline4(mat_t m,
				    double a,
				    double b,
				    double c,
				    double d,
				    double alpha);

/**
 * pt           vector to receive the interpolation result
 * m            spline matrix (see rt_dspline4_matrix)
 * a, b, c, d   knot values
 * alpha:       0..1 interpolation point
 *
 * Evaluate a spline at a point "alpha" between knot pts b & c. The
 * knots and result are all vectors with "depth" values (length).
 *
 */
RT_EXPORT extern void rt_dspline4v(double *pt,
				   const mat_t m,
				   const double *a,
				   const double *b,
				   const double *c,
				   const double *d,
				   const int depth,
				   const double alpha);

/**
 * Interpolate n knot vectors over the range 0..1
 *
 * "knots" is an array of "n" knot vectors.  Each vector consists of
 * "depth" values.  They define an "n" dimensional surface which is
 * evaluated at the single point "alpha".  The evaluated point is
 * returned in "r"
 *
 * Example use:
 *
 * double result[MAX_DEPTH], knots[MAX_DEPTH*MAX_KNOTS];
 * mat_t m;
 * int knot_count, depth;
 *
 * knots = bu_malloc(sizeof(double) * knot_length * knot_count);
 * result = bu_malloc(sizeof(double) * knot_length);
 *
 * rt_dspline4_matrix(m, "Catmull", (double *)NULL, 0.0);
 *
 * for (i=0; i < knot_count; i++)
 *   get a knot(knots, i, knot_length);
 *
 * rt_dspline_n(result, m, knots, knot_count, knot_length, alpha);
 *
 */
RT_EXPORT extern void rt_dspline_n(double *r,
				   const mat_t m,
				   const double *knots,
				   const int n,
				   const int depth,
				   const double alpha);


__END_DECLS

#endif /* RT_DSPLINE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
