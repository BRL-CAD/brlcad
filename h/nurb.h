/*	   N U R B . H
 *
 *  Function -
 *	Define surface and curve structures for
 * 	Non Rational Uniform B-Spline (NURB) 
 *	curves and surfaces. 
 *
 *  Author -
 *	Paul Randal Stay
 * 
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

#ifndef RAYTRACE_H
# include <stdio.h>
# include "raytrace.h"
#endif

#ifndef NURB_H
#define NURB_H seen

/* make sure all the prerequisite include files have been included
 */
#ifndef MACHINE_H
#include "machine.h"
#endif

#ifndef VMATH_H
#include "vmath.h"
#endif

/* Define parametric directions for splitting. */

#define ROW 0
#define COL 1
#define FLAT 2

/* Definition of NURB point types and coordinates 
 * Bit:	  8765 4321 0
 *       |nnnn|tttt|h
 *			h     - 1 if Homogeneous
 *			tttt  - point type
 *				1 = XY 
 *				2 = XYZ
 *				3 = UV
 *				4 = Random data
 *				5 = Projected surface
 *			nnnnn - number of coordinates per point
 *				includes the rational coordinate
 */

/* point types */
#define PT_XY 	1			/* x,y coordintes */
#define PT_XYZ	2			/* x,y,z coordinates */
#define PT_UV	3			/* trim u,v parameter space */
#define PT_DATA 4			/* random data */
#define PT_PROJ	5			/* Projected Surface */

#define RAT	1
#define NONRAT  0

#define MAKE_PT_TYPE(n,t,h)	((n<<5) | (t<<1) | h)
#define EXTRACT_COORDS(pt)	(pt>>5)
#define EXTRACT_PT_TYPE(pt)		((pt>>1) & 0x0f)
#define EXTRACT_RAT(pt)		(pt & 0x1)
#define STRIDE(pt)		(EXTRACT_COORDS(pt) * sizeof( fastf_t))

struct knot_vector {
	int k_size;			/* knot vector size */
	fastf_t * knots;		/* pointer to knot vector  */
};

struct c_mesh {
	int c_size;		/* number of ctl points */
	int pt_type;		/* curve point type */
	fastf_t * ctl_points;   /* floating point values from machine.h */
};

struct s_mesh {
	int s_size[2];		/* mesh size */
	int pt_type;		/* surface point type */
	fastf_t * ctl_points;   /* floating point values from machine.h */
};

struct cnurb {
	struct cnurb * next;		/* next curve in list */
	int order;			/* Curve Order */
	struct knot_vector * knot;	/* curve knot vector */
	struct c_mesh * mesh;		/* curve control polygon */
};

struct snurb {
	struct snurb * next;		/* next surface */
	struct cnurb * trim;		/* surface trimming curves */
	int order[2];			/* surface order [0] = u, [1] = v */
	short dir;			/* last direction of refinement */
	struct knot_vector * u_knots;	/* surface knot vectors */
	struct knot_vector * v_knots;	/* surface knot vectors */
	struct s_mesh * mesh;		/* surface control points */
};

struct rt_nurb_poly {
	struct rt_nurb_poly * next;
	point_t ply[3];			/* Vertices */
	fastf_t uv[3][2];		/* U,V parametric values */
};


struct oslo_mat {
	struct oslo_mat * next;
	int offset;
	int osize;
	fastf_t * o_vec;
};

#define EPSILON 0.0001
#define APX_EQ(x,y)    (fabs(x - y) < EPSILON)
#define MAX(i,j)    ( (i) > (j) ? (i) : (j) )
#define MIN(i,j)    ( (i) < (j) ? (i) : (j) )
#define SPL_INFINIT	(1.0e20)

struct knot_vector * rt_nurb_kvknot();
struct knot_vector * rt_nurb_kvmult();
struct knot_vector * rt_nurb_kvgen();
struct knot_vector * rt_nurb_kvmerge();
struct knot_vector * rt_nurb_kvextract();
struct knot_vector * rt_nurb_kvcopy();
int rt_nurb_kvcheck();
void rt_nurb_kvnorm();
int rt_knot_index();


fastf_t rt_nurb_basis_eval();
struct oslo_mat * rt_nurb_calc_oslo();
void rt_nurb_pr_oslo();
void rt_nurb_map_oslo();

#endif
