/*                         P L A N E . H
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup librt */
/*@{*/
/** @file plane.h
 *  This header file describes the plane_specific structure, which
 *  is for ARS processing, and the tri_specific structure,
 *  which is for ARB and PG processing.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */
/*@}*/

#ifndef PLANE_H
#define PLANE_H seen

#define MAXPTS	4			/* All we need are 4 points */
#define pl_A	pl_points[0]		/* Synonym for A point */

struct plane_specific  {
	int	pl_npts;		/* number of points on plane */
	point_t	pl_points[MAXPTS];	/* Actual points on plane */
	vect_t	pl_Xbasis;		/* X (B-A) vector (for 2d coords) */
	vect_t	pl_Ybasis;		/* Y (C-A) vector (for 2d coords) */
	vect_t	pl_N;			/* Unit-length Normal (outward) */
	fastf_t	pl_NdotA;		/* Normal dot A */
	fastf_t	pl_2d_x[MAXPTS];	/* X 2d-projection of points */
	fastf_t	pl_2d_y[MAXPTS];	/* Y 2d-projection of points */
	fastf_t	pl_2d_com[MAXPTS];	/* pre-computed common-term */
	struct plane_specific *pl_forw;	/* Forward link */
	char	pl_code[MAXPTS+1];	/* Face code string.  Decorative. */
};

/*
 *  Describe the tri_specific structure.
 */
struct tri_specific  {
	point_t	tri_A;			/* triangle vertex (A) */
	vect_t	tri_BA;			/* B - A (second point) */
	vect_t	tri_CA;			/* C - A (third point) */
	vect_t	tri_wn;			/* facet normal (non-unit) */
	vect_t	tri_N;			/* unit normal vector */
	fastf_t *tri_normals;		/* unit vertex normals A, B, C  (this is malloced storage) */
	int	tri_surfno;		/* solid specific surface number */
	struct tri_specific *tri_forw;	/* Next facet */
};

typedef struct tri_specific tri_specific_double;

/*
 *	A more memory conservative version
 */
struct tri_float_specific  {
	float	tri_A[3];			/* triangle vertex (A) */
	float	tri_BA[3];			/* B - A (second point) */
	float	tri_CA[3];			/* C - A (third point) */
	float	tri_wn[3];			/* facet normal (non-unit) */
	float	tri_N[3];			/* unit normal vector */
	signed char *tri_normals;		/* unit vertex normals A, B, C  (this is malloced storage) */
	int	tri_surfno;		/* solid specific surface number */
	struct tri_float_specific *tri_forw;	/* Next facet */
};

typedef struct tri_float_specific tri_specific_float;

#endif /* PLANE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
