/*
 *			N M G _ I N T E R . C
 *
 *	Routines to intersect two NMG regions.
 *	There are no "user interface" routines in here.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/*
 *			R T _ P T 3 _ P T 3 _ E Q U A L
 *
 *  Returns -
 *	1	if the two points are equal, within the tolerance
 *	0	if the two points are not "the same"
 */
int
rt_pt3_pt3_equal( a, b, tol )
CONST point_t		a;
CONST point_t		b;
CONST struct rt_tol	*tol;
{
	vect_t	diff;

	RT_CK_TOL(tol);
	VSUB2( diff, b, a );
	if( MAGSQ( diff ) < tol->dist_sq )  return 1;
	return 0;
}

/* XXX move to plane.c */

/*
 *			R T _ I S E C T _ L I N E 2 _ L I N E 2
 *
 *  Intersect two lines, each in given in parametric form:
 *
 *	X = P + t * D
 *  and
 *	X = A + u * C
 *
 *  While the parametric form is usually used to denote a ray
 *  (ie, positive values of the parameter only), in this case
 *  the full line is considered.
 *
 *  The direction vectors C and D need not have unit length.
 *
 *  Explicit Return -
 *	-1	no intersection, lines are parallel.
 *	 0	lines are co-linear
 *			dist[0] gives distance from P to A,
 *			dist[1] gives distance from P to (A+C) [not same as below]
 *	 1	intersection found (t and u returned)
 *			dist[0] gives distance from P to isect,
 *			dist[1] gives distance from A to isect.
 *
 *  Implicit Returns -
 *	When explicit return > 0, dist[0] and dist[1] are the
 *	line parameters of the intersection point on the 2 rays.
 *	The actual intersection coordinates can be found by
 *	substituting either of these into the original ray equations.
 */
int
rt_isect_line2_line2( dist, p, d, a, c, tol )
fastf_t			*dist;			/* dist[2] */
CONST point_t		p;
CONST vect_t		d;
CONST point_t		a;
CONST vect_t		c;
CONST struct rt_tol	*tol;
{
	fastf_t			hx, hy;		/* A - P */
	register fastf_t	det;
	register fastf_t	det1;

	RT_CK_TOL(tol);
#	if DEBUG_2D_LINES
		rt_log("rt_isect_line_line2() p=(%g,%g), d=(%g,%g)\n\t\ta=(%g,%g), c=(%g,%g)\n",
			V2ARGS(p), V2ARGS(d), V2ARGS(a), V2ARGS(c) );
#	endif

	/*
	 *  From the two components q and r, form a system
	 *  of 2 equations in 2 unknowns.
	 *  Solve for t and u in the system:
	 *
	 *	Px + t * Dx = Ax + u * Cx
	 *	Py + t * Dy = Ay + u * Cy
	 *  or
	 *	t * Dx - u * Cx = Ax - Px
	 *	t * Dy - u * Cy = Ay - Py
	 *
	 *  Let H = A - P, resulting in:
	 *
	 *	t * Dx - u * Cx = Hx
	 *	t * Dy - u * Cy = Hy
	 *
	 *  or
	 *
	 *	[ Dx  -Cx ]   [ t ]   [ Hx ]
	 *	[         ] * [   ] = [    ]
	 *	[ Dy  -Cy ]   [ u ]   [ Hy ]
	 *
	 *  This system can be solved by direct substitution, or by
	 *  finding the determinants by Cramers rule:
	 *
	 *	             [ Dx  -Cx ]
	 *	det(M) = det [         ] = -Dx * Cy + Cx * Dy
	 *	             [ Dy  -Cy ]
	 *
	 *  If det(M) is zero, then the lines are parallel (perhaps colinear).
	 *  Otherwise, exactly one solution exists.
	 */
	det = c[X] * d[Y] - d[X] * c[Y];

	/*
	 *  det(M) is non-zero, so there is exactly one solution.
	 *  Using Cramer's rule, det1(M) replaces the first column
	 *  of M with the constant column vector, in this case H.
	 *  Similarly, det2(M) replaces the second column.
	 *  Computation of the determinant is done as before.
	 *
	 *  Now,
	 *
	 *	                  [ Hx  -Cx ]
	 *	              det [         ]
	 *	    det1(M)       [ Hy  -Cy ]   -Hx * Cy + Cx * Hy
	 *	t = ------- = --------------- = ------------------
	 *	     det(M)       det(M)        -Dx * Cy + Cx * Dy
	 *
	 *  and
	 *
	 *	                  [ Dx   Hx ]
	 *	              det [         ]
	 *	    det2(M)       [ Dy   Hy ]    Dx * Hy - Hx * Dy
	 *	u = ------- = --------------- = ------------------
	 *	     det(M)       det(M)        -Dx * Cy + Cx * Dy
	 */
	hx = a[X] - p[X];
	hy = a[Y] - p[Y];
	det1 = (c[X] * hy - hx * c[Y]);
	if( NEAR_ZERO( det, SQRT_SMALL_FASTF ) )  {
		/* Lines are parallel */
		if( !NEAR_ZERO( det1, SQRT_SMALL_FASTF ) )  {
			/* Lines are NOT co-linear, just parallel */
#			if DEBUG_2D_LINES
				rt_log("parallel, not co-linear\n");
#			endif
			return -1;	/* parallel, no intersection */
		}

		/*
		 *  Lines are co-linear.
		 *  Determine t as distance from P to A.
		 *  Determine u as distance from P to (A+C).  [special!]
		 *  Use largest direction component, for numeric stability
		 *  (and avoiding division by zero).
		 */
		if( fabs(d[X]) >= fabs(d[Y]) )  {
			dist[0] = hx/d[X];
			dist[1] = (hx + c[X]) / d[X];
		} else {
			dist[0] = hy/d[Y];
			dist[1] = (hy + c[Y]) / d[Y];
		}
#		if DEBUG_2D_LINES
			rt_log("colinear, t = %g, u = %g\n", dist[0], dist[1] );
#		endif
		return 0;	/* Lines co-linear */
	}
	det = 1/det;
	dist[0] = det * det1;
	dist[1] = det * (d[X] * hy - hx * d[Y]);
#	if DEBUG_2D_LINES
		rt_log("intersection, t = %g, u = %g\n", dist[0], dist[1] );
#	endif

	return 1;		/* Intersection found */
}

/*
 *			R T _ I S E C T _ L I N E 2 _ L S E G 2
 *
 *  Intersect a line in parametric form:
 *
 *	X = P + t * D
 *
 *  with a line segment defined by two distinct points A and B=(A+C).
 *
 *  Explicit Return -
 *	-4	A and B are not distinct points
 *	-3	Lines do not intersect
 *	-2	Intersection exists, but outside segemnt, < A
 *	-1	Intersection exists, but outside segment, > B
 *	 0	Lines are co-linear (special meaning of dist[1])
 *	 1	Intersection at vertex A
 *	 2	Intersection at vertex B (A+C)
 *	 3	Intersection between A and B
 *
 *  Implicit Returns -
 *	t	When explicit return >= 0, t is the parameter that describes
 *		the intersection of the line and the line segment.
 *		The actual intersection coordinates can be found by
 *		solving P + t * D.  However, note that for return codes
 *		1 and 2 (intersection exactly at a vertex), it is
 *		strongly recommended that the original values passed in
 *		A or B are used instead of solving P + t * D, to prevent
 *		numeric error from creeping into the position of
 *		the endpoints.
 */
int
rt_isect_line2_lseg2( dist, p, d, a, c, tol )
fastf_t			*dist;		/* dist[2] */
CONST point_t		p;
CONST vect_t		d;
CONST point_t		a;
CONST vect_t		c;
CONST struct rt_tol	*tol;
{
	register fastf_t f;
	fastf_t		fuzz;
	int		ret;

	RT_CK_TOL(tol);
	/*
	 *  To keep the values of u between 0 and 1,
	 *  C should NOT be scaled to have unit length.
	 *  However, it is a good idea to make sure that
	 *  C is a non-zero vector, (ie, that A and B are distinct).
	 */
	if( (fuzz = MAGSQ(c)) < tol->dist_sq )  {
		return -4;		/* points A and B are not distinct */
	}

	if( (ret = rt_isect_line2_line2( dist, p, d, a, c, tol )) < 0 )  {
		/* Lines are parallel, non-colinear */
		return -3;		/* No intersection found */
	}
	if( ret == 0 )  {
		fastf_t	ptol;
		/*  Lines are colinear */
		/*  If P within tol of either endpoint (0, 1), make exact. */
		ptol = tol->dist / sqrt( d[X]*d[X] + d[Y]*d[Y] );
		if( dist[0] > -ptol && dist[0] < ptol )  dist[0] = 0;
		else if( dist[0] > 1-ptol && dist[0] < 1+ptol ) dist[0] = 1;

		if( dist[1] > -ptol && dist[1] < ptol )  dist[1] = 0;
		else if( dist[1] > 1-ptol && dist[1] < 1+ptol ) dist[1] = 1;
		return 0;		/* Colinear */
	}

	/*
	 *  The two lines intersect at a point.
	 *  If the dist[1] parameter is outside the range (0..1),
	 *  reject the intersection, because it falls outside
	 *  the line segment A--B.
	 *
	 *  Convert the tol->dist into allowable deviation in terms of
	 *  (0..1) range of the parameters.
	 */
	fuzz = tol->dist / sqrt(fuzz);
	if( dist[1] < -fuzz )
		return -2;		/* Intersection < A */
	if( (f=(dist[1]-1)) > fuzz )
		return -1;		/* Intersection > B */

	/* Check for fuzzy intersection with one of the verticies */
	if( dist[1] < fuzz )  {
		dist[1] = 0;
		return 1;		/* Intersection at A */
	}
	if( f >= -fuzz )  {
		dist[1] = 1;
		return 2;		/* Intersection at B */
	}
	return 3;			/* Intersection between A and B */
}

/*
 *			R T _ I S E C T _ L S E G 2  _ L S E G 2
 *
 *  Intersect two 2D line segments, defined by two points and two vectors.
 *  The vectors are unlikely to be unit length.
 *
 *  Explicit Return -
 *	-2	missed (line segments are parallel)
 *	-1	missed (colinear and non-overlapping)
 *	 0	hit (line segments colinear and overlapping)
 *	 1	hit (normal intersection)
 *
 *  Implicit Return -
 *	The value at dist[] is set to the parametric distance of the intercept
 *	dist[0] is parameter along p, range 0 to 1, to intercept.
 *	dist[1] is parameter along q, range 0 to 1, to intercept.
 *	If within distance tolerance of the endpoints, these will be
 *	exactly 0.0 or 1.0, to ease the job of caller.
 *
 *  Special note:  when return code is "0" for co-linearity, dist[1] has
 *  an alternate interpretation:  it's the parameter along p (not q)
 *  which takes you from point p to the point (q + qdir), i.e., it's
 *  the endpoint of the q linesegment, since in this case there may be
 *  *two* intersections, if q is contained within span p to (p + pdir).
 *  And either may be -10 if the point is outside the span.
 */
int
rt_isect_lseg2_lseg2( dist, p, pdir, q, qdir, tol )
fastf_t		*dist;
CONST point_t	p;
CONST vect_t	pdir;
CONST point_t	q;
CONST vect_t	qdir;
struct rt_tol	*tol;
{
	fastf_t	dx, dy;
	fastf_t	det;		/* determinant */
	fastf_t	det1, det2;
	fastf_t	b,c;
	fastf_t	hx, hy;		/* H = Q - P */
	fastf_t	ptol, qtol;	/* length in parameter space == tol->dist */
	int	status;

	RT_CK_TOL(tol);
rt_log("rt_isect_lseg2_lseg2() p=(%g,%g), pdir=(%g,%g)\n\t\tq=(%g,%g), qdir=(%g,%g)\n",
V2ARGS(p), V2ARGS(pdir), V2ARGS(q), V2ARGS(qdir) );

	status = rt_isect_line2_line2( dist, p, pdir, q, qdir, tol );
	if( status < 0 )  {
		/* Lines are parallel, non-colinear */
		return -1;	/* No intersection */
	}
	if( status == 0 )  {
		int	nogood = 0;
		/* Lines are colinear */
		/*  If P within tol of either endpoint (0, 1), make exact. */
		ptol = tol->dist / sqrt( pdir[X]*pdir[X] + pdir[Y]*pdir[Y] );
rt_log("ptol=%g\n", ptol);
		if( dist[0] > -ptol && dist[0] < ptol )  dist[0] = 0;
		else if( dist[0] > 1-ptol && dist[0] < 1+ptol ) dist[0] = 1;

		if( dist[1] > -ptol && dist[1] < ptol )  dist[1] = 0;
		else if( dist[1] > 1-ptol && dist[1] < 1+ptol ) dist[1] = 1;

		if( dist[1] < 0 || dist[1] > 1 )  nogood = 1;
		if( dist[0] < 0 || dist[0] > 1 )  nogood++;
		if( nogood >= 2 )
			return -1;	/* colinear, but not overlapping */
rt_log("  HIT colinear!\n");
		return 0;		/* colinear and overlapping */
	}
	/* Lines intersect */
	/*  If within tolerance of an endpoint (0, 1), make exact. */
	ptol = tol->dist / sqrt( pdir[X]*pdir[X] + pdir[Y]*pdir[Y] );
	if( dist[0] > -ptol && dist[0] < ptol )  dist[0] = 0;
	else if( dist[0] > 1-ptol && dist[0] < 1+ptol ) dist[0] = 1;

	qtol = tol->dist / sqrt( qdir[X]*qdir[X] + qdir[Y]*qdir[Y] );
	if( dist[1] > -qtol && dist[1] < qtol )  dist[1] = 0;
	else if( dist[1] > 1-qtol && dist[1] < 1+qtol ) dist[1] = 1;

rt_log("ptol=%g, qtol=%g\n", ptol, qtol);
	if( dist[0] < 0 || dist[0] > 1 || dist[1] < 0 || dist[1] > 1 ) {
rt_log("  MISS\n");
		return -1;		/* missed */
	}
rt_log("  HIT!\n");
	return 1;			/* hit, normal intersection */
}

/* Was nmg_boolstruct, but that name has appeared in nmg.h */
struct nmg_inter_struct {
	long		magic;
	struct nmg_ptbl	*l1;		/* vertexuses on the line of */
	struct nmg_ptbl *l2;		/* intersection between planes */
	struct rt_tol	tol;
	point_t		pt;		/* 3D line of intersection */
	vect_t		dir;
	int		coplanar;
	fastf_t		*vert2d;	/* Array of 2d vertex projections [index] */
	int		maxindex;	/* size of vert2d[] */
	mat_t		proj;		/* Matrix to project onto XY plane */
};
#define NMG_INTER_STRUCT_MAGIC	0x99912120
#define NMG_CK_INTER_STRUCT(_p)	NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")

static CONST struct nmg_visit_handlers nmg_null_handlers;	/* null filled */

struct ee_2d_state {
	struct nmg_inter_struct	*is;
	struct edgeuse	*eu;
	point_t	start;
	point_t	end;
	vect_t	dir;
};

/*
 *			N M G _ G E T _ 2 D _ V E R T E X
 *
 *  A "lazy evaluator" to obtain the 2D projection of a vertex.
 *  The lazy approach is not a luxury, since new (3D) vertices are created
 *  as the edge/edge intersection proceeds, and their 2D coordinates may
 *  be needed later on in the calculation.
 *  The alternative would be to store the 2D projection each time a
 *  new vertex is created, but that is likely to be a lot of bothersome
 *  code, where one omission would be deadly.
 *
 *  The return is a 3-tuple, with the Z coordinate set to 0.0 for safety.
 *  This is especially useful when the projected value is printed using
 *  one of the 3D print routines.
 */
static void
nmg_get_2d_vertex( v2d, v, is, fu )
point_t		v2d;		/* a 3-tuple */
struct vertex	*v;
struct nmg_inter_struct	*is;
CONST struct faceuse	*fu;	/* for plane equation */
{
	register fastf_t	*pt2d;
	point_t			pt;
	struct vertex_g		*vg;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEX(v);

	/* If 2D preparations have not been made yet, do it now */
	if( !is->vert2d )  {
		nmg_isect2d_prep( is, fu->f_p );
	}

	if( !v->vg_p )  rt_bomb("nmg_get_2d_vertex:  vertex with no geometry!\n");
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);
	if( v->index >= is->maxindex )  rt_bomb("nmg_get_2d_vertex:  array overrun\n");
	pt2d = &is->vert2d[v->index*3];
	if( pt2d[2] == 0 )  {
		/* Flag set.  Conversion is done.  Been here before */
		v2d[0] = pt2d[0];
		v2d[1] = pt2d[1];
		v2d[2] = 0;
		return;
	}

	MAT4X3PNT( pt, is->proj, vg->coord );
	v2d[0] = pt2d[0] = pt[0];
	v2d[1] = pt2d[1] = pt[1];
	v2d[2] = pt2d[2] = 0;		/* flag */

	if( !NEAR_ZERO( pt[2], is->tol.dist ) )  {
		rt_log("nmg_get_2d_vertex ERROR %d (%g,%g,%g) becomes (%g,%g) %g != zero!\n",
			v->index, V3ARGS(vg->coord), V3ARGS(pt) );
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("%d (%g,%g,%g) becomes (%g,%g) %g\n",
			v->index, V3ARGS(vg->coord), V3ARGS(pt) );
	}
}

/*
 *			N M G _ I S E C T 2 D _ P R E P
 *
 *  To intersect two co-planar faces, project all vertices from those
 *  faces into 2D.
 *  At the moment, use a memory intensive strategy which allocates a
 *  (3d) point_t for each "index" item, and subscripts the resulting
 *  array by the vertices index number.
 *  Since additional vertices can be created as the intersection process
 *  operates, 2*maxindex items are originall allocated, as a (generous)
 *  upper bound on the amount of intersecting that might happen.
 *
 *  In the array, the third double of each projected vertex is set to -1 when
 *  that slot has not been filled yet, and 0 when it has been.
 */
void
nmg_isect2d_prep( is, f1 )
struct nmg_inter_struct	*is;
struct face		*f1;
{
	struct model	*m;
	struct face_g	*fg;
	vect_t		to;
	point_t		centroid;
	point_t		centroid_proj;
	register int	i;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACE(f1);
	fg = f1->fg_p;
	NMG_CK_FACE_G(fg);

	m = nmg_find_model( &f1->magic );

	is->maxindex = ( 2 * m->maxindex );
	is->vert2d = (fastf_t *)rt_malloc( 3 * is->maxindex * sizeof(fastf_t), "vert2d[]");

	/*
	 *  Rotate so that f1's N vector points up +Z.
	 *  This places all 2D calcuations in the XY plane.
	 *  Translate so that f1's centroid becomes the 2D origin.
	 *  Reasoning:  no vertex should be favored by putting it at
	 *  the origin.  The "desirable" floating point space in the
	 *  vicinity of the origin should be used to best advantage,
	 *  by centering calculations around it.
	 */
	VSET( to, 0, 0, 1 );
	mat_fromto( is->proj, fg->N, to );
	VADD2SCALE( centroid, fg->max_pt, fg->min_pt, 0.5 );
	MAT4X3PNT( centroid_proj, is->proj, centroid );
	centroid_proj[Z] = fg->N[3];	/* pull dist from origin off newZ */
	MAT_DELTAS_VEC_NEG( is->proj, centroid_proj );

	/* Clear out the 2D vertex array, setting flag in [2] to -1 */
	for( i = (3*is->maxindex)-1-2; i >= 0; i -= 3 )  {
		VSET( &is->vert2d[i], 0, 0, -1 );
	}
}

/* XXX Move to nmg_pr.c or nmg_misc.c */
/*
 *			N M G _ P R _ P T B L _ V E R T _ L I S T
 *
 *  Print a ptbl array as a vertex list.
 */
void
nmg_pr_ptbl_vert_list( str, tbl )
char		*str;
struct nmg_ptbl	*tbl;
{
	int			i;
	struct vertexuse	**vup;
	struct vertexuse	*vu;
	struct vertex		*v;
	struct vertex_g		*vg;

    	rt_log("nmg_pr_ptbl_vert_list(%s):\n", str);

	vup = (struct vertexuse **)tbl->buffer;
	for (i=0 ; i < tbl->end ; ++i) {
		vu = vup[i];
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		NMG_CK_VERTEX_G(vg);
		rt_log("%d\t%g, %g, %g\t", i, V3ARGS(vg->coord) );
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
			rt_log("EDGEUSE");
		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
			rt_log("LOOPUSE");
			if ((struct vertexuse *)vu->up.lu_p->down_hd.forw != vu) {
				rt_log("ERROR vertexuse's parent disowns us!\n");
				if (((struct vertexuse *)(vu->up.lu_p->lumate_p->down_hd.forw))->l.magic == NMG_VERTEXUSE_MAGIC)
					rt_bomb("lumate has vertexuse\n");
				else
					rt_bomb("lumate has garbage\n");
			}
		} else {
			rt_log("UNKNOWN");
		}
		rt_log("\tv=x%x\n", v);
	}
}

/*
 *			N M G _ F I N D _ V E R T E X U S E_ O N _ F A C E
 *
 *	Perform a topological check to
 *	determine if the given vertex can be found in the given faceuse.
 *	If it can, return a pointer to the vertexuse which was found in the
 *	faceuse.
 */
static struct vertexuse *
nmg_find_vertexuse_on_face(v, fu)
CONST struct vertex	*v;
CONST struct faceuse	*fu;
{
	struct vertexuse *vu;
	struct edgeuse *eu;
	struct loopuse *lu;

#define CKLU_FOR_FU(_lu, _fu, _vu) \
	if (*_lu->up.magic_p == NMG_FACEUSE_MAGIC && _lu->up.fu_p == _fu) \
		return(_vu)

	NMG_CK_VERTEX(v);

	for( RT_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
			eu = vu->up.eu_p;
			if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
				lu = eu->up.lu_p;
				CKLU_FOR_FU(lu, fu, vu);
			}
		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
			lu = vu->up.lu_p;
			CKLU_FOR_FU(lu, fu, vu);
		}
	}

	return((struct vertexuse *)NULL);
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ V E R T 2 P
 *
 *  Intersect an edge and a vertex known to lie in the same plane.
 *
 *  This is a separate routine because it's used more than once.
 *
 *  Returns -
 *	1	if the vertex and edge intersected and have been joined
 *	0	if there was no intersection
 */
static int
nmg_isect_edge2p_vert2p( is, eu, vu )
struct nmg_inter_struct *is;
struct edgeuse		*eu;
struct vertexuse	*vu;
{
	fastf_t		dist;
	int		status;
	point_t		endpt;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(vu);

	/* Perhaps a 2d formulation would be better? */
#if 0
	/* This needs to be done every pass:  it changes as edges are cut */
	nmg_get_2d_vertex( is->pt, eu->vu_p->v_p, is, fu );
	nmg_get_2d_vertex( endpt, eu->eumate_p->vu_p->v_p, is, fu );
	VSUB2( is->dir, endpt, is->pt );
	/* rt_isect_pt2_lseg2() */
#endif

	status = rt_isect_pt_lseg( &dist,
		eu->vu_p->v_p->vg_p->coord,
		eu->eumate_p->vu_p->v_p->vg_p->coord,
		vu->v_p->vg_p->coord, &is->tol );
	switch( status )  {
	default:
		break;
	case 1:
		/* pt is at start of edge */
		nmg_jv( vu->v_p, eu->vu_p->v_p );
		return;
	case 2:
		/* pt is at end of edge */
		nmg_jv( vu->v_p, eu->eumate_p->vu_p->v_p );
		return;
	case 3:
		/* pt is in interior of edge, break edge */
		(void)nmg_ebreak( vu->v_p, eu );
		return;
	}
}

/*
 *			N M G _ I S E C T _ V E R T 2 P _ F A C E 2 P
 *
 *  Handle the complete intersection of a vertex which lies on the
 *  plane of a face.
 *  If already part of the topology of the face, do nothing more.
 *  It it intersects one of the edges of the face, break the edge there.
 *  Otherwise, add a self-loop into the face as a marker.
 *
 *  Since this is an intersection of a vertex, no loop cut/join action
 *  is needed, since the only possible actions are fusing vertices
 *  and breaking edges into two edges about this vertex.
 *  This also means that no nmg_tbl( , TBL_INS_UNIQUE ) ops need be done.
 */
static void
nmg_isect_vert2p_face2p(is, vu, fu)
struct nmg_inter_struct *is;
struct vertexuse	*vu;
struct faceuse		*fu;
{
	struct loopuse	 *lu;
	pointp_t	pt;
	fastf_t		dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_vert2p_face2p(, vu=x%x, fu=x%x)\n", vu, fu);
	NMG_CK_INTER_STRUCT(is);
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_FACEUSE(fu);

	/* check the topology first */	
	if( nmg_find_vertexuse_on_face(vu->v_p, fu) )  return;

	/* topology didn't say anything, check with the geometry. */
	pt = vu->v_p->vg_p->coord;

	/* For every edge and vert in face, check geometric intersection */
	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		struct edgeuse	*eu;
		struct vertexuse *vu2;

		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu2 = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			if( vu->v_p == vu2->v_p )  return;
			/* Perhaps a 2d routine here? */
			if( rt_pt3_pt3_equal( pt, vu2->v_p->vg_p->coord, &is->tol ) )  {
				/* Fuse the two verts together */
				nmg_jv( vu->v_p, vu2->v_p );
				return;
			}
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			/* If there is an intersection, we are done */
			if( nmg_isect_edge2p_vert2p( is, eu, vu ) )  return;
		}
	}

	/* The vertex lies in the face, but touches nothing.  Place marker */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    	VPRINT("Making vertexloop", pt);

	lu = nmg_mlv(&fu->l.magic, vu->v_p, OT_UNSPEC);
	nmg_loop_g( lu->l_p );
}

/*
 *			N M G _ I S E C T _ 3 V E R T E X _ 3 F A C E
 *
 *	intersect a vertex with a face (primarily for intersecting
 *	loops of a single vertex with a face).
 */
static void
nmg_isect_3vertex_3face(bs, vu, fu)
struct nmg_inter_struct *bs;
struct vertexuse *vu;
struct faceuse *fu;
{
	struct loopuse *plu;
	struct vertexuse *vup;
	pointp_t pt;
	fastf_t dist;

	NMG_CK_INTER_STRUCT(bs);
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_FACEUSE(fu);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3vertex_3face(, vu=x%x, fu=x%x)\n", vu, fu);

	/* check the topology first */	
	if (vup=nmg_find_vertexuse_on_face(vu->v_p, fu)) {
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vup->l.magic);
		return;
	}


	/* since the topology didn't tell us anything, we need to check with
	 * the geometry
	 */
	pt = vu->v_p->vg_p->coord;
	dist = DIST_PT_PLANE(pt, fu->f_p->fg_p->N);

	if ( !NEAR_ZERO(dist, bs->tol.dist) )  return;

	/* The point lies on the plane of the face, by geometry. */

	/*
	 *  Intersect this point with all the edges of the face.
	 *  Note that no nmg_tbl() calls will be done during vert2p_face2p,
	 *  which is exactly what is needed here.
	 */
	nmg_isect_vert2p_face2p( bs, vu, fu );

	/* Re-check the topology */
	if (vup=nmg_find_vertexuse_on_face(vu->v_p, fu)) {
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vup->l.magic);
		return;
	}

	/* Make lone vertex-loop in fu to indicate sharing */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
	    	VPRINT("nmg_isect_3vertex_3face() making vertexloop", vu->v_p->vg_p->coord);

	plu = nmg_mlv(&fu->l.magic, vu->v_p, OT_UNSPEC);
	nmg_loop_g(plu->l_p);
	vup = RT_LIST_FIRST(vertexuse, &plu->down_hd);
	NMG_CK_VERTEXUSE(vup);
	(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &vu->l.magic);
    	(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vup->l.magic);
}

/*	M E G A _ C H E C K _ E B R E A K _ R E S U L T
 *
 *	Given all the trouble that nmg_ebreak() and nmg_esplit() were
 *	to develop, this routine exists to assist isect_edge_face() in
 *	checking the results of a call to these routines.
 */
static struct edgeuse *
mega_check_ebreak_result(eu)
struct edgeuse *eu;
{
	struct edgeuse *euforw;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);

	/* since we just split eu, the "next" edgeuse
	 * from eu CAN'T (in a working [as opposed to broken]
	 * system) be the list head.
	 */
	euforw = RT_LIST_PNEXT(edgeuse, eu);

	NMG_CK_EDGEUSE(euforw);
	NMG_CK_EDGEUSE(euforw->eumate_p);

	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEXUSE(euforw->vu_p);
	NMG_CK_VERTEXUSE(euforw->eumate_p->vu_p);

	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX(euforw->vu_p->v_p);
	NMG_CK_VERTEX(euforw->eumate_p->vu_p->v_p);

	nmg_ck_lueu(eu->up.lu_p, "isect_edge_face" );

	if (euforw->vu_p->v_p != eu->eumate_p->vu_p->v_p)
		rt_bomb("I was supposed to share verticies!\n");

	/* Make sure there is no geometry at the place we're about
	 * to stick the new geometry
	 */
	if (eu->eumate_p->vu_p->v_p->vg_p != (struct vertex_g *)NULL) {
		VPRINT("where'd this geometry come from?",
			eu->eumate_p->vu_p->v_p->vg_p->coord);
		rt_bomb("I didn't order this\n");
	}

	return(euforw);
}

/*
 *			N M G _ B R E A K _ 3 E D G E _ A T _ P L A N E
 *
 *	Having decided that an edge(use) crosses a plane of intersection,
 *	stick a vertex at the point of intersection along the edge.
 */
static void
nmg_break_3edge_at_plane(hit_pt, fu, bs, eu, v1, v1mate)
point_t	hit_pt;
struct faceuse *fu;
struct nmg_inter_struct *bs;
struct edgeuse *eu;
struct vertex	*v1;
struct vertex	*v1mate;
{
	struct vertexuse *vu_other;
	struct loopuse	*plu;
	struct edgeuse *euforw;

	NMG_CK_INTER_STRUCT(bs);

	/* Intersection is between first and second vertex points.
	 * Insert new vertex at intersection point.
	 */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("Splitting %g, %g, %g <-> %g, %g, %g\n",
			V3ARGS(v1->vg_p->coord), V3ARGS(v1mate->vg_p->coord) );
		VPRINT("\tPoint of intersection", hit_pt);
	}

	/* if we can't find the appropriate vertex in the
	 * other face, we'll build a new vertex.  Otherwise
	 * we re-use an old one.
	 */
	vu_other = nmg_find_vu_in_face(hit_pt, fu, &(bs->tol));
	if (vu_other) {
		/* the other face has a convenient vertex for us */

		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("re-using vertex from other face\n");

		(void)nmg_ebreak(vu_other->v_p, eu);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
	} else {
		/* The other face has no vertex in this vicinity */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("Making new vertex\n");

		(void)nmg_ebreak((struct vertex *)NULL, eu);

		(void)mega_check_ebreak_result(eu);
		euforw = RT_LIST_PNEXT(edgeuse, eu);

		if (euforw->vu_p->v_p != eu->eumate_p->vu_p->v_p)
			rt_bomb("I thought you said I was sharing verticies!\n");

		nmg_vertex_gv(eu->eumate_p->vu_p->v_p, hit_pt);

		NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(euforw->vu_p->v_p->vg_p);
		NMG_CK_VERTEX_G(euforw->eumate_p->vu_p->v_p->vg_p);

		if (euforw->vu_p->v_p != eu->eumate_p->vu_p->v_p)
			rt_bomb("I thought I was sharing verticies!\n");

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t p1 = eu->vu_p->v_p->vg_p->coord;
			register pointp_t p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;

			rt_log("Just split %g, %g, %g -> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
			p1 = euforw->vu_p->v_p->vg_p->coord;
			p2 = euforw->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("\t\t\t%g, %g, %g -> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
		}

		/* stick this vertex in the other shell
		 * and make sure it is in the other shell's
		 * list of vertices on the intersect line
		 */
		plu = nmg_mlv(&fu->l.magic, eu->eumate_p->vu_p->v_p, OT_UNSPEC);
		vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
		NMG_CK_VERTEXUSE(vu_other);
		nmg_loop_g(plu->l_p);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		    	VPRINT("Making vertexloop",
				vu_other->v_p->vg_p->coord);
			if (RT_LIST_FIRST_MAGIC(&plu->down_hd) !=
				NMG_VERTEXUSE_MAGIC)
				rt_bomb("bad plu\n");
			if (RT_LIST_FIRST_MAGIC(&plu->lumate_p->down_hd) !=
				NMG_VERTEXUSE_MAGIC)
				rt_bomb("bad plumate\n");
		}
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
	}

	euforw = RT_LIST_PNEXT_CIRC(edgeuse, eu);
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		register pointp_t	p1, p2;
		p1 = eu->vu_p->v_p->vg_p->coord;
		p2 = eu->eumate_p->vu_p->v_p->vg_p->coord;
		rt_log("\tNow %g, %g, %g <-> %g, %g, %g\n",
			V3ARGS(p1), V3ARGS(p2) );
		p1 = euforw->vu_p->v_p->vg_p->coord;
		p2 = euforw->eumate_p->vu_p->v_p->vg_p->coord;
		rt_log("\tand %g, %g, %g <-> %g, %g, %g\n\n",
			V3ARGS(p1), V3ARGS(p2) );
	}
	(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &euforw->vu_p->l.magic);
}

/*
 *  For the moment, a quick hack to see if breaking an edge at a given
 *  vertex results in a null edge being created.
 */
static void
nmg_ck_face_worthless_edges( fu )
CONST struct faceuse	*fu;
{
	CONST struct loopuse	*lu;

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		struct edgeuse	*eu;
		struct vertexuse *vu2;

		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			continue;
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			struct edgeuse		*neu;
			neu = RT_LIST_PNEXT_CIRC( edgeuse, eu );
			if( eu->vu_p == neu->vu_p )
				rt_bomb("edge runs between two copies of vu??\n");
			if( eu->vu_p->v_p == neu->vu_p->v_p )  {
#if 0
				nmg_pr_eu( eu, NULL );
				nmg_pr_eu( neu, NULL );
#endif
				rt_log("eu=x%x, neu=x%x, v=x%x\n", eu, neu, eu->vu_p->v_p);
				rt_log("eu=x%x, neu=x%x, v=x%x\n", eu->eumate_p, neu->eumate_p, eu->eumate_p->vu_p->v_p);
				rt_bomb("edge runs from&to same vertex\n");
			}
		}
	}
}

/*
 *			N M G _ I S E C T _ T W O _ C O L I N E A R _ E D G E 2 P
 *
 *  Two colinear line segments (eu1 and eu2, or just "1" and "2" in the
 *  diagram) can overlap each other in one of 8 configurations,
 *  labeled A through H:
 *
 *	A	B	C	D	E	F	G	H
 *
 *  vu1b,vu2b
 *	*	*	  *	  *	*	  *	*	  *
 *	1	1	  2	  2	1	  2	1	  2
 *	1=*	1	  2	*=2	1=*	*=2	*	  *
 *	1 2	*=*	*=*	1 2	1 2	1 2
 *	1 2	  2	1	1 2	1 2	1 2	  *	*
 *	1=*	  2	1	*=2	*=2	1=*	  2	1
 *	1	  *	*	  2	  2	1	  *	*
 *	*			  *	  *	*
 *   vu1a,vu2a
 *
 *  dist[0] has the distance (0..1) along eu1 from vu1a to vu2a.
 *  dist[1] has the distance (0..1) along eu1 from vua1 to vu2b.
 *
 *  As a consequence of this, conditions D, E, F can not be
 *  completely processed by just one call.
 *  If the caller computes a second array where the sense of eu1 and eu2
 *  are exchanged, and calls again, then the full intersection 
 *  be achieved.
 */
void
nmg_isect_two_colinear_edge2p( dist, l1, l2, vu1a, vu1b, vu2a, vu2b, eu1, eu2, fu1, fu2, str )
CONST fastf_t		dist[2];
struct nmg_ptbl		*l1;
struct nmg_ptbl		*l2;
struct vertexuse	*vu1a;
struct vertexuse	*vu1b;
struct vertexuse	*vu2a;
struct vertexuse	*vu2b;
struct edgeuse		*eu1;
struct edgeuse		*eu2;
struct faceuse		*fu1;		/* fu of eu1, for plane equation */
struct faceuse		*fu2;		/* fu of eu2, for error checks */
CONST char		*str;
{
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);
	NMG_CK_VERTEXUSE(vu2a);
	NMG_CK_VERTEXUSE(vu2b);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_two_colinear_edge2p(x%x, x%x) %s\n", eu1, eu2, str);
	nmg_ck_face_worthless_edges( fu1 );
	nmg_ck_face_worthless_edges( fu2 );

	/* First intersection point:  break eu1 */
	if( dist[0] == 0 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu1a intersects vu2a\n");
		(void)nmg_tbl(l1, TBL_INS_UNIQUE, &vu1a->l.magic);
		(void)nmg_tbl(l2, TBL_INS_UNIQUE, &vu2a->l.magic);
		nmg_jv(vu1a->v_p, vu2a->v_p);
	} else if( dist[0] == 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu1a intersects vu2b\n");
		(void)nmg_tbl(l1, TBL_INS_UNIQUE, &vu1a->l.magic);
		(void)nmg_tbl(l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_jv(vu1a->v_p, vu2b->v_p);
	} else if( dist[0] > 0 && dist[0] < 1 )  {
		/* Break eu1 into two pieces */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu2a=x%x breaks eu1=x%x\n", vu2a, eu1 );
		(void)nmg_ebreak( vu2a->v_p, eu1 );
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	}

	/* Second intersection point: break eu1 again */
	if( dist[1] == 0 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu1a intersects vu2b\n");
		(void)nmg_tbl(l1, TBL_INS_UNIQUE, &vu1a->l.magic);
		(void)nmg_tbl(l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_jv(vu1a->v_p, vu2b->v_p);
	} else if( dist[1] == 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu1b intersects vu2b\n");
		(void)nmg_tbl(l1, TBL_INS_UNIQUE, &vu1b->l.magic);
		(void)nmg_tbl(l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_jv(vu1b->v_p, vu2b->v_p);
	} else if( dist[1] > 0 && dist[1] < 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu2b=x%x breaks eu1=x%x\n", vu2b, eu1 );
		(void)nmg_ebreak( vu2b->v_p, eu1 );
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	}
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ E D G E 2 P
 *
 *  Actual 2d edge/edge intersector
 */
void
nmg_isect_edge2p_edge2p( is, eu1, eu2, fu1, fu2 )
struct nmg_inter_struct	*is;
struct edgeuse		*eu1;
struct edgeuse		*eu2;
struct faceuse		*fu1;		/* fu of eu1, for plane equation */
struct faceuse		*fu2;		/* fu of eu2, for error checks */
{
	point_t		eu1_start;
	point_t		eu1_end;
	vect_t		eu1_dir;
	point_t		eu2_start;
	point_t		eu2_end;
	vect_t		eu2_dir;
	vect_t	dir;
	fastf_t	dist[2];
	int	status;
	point_t		hit_pt;
	struct vertexuse	*vu;
	struct vertexuse	*vu1a, *vu1b;
	struct vertexuse	*vu2a, *vu2b;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	/*
	 * Important note:  don't use eu1->eumate_p->vu_p here,
	 * because that vu is in the opposite orientation faceuse.
	 * Putting those vu's on the intersection line makes for big trouble.
	 */
	vu1a = eu1->vu_p;
	vu1b = RT_LIST_PNEXT_CIRC( edgeuse, eu1 )->vu_p;
	vu2a = eu2->vu_p;
	vu2b = RT_LIST_PNEXT_CIRC( edgeuse, eu2 )->vu_p;
	NMG_CK_VERTEXUSE(vu1a);
	NMG_CK_VERTEXUSE(vu1b);
	NMG_CK_VERTEXUSE(vu2a);
	NMG_CK_VERTEXUSE(vu2b);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge2p_edge2p(eu1=x%x, eu2=x%x)\n\tvu1a=%x vu1b=%x, vu2a=%x vu2b=%x\n\tv1a=%x v1b=%x,   v2a=%x v2b=%x\n",
			eu1, eu2,
			vu1a, vu1b, vu2a, vu2b,
			vu1a->v_p, vu1b->v_p, vu2a->v_p, vu2b->v_p );

	/* First, a topology check. */
	if( vu1a->v_p == vu2a->v_p || vu1a->v_p == vu2b->v_p || vu2a->v_p == vu1b->v_p || vu2b->v_p == vu1b->v_p )  {
		/* These edges intersect, topologically.  No work. */
		/* XXX True if both endpoints match.
		 * XXX True if not co-linear.
		 * XXX what about co-linear case, with only one matching
		 * XXX endpoint to start with?
		 */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("edge2p_edge2p: shared topology x%x--x%x, x%x--x%x\n", vu1a->v_p, vu1b->v_p, vu2a->v_p, vu2b->v_p);
		goto topo;
	}

	/* This needs to be done every pass:  it changes as edges are cut */
	VMOVE( is->pt, vu1a->v_p->vg_p->coord );		/* 3D line */
	VSUB2( is->dir, vu1b->v_p->vg_p->coord, is->pt );

	nmg_get_2d_vertex( eu1_start, vu1a->v_p, is, fu1 );	/* 2D line */
	nmg_get_2d_vertex( eu1_end, vu1b->v_p, is, fu1 );
	VSUB2( eu1_dir, eu1_end, eu1_start );

	nmg_get_2d_vertex( eu2_start, vu2a->v_p, is, fu2 );
	nmg_get_2d_vertex( eu2_end, vu2b->v_p, is, fu2 );
	VSUB2( eu2_dir, eu2_end, eu2_start );

	dist[0] = dist[1] = 0;	/* for clean prints, below */
#define PROPER_2D	0
#if PROPER_2D
	/* The "proper" thing to do is intersect two line segments.
	 * However, this means that none of the intersections of edge "line"
	 * with the exterior of the loop are computed, and that
	 * violates the strategy assumptions of the face-cutter.
	 */
	status = rt_isect_lseg2_lseg2(&dist[0], eu1_start, eu1_dir,
			eu2_start, eu2_dir, &is->tol );
#else
	/* To pick up ALL intersection points, the source edge is a line */
	status = rt_isect_line2_lseg2( dist, eu1_start, eu1_dir,
			eu2_start, eu2_dir, &is->tol );
#endif
	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\trt_isect_lseg2_lseg2()=%d, dist: %g, %g\n",
			status, dist[0], dist[1] );
	}
	if (status < 0)  return;	/* No geometric intersection */

	if( status == 0 )  {
		fastf_t	eu2dist[2];
		fastf_t	ptol;

		/*
		 *  Special case of colinear overlapping edges.
		 *  There may be 2 intersect points.
		 *  dist[1] has special meaning for this return status:
		 *  it's distance w.r.t. eu1's 1st point (vu1a), not eu2's 1st.
		 *  Find break points on eu1 caused by vu2[ab].
		 */
		nmg_isect_two_colinear_edge2p( dist, is->l1, is->l2,
			vu1a, vu1b, vu2a, vu2b,
			eu1, eu2, fu1, fu2, "eu1/eu2" );

		/*
		 *  If the segments only partially overlap, need to intersect
		 *  the other way as well.
		 *  Find break points on eu2 caused by vu1[ab].
		 */
		if( fabs(eu2_dir[X]) >= fabs(eu2_dir[Y]) )  {
			eu2dist[0] = (eu1_start[X] - eu2_start[X])/eu2_dir[X];
			eu2dist[1] = (eu1_start[X] + eu1_dir[X] - eu2_start[X])/eu2_dir[X];
		} else {
			eu2dist[0] = (eu1_start[Y] - eu2_start[Y])/eu2_dir[Y];
			eu2dist[1] = (eu1_start[Y] + eu1_dir[Y] - eu2_start[Y])/eu2_dir[Y];
		}
		/* Tolerance processing */
		ptol = is->tol.dist / sqrt( eu2_dir[X]*eu2_dir[X] + eu2_dir[Y]*eu2_dir[Y] );
		if( eu2dist[0] > -ptol && eu2dist[0] < ptol )  eu2dist[0] = 0;
		else if( eu2dist[0] > 1-ptol && eu2dist[0] < 1+ptol ) eu2dist[0] = 1;

		if( eu2dist[1] > -ptol && eu2dist[1] < ptol )  eu2dist[1] = 0;
		else if( eu2dist[1] > 1-ptol && eu2dist[1] < 1+ptol ) eu2dist[1] = 1;

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("\trt_isect_lseg2_lseg2()=%d, eu2dist: %g, %g\n",
				status, eu2dist[0], eu2dist[1] );
rt_log("ptol = %g, eu2dist=%g, %g\n", ptol, eu2dist[0], eu2dist[1]);
		}

		nmg_isect_two_colinear_edge2p( eu2dist, is->l2, is->l1,
			vu2a, vu2b, vu1a, vu1b,
			eu2, eu1, fu2, fu1, "eu2/eu1" );

		return;
	}

	/* There is only one intersect point.  Break one or both edges. */


	/* The ray defined by the edgeuse intersects the eu2.
	 * Tolerances have already been factored in.
	 * The edge exists over values of 0 <= dist <= 1.
	 */
	VJOIN1( hit_pt, is->pt, dist[0], is->dir );	/* 3d */

	if ( dist[0] == 0 )  {
		/* First point of eu1 is on eu2, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu=x%x vu1a is intersect point\n", vu1a);
		if( dist[1] < 0 || dist[1] > 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\teu1 line intersects eu2 outside vu2a...vu2b range, ignore.\n");
			return;
		}
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1a->l.magic);

		/* Edges not colinear. Either join up with a matching vertex,
		 * or break eu2 on our vert.
		 */
		if( dist[1] == 0 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tvu2a matches vu1a\n");
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
			nmg_jv(vu1a->v_p, vu2a->v_p);
			return;
		}
		if( dist[1] == 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tsecond point of eu2 matches vu1a\n");
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
			nmg_jv(vu1a->v_p, vu2b->v_p);
			return;
		}
		/* Break eu2 on our first vertex */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tbreaking eu2 on vu1a\n");
		(void)nmg_ebreak( vu1a->v_p, eu2 );
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		return;
	}

	if ( dist[0] == 1 )  {
		/* Second point of eu1 is on eu2, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tvu=x%x vu1b is intersect point\n", vu1b);
		if( dist[1] < 0 || dist[1] > 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\teu1 line intersects eu2 outside vu2a...vu2b range, ignore.\n");
			return;
		}
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1b->l.magic);

		/* Edges not colinear. Either join up with a matching vertex,
		 * or break eu2 on our vert.
		 */
		if( dist[1] == 0 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tvu2a matches vu1b\n");
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
			nmg_jv(vu1b->v_p, vu2a->v_p);
			return;
		}
		if( dist[1] == 1 )  {
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
				rt_log("\tsecond point of eu2 matches vu1b\n");
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
			nmg_jv(vu1b->v_p, vu2b->v_p);
			return;
		}
		/* Break eu2 on our second vertex */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tbreaking eu2 on vu1b\n");
		(void)nmg_ebreak( vu1b->v_p, eu2 );
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
		return;
	}

	/* eu2 intersect point is on eu1 line, but not between vertices */
	if( dist[0] < 0 || dist[0] > 1 )  {
		struct loopuse *plu;
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tIntersect point on eu2 is outside vu1a...vu1b.  Break eu2 anyway.\n");
#if !PROPER_2D
		if( dist[1] == 0 )  {
			if( vu = nmg_find_vertexuse_on_face( vu2a->v_p, fu1 ) )  {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("\t\tIntersect point is vu2a\n");
			} else {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("\t\tIntersect point is vu2a, make self-loop in fu1\n");
				plu = nmg_mlv(&fu1->l.magic, vu2a->v_p, OT_UNSPEC);
				nmg_loop_g(plu->l_p);
				vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			}
			NMG_CK_VERTEXUSE(vu);
			(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu->l.magic);
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
			return;
		} else if( dist[1] == 1 )  {
			if( vu = nmg_find_vertexuse_on_face( vu2b->v_p, fu1 ) )  {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("\t\tIntersect point is vu2b\n");
			} else {
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("\t\tIntersect point is vu2b, make self-loop in fu1\n");
				plu = nmg_mlv(&fu1->l.magic, vu2b->v_p, OT_UNSPEC);
				nmg_loop_g(plu->l_p);
				vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			}
			NMG_CK_VERTEXUSE(vu);
			(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu->l.magic);
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
			return;
		} else if( dist[1] > 0 && dist[1] < 1 )  {
			/* Break eu2 somewhere in the middle */
			struct vertexuse	*new_vu2;
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("\t\tBreaking eu2 at intersect point", hit_pt);
			new_vu2 = nmg_ebreak( NULL, eu2 )->vu_p;
			nmg_vertex_gv( new_vu2->v_p, hit_pt );	/* 3d geom */

			plu = nmg_mlv(&fu1->l.magic, new_vu2->v_p, OT_UNSPEC);
			nmg_loop_g(plu->l_p);
			vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu);

			(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu->l.magic);
			(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &new_vu2->l.magic);
			nmg_ck_face_worthless_edges( fu1 );
			nmg_ck_face_worthless_edges( fu2 );
			return;
		}
#endif
		/* Ray misses eu2, nothing to do */
		return;
	}

	/* Intersection is in the middle of the reference edge */
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tintersect is in middle of eu1, breaking it\n");

	/* Edges not colinear. Either join up with a matching vertex,
	 * or break eu2 on our vert.
	 */
	if( dist[1] == 0 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tintersect point is vu2a\n");
		nmg_ebreak( vu2a->v_p, eu1 );
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1b->l.magic);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	} else if( dist[1] == 1 )  {
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\t\tintersect point is vu2b\n");
		nmg_ebreak( vu2b->v_p, eu1 );
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1b->l.magic);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	} else {
		/* Intersection is in the middle of both, split edge */
		struct vertex	*new_v;
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
		    	VPRINT("\t\tBreaking both edges at intersect point", hit_pt);
		new_v = nmg_e2break( eu1, eu2 );
		nmg_vertex_gv( new_v, hit_pt );	/* 3d geometry */

		/* new_v is at far end of eu1 */
		if( eu1->eumate_p->vu_p->v_p != new_v ) rt_bomb("new_v 1\n");
		/* Can't use eumate_p here, it's in wrong orientation face */
		(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &RT_LIST_PNEXT_CIRC(edgeuse,eu1)->vu_p->l.magic);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &RT_LIST_PNEXT_CIRC(edgeuse,eu2)->vu_p->l.magic);
		nmg_ck_face_worthless_edges( fu1 );
		nmg_ck_face_worthless_edges( fu2 );
	}

	/*
	 *  Now that processing is done and edges may have been shortened,
	 *  enrole any shared topology (representing edge intersections)
	 *  in the list of vu's encountered along this edge.
	 *  Note that either (or both) vu's may be shared in the case
	 *  of co-linear edges.
	 *  Can't use eumate_p here, it's in wrong orientation faceuse.
	 */
topo:
	vu1a = eu1->vu_p;
	vu1b = RT_LIST_PNEXT_CIRC(edgeuse,eu1)->vu_p;
	vu2a = eu2->vu_p;
	vu2b = RT_LIST_PNEXT_CIRC(edgeuse,eu2)->vu_p;
	/*
	 *  vu1a and vu1b and their duals MUST be listed on
	 *  the intersection line.
	 *  Given that the edges are coplanar, if each vu does not
	 *  have a (dual) reference in the other face, then we need to
	 *  create a self-loop vu over there.
	 */
	(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1a->l.magic);
	(void)nmg_tbl(is->l1, TBL_INS_UNIQUE, &vu1b->l.magic);

	/* Find dual of vu1a in fu2 */
	if( vu1a->v_p == vu2a->v_p )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
	} else if( vu1a->v_p == vu2b->v_p )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
	} else if( vu=nmg_find_vertexuse_on_face(vu1a->v_p, fu2) )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu->l.magic);
	} else {
		struct loopuse *plu;
		plu = nmg_mlv(&fu2->l.magic, vu1a->v_p, OT_UNSPEC);
		nmg_loop_g(plu->l_p);
		vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu->l.magic);
	}

	/* Find dual of vu1b in fu2 */
	if( vu1b->v_p == vu2a->v_p )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2a->l.magic);
	} else if( vu1b->v_p == vu2b->v_p )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu2b->l.magic);
	} else if( vu=nmg_find_vertexuse_on_face(vu1b->v_p, fu2) )  {
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu->l.magic);
	} else {
		struct loopuse *plu;
		plu = nmg_mlv(&fu2->l.magic, vu1b->v_p, OT_UNSPEC);
		nmg_loop_g(plu->l_p);
		vu = RT_LIST_FIRST( vertexuse, &plu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		(void)nmg_tbl(is->l2, TBL_INS_UNIQUE, &vu->l.magic);
	}
}

/*
 *			N M G _ I S E C T _ 3 E D G E _ 3 F A C E
 *
 *	Intersect an edge with a (non-colinear/coplanar) face
 *
 * This code currently assumes that an edge can only intersect a face at one
 * point.  This is probably a bad assumption for the future
 */
static void
nmg_isect_3edge_3face(bs, eu, fu)
struct nmg_inter_struct *bs;
struct edgeuse *eu;
struct faceuse *fu;
{
	struct vertexuse *vu_other;
	struct vertex	*v1;
	struct vertex	*v1mate;
	point_t		hit_pt;
	vect_t		edge_vect;
	fastf_t		edge_len;	/* MAGNITUDE(edge_vect) */
	fastf_t		dist;		/* parametric dist to hit point */
	fastf_t		dist_to_plane;	/* distance to hit point, in mm */
	int		status;
	struct loopuse	*plu;
	fastf_t		dist2;
	vect_t		start_pt;
	struct edgeuse	*euforw;
	struct edgeuse	*eunext;
	struct edgeuse	*eulast;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3edge_3face(, eu=x%x, fu=x%x) START\n", eu, fu);

	NMG_CK_INTER_STRUCT(bs);
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	v1 = eu->vu_p->v_p;
	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX_G(v1->vg_p);

	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	v1mate = eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v1mate);
	NMG_CK_VERTEX_G(v1mate->vg_p);

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		/* XXX edge sanity checking */
		nmg_veu( &eu->up.lu_p->down_hd, eu->up.magic_p );
	}

	/*
	 * First check the topology.  If the topology says that starting
	 * vertex of this edgeuse is on the other face, enter the
	 * two vertexuses of it in the list and return.
	 */
	if (vu_other=nmg_find_vertexuse_on_face(v1, fu)) {
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			register pointp_t p1 = v1->vg_p->coord;
			register pointp_t p2 = v1mate->vg_p->coord;
			rt_log("Edgeuse %g, %g, %g -> %g, %g, %g\n",
				V3ARGS(p1), V3ARGS(p2) );
			rt_log("\tvertex topologically on isect plane.\n\tAdding vu1=x%x (v=x%x), vu_other=x%x (v=x%x)\n",
				eu->vu_p, eu->vu_p->v_p,
				vu_other, vu_other->v_p);
		}

		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);
		(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		goto out;
	}

	/*
	 *  Starting vertex does not lie on the face according to topology.
	 *  Form a ray that starts at one vertex of the edgeuse
	 *  and points to the other vertex.
	 */
	VSUB2(edge_vect, v1mate->vg_p->coord, v1->vg_p->coord);
	edge_len = MAGNITUDE(edge_vect);

	VMOVE( start_pt, v1->vg_p->coord );

	if (rt_g.NMG_debug & DEBUG_POLYSECT)  {
		rt_log("Testing (%g, %g, %g) -> (%g, %g, %g) dir=(%g, %g, %g)\n",
			V3ARGS(start_pt),
			V3ARGS(v1mate->vg_p->coord),
			V3ARGS(edge_vect) );
	}

	status = rt_isect_ray_plane(&dist, start_pt, edge_vect, fu->f_p->fg_p->N);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
	    if (status >= 0)
		rt_log("\tHit. Status of rt_isect_ray_plane: %d dist: %g\n",
				status, dist);
	    else
		rt_log("\tMiss. Boring status of rt_isect_ray_plane: %d\n",
				status);
	}
	if (status < 0)  {
		/*  Ray does not strike plane.
		 *  See if start point lies on plane.
		 */
		dist = VDOT( start_pt, fu->f_p->fg_p->N ) - fu->f_p->fg_p->N[3];
		if( !NEAR_ZERO( dist, bs->tol.dist ) )
			goto out;		/* No geometric intersection */

		/* Start point lies on plane of other face */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tStart point lies on plane of other face\n");
		dist = VSUB2DOT( v1->vg_p->coord, start_pt, edge_vect )
				/ edge_len;
	}

	/* The ray defined by the edgeuse intersects the plane 
	 * of the other face.  Check to see if the distance to
         * intersection is between limits of the endpoints of
	 * this edge(use).
	 * The edge exists over values of 0 <= dist <= 1, ie,
	 * over values of 0 <= dist_to_plane <= edge_len.
	 * The tolerance, an absolute distance, can only be compared
	 * to other absolute distances like dist_to_plane & edge_len.
	 * The vertices are "fattened" by +/- bs->tol units.
	 */
	VJOIN1( hit_pt, start_pt, dist, edge_vect );
	dist_to_plane = edge_len * dist;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tedge_len=%g, dist=%g, dist_to_plane=%g\n",
			edge_len, dist, dist_to_plane);

	if ( dist_to_plane < -bs->tol.dist )  {
		/* Hit is behind first point */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tplane behind first point\n");
		goto out;
	}


	/*
	 * If the vertex on the other end of this edgeuse is on the face,
	 * then make a linkage to an existing face vertex (if found),
	 * and give up on this edge, knowing that we'll pick up the
	 * intersection of the next edgeuse with the face later.
	 */
	if ( dist_to_plane < bs->tol.dist )  {
		/* First point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge starts at plane intersect\n");
		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eu->vu_p->l.magic);

		vu_other = nmg_find_vu_in_face(v1->vg_p->coord, fu, &(bs->tol));
		if (vu_other) {
			register pointp_t	p3;
			/* Face has a very similar vertex.  Add to list */
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
			/* Make new coordinates be the midpoint */
			p3 = vu_other->v_p->vg_p->coord;
			VADD2SCALE(v1->vg_p->coord, v1->vg_p->coord, p3, 0.5);
			/* Combine the two vertices */
			nmg_jv(v1, vu_other->v_p);
		} else {
			/* Insert copy of this vertex into face */
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("Making vertexloop",
			    		v1->vg_p->coord);

			plu = nmg_mlv(&fu->l.magic, v1, OT_UNSPEC);
			nmg_loop_g(plu->l_p);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}
		goto out;
	}

	if ( dist_to_plane < edge_len - bs->tol.dist) {
		/* Intersection is between first and second vertex points.
		 * Insert new vertex at intersection point.
		 */
		nmg_break_3edge_at_plane(hit_pt, fu, bs, eu, v1, v1mate);
		goto out;
	}

	if ( dist_to_plane < edge_len + bs->tol.dist) {
		/* Second point is on plane of face, by geometry */
		if (rt_g.NMG_debug & DEBUG_POLYSECT)
			rt_log("\tedge ends at plane intersect\n");

		eunext = RT_LIST_PNEXT_CIRC(edgeuse,eu);
		NMG_CK_EDGEUSE(eunext);
		if( eunext->vu_p->v_p != v1mate )
			rt_bomb("nmg_isect_3edge_3face: discontinuous eu loop\n");

		(void)nmg_tbl(bs->l1, TBL_INS_UNIQUE, &eunext->vu_p->l.magic);

		vu_other = nmg_find_vu_in_face(v1mate->vg_p->coord, fu, &(bs->tol));
		if (vu_other) {
			register pointp_t	p3;
			/* Face has a very similar vertex.  Add to list */
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
			/* Make new coordinates be the midpoint */
			p3 = vu_other->v_p->vg_p->coord;
			VADD2SCALE(v1mate->vg_p->coord, v1mate->vg_p->coord, p3, 0.5);
			/* Combine the two vertices */
			nmg_jv(v1mate, vu_other->v_p);
		} else {
			/* Insert copy of this vertex into face */
			if (rt_g.NMG_debug & DEBUG_POLYSECT)
			    	VPRINT("Making vertexloop",
			    		v1mate->vg_p->coord);

			plu = nmg_mlv(&fu->l.magic, v1mate, OT_UNSPEC);
			nmg_loop_g(plu->l_p);
			vu_other = RT_LIST_FIRST( vertexuse, &plu->down_hd );
			NMG_CK_VERTEXUSE(vu_other);
			(void)nmg_tbl(bs->l2, TBL_INS_UNIQUE, &vu_other->l.magic);
		}
		goto out;
	}

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("\tdist to plane X: X > MAGNITUDE(edge)\n");

out:
	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		/* XXX edge sanity checking */
		nmg_veu( &eu->up.lu_p->down_hd, eu->up.magic_p );
	}
	/* XXX Ensure that other face is still OK */
	nmg_vfu( &fu->s_p->fu_hd, fu->s_p );

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_3edge_3face(, eu=x%x, fu=x%x) END\n", eu, fu);
}

/*
 *			N M G _ I S E C T _ L O O P 3 P _ F A C E 3 P
 *
 *	Intersect a single loop with another face
 */
static void
nmg_isect_loop3p_face3p(bs, lu, fu)
struct nmg_inter_struct *bs;
struct loopuse *lu;
struct faceuse *fu;
{
	struct edgeuse	*eu;
	long		magic1;

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_loop3p_face3p(, lu=x%x, fu=x%x)\n", lu, fu);
		HPRINT("  fg N", fu->f_p->fg_p->N);
	}

	NMG_CK_INTER_STRUCT(bs);
	NMG_CK_LOOPUSE(lu);
	NMG_CK_LOOP(lu->l_p);
	NMG_CK_LOOP_G(lu->l_p->lg_p);

	NMG_CK_FACEUSE(fu);

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		struct vertexuse	*vu = RT_LIST_FIRST(vertexuse,&lu->down_hd);
		/* this is most likely a loop inserted when we split
		 * up fu2 wrt fu1 (we're now splitting fu1 wrt fu2)
		 */
		nmg_isect_3vertex_3face(bs, vu, fu);
		return;
	} else if (magic1 != NMG_EDGEUSE_MAGIC) {
		rt_bomb("nmg_isect_loop3p_face3p() Unknown type of NMG loopuse\n");
	}

	/*  Process loop consisting of a list of edgeuses.
	 *
	 * By going backwards around the list we avoid
	 * re-processing an edgeuse that was just created
	 * by nmg_isect_3edge_3face.  This is because the edgeuses
	 * point in the "next" direction, and when one of
	 * them is split, it inserts a new edge AHEAD or
	 * "nextward" of the current edgeuse.
	 */ 
	nmg_vfu( &fu->s_p->fu_hd, fu->s_p );
	nmg_veu( &lu->down_hd, &lu->l.magic );
	for( eu = RT_LIST_LAST(edgeuse, &lu->down_hd );
	     RT_LIST_NOT_HEAD(eu,&lu->down_hd);
	     eu = RT_LIST_PLAST(edgeuse,eu) )  {
		NMG_CK_EDGEUSE(eu);

		if (eu->up.magic_p != &lu->l.magic) {
			rt_bomb("nmg_isect_loop3p_face3p: edge does not share loop\n");
		}

		nmg_isect_3edge_3face(bs, eu, fu);

		nmg_ck_lueu(lu, "nmg_isect_loop3p_face3p");
	}
	nmg_veu( &lu->down_hd, &lu->l.magic );
	nmg_vfu( &fu->s_p->fu_hd, fu->s_p );
}

/*
 *			N M G _ I S E C T _ F A C E 3 P _ F A C E 3 P
 *
 *	Intersect entirely of planar face 1 with the entirety of planar face 2
 *	given that the two faces are not coplanar.
 */
static void
nmg_isect_face3p_face3p(bs, fu1, fu2)
struct nmg_inter_struct *bs;
struct faceuse	*fu1;
struct faceuse	*fu2;
{
	struct loopuse	*lu, *fu2lu;
	struct loop_g	*lg;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_face3p_face3p(, fu1=x%x, fu2=x%x) START ++++++++++\n", fu1, fu2);

	NMG_CK_INTER_STRUCT(bs);
	NMG_CK_FACE_G(fu2->f_p->fg_p);
	NMG_CK_FACE_G(fu1->f_p->fg_p);

	/* process each face loop in face 1 */
	for( RT_LIST_FOR( lu, loopuse, &fu1->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		if (lu->up.fu_p != fu1) {
			rt_bomb("nmg_isect_face3p_face3p() Child loop doesn't share parent!\n");
		}
		NMG_CK_LOOP(lu->l_p);
		lg = lu->l_p->lg_p;
		NMG_CK_LOOP_G(lg);

		/* If the bounding box of a loop doesn't intersect the
		 * bounding box of a loop in the other face, it doesn't need
		 * to get cut.
		 */
		for (RT_LIST_FOR(fu2lu, loopuse, &fu2->lu_hd )){
			struct loop_g *fu2lg;

			NMG_CK_LOOPUSE(fu2lu);
			NMG_CK_LOOP(fu2lu->l_p);
			fu2lg = fu2lu->l_p->lg_p;
			NMG_CK_LOOP_G(fu2lg);

			if (! V3RPP_OVERLAP( fu2lg->min_pt, fu2lg->max_pt,
			    lg->min_pt, lg->max_pt)) continue;

			nmg_isect_loop3p_face3p(bs, lu, fu2);
		}
	}
	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_face3p_face3p(, fu1=x%x, fu2=x%x) RETURN ++++++++++\n\n", fu1, fu2);
}

/*
 *			N M G _ I S E C T _ E D G E 2 P _ F A C E 2 P
 *
 *  Given one (2D) edge lying in the plane of another face,
 *  intersect with all the other edges of that face.
 *  This edge represents a line of intersection, and so a
 *  cutjoin/mesh pass will be needed for each one.
 * XXX How to really treat this edge as a ray, so that the state machine
 * XXX knows how to get in and out of loops?
 */
static void
nmg_isect_edge2p_face2p( is, eu, fu, eu_fu )
struct nmg_inter_struct	*is;
struct edgeuse		*eu;
struct faceuse		*fu;
struct faceuse		*eu_fu;		/* fu that eu is from */
{
	struct nmg_ptbl vert_list1, vert_list2;
	struct loopuse	*lu;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_EDGEUSE(eu);
	NMG_CK_FACEUSE(fu);
	NMG_CK_FACEUSE(eu_fu);

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_edge2p_face2p(eu=x%x, fu=x%x)\n", eu, fu);

	/* See if this edge is topologically on other face already */
	/* XXX Is it safe to assume that since both endpoints are
	 * XXX already registered in the other face that the edge
	 * XXX has been fully intersected already?
	 */
	if( nmg_find_vertexuse_on_face( eu->vu_p->v_p, fu ) &&
	    nmg_find_vertexuse_on_face( eu->eumate_p->vu_p->v_p, fu ) )  {
#if 0
		rt_log("edge: (%g,%g,%g) (%g,%g,%g)\n", V3ARGS(eu->vu_p->v_p->vg_p->coord), V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
#endif
		return;
	}

	(void)nmg_tbl(&vert_list1, TBL_INIT,(long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT,(long *)NULL);

    	is->l1 = &vert_list1;
    	is->l2 = &vert_list2;

    	if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "Isect_faces%d.pl", fno++, fu, eu_fu, 0 );
    	}

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		struct edgeuse	*eu2;

		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			struct vertexuse	*vu;
			vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			/* XXX How to ensure that if original edge is split,
			 * then both parts get processed?
			 */
			(void)nmg_isect_edge2p_vert2p( is, eu, vu );
			continue;
		}
		for( RT_LIST_FOR( eu2, edgeuse, &lu->down_hd ) )  {
			/* isect eu with eu */
			/* XXX same question here */
			nmg_isect_edge2p_edge2p( is, eu, eu2, eu_fu, fu );
		}
	}

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_edge2p_face2p(eu=x%x, fu=x%x) vert_lists A:\n", eu, fu );
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

	nmg_purge_unwanted_intersection_points(&vert_list1, fu);
	nmg_purge_unwanted_intersection_points(&vert_list2, eu_fu);

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_edge2p_face2p(eu=x%x, fu=x%x) vert_lists B:\n", eu, fu );
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

    	if (vert_list1.end == 0) {
    		/* there were no intersections */
    		goto out;
    	}

	nmg_face_cutjoin(&vert_list1, &vert_list2, fu, eu_fu, is->pt, is->dir, &is->tol);
	nmg_mesh_faces(fu, eu_fu);

out:
	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);
}

/*
 *			N M G _ I S E C T _ T W O _ F A C E 2 P
 *
 *  Manage the mutual intersection of two 3-D coplanar planar faces.
 */
static void
nmg_isect_two_face2p( is, fu1, fu2 )
struct nmg_inter_struct	*is;
struct faceuse		*fu1, *fu2;
{
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertexuse	*vu;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

#if 0
	/* r71, r23 are useful demonstrations */
	/* Turn these flags on only for early code debugging */
	rt_g.NMG_debug |= DEBUG_POLYSECT;
	rt_g.NMG_debug |= DEBUG_FCUT;
	rt_g.NMG_debug |= DEBUG_VU_SORT;
	rt_g.NMG_debug |= DEBUG_PLOTEM;
	rt_g.NMG_debug |= DEBUG_INS;
	rt_g.NMG_debug |= DEBUG_FCUT;
#endif

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_isect_two_face2p\n");

	/* For every edge in f1, intersect with f2, incl. cutjoin */
	for( RT_LIST_FOR( lu, loopuse, &fu1->lu_hd ) )  {
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			nmg_isect_vert2p_face2p( is, vu, fu2 );
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			nmg_isect_edge2p_face2p( is, eu, fu2, fu1 );
		}
	}

	/* For every edge in f2, intersect with f1, incl. cutjoin */
	for( RT_LIST_FOR( lu, loopuse, &fu2->lu_hd ) )  {
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )  {
			vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			nmg_isect_vert2p_face2p( is, vu, fu1 );
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			nmg_isect_edge2p_face2p( is, eu, fu1, fu2 );
		}
	}
}

/*
 *			N M G _ I S E C T _ T W O _ F A C E 3 P
 *
 *  Handle the complete mutual intersection of
 *  two 3-D non-coplanar planar faces,
 *  including cutjoin and meshing.
 */
static void
nmg_isect_two_face3p( is, fu1, fu2 )
struct nmg_inter_struct	*is;
struct faceuse		*fu1, *fu2;
{
	struct nmg_ptbl vert_list1, vert_list2;

	NMG_CK_INTER_STRUCT(is);
	NMG_CK_FACEUSE(fu1);
	NMG_CK_FACEUSE(fu2);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("nmg_isect_two_face3p( fu1=x%x, fu2=x%x )\n", fu1, fu2);
		VPRINT("isect ray is->pt ", is->pt);
		VPRINT("isect ray is->dir", is->dir);
	}

	nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
	nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );

	(void)nmg_tbl(&vert_list1, TBL_INIT,(long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_INIT,(long *)NULL);

    	is->l1 = &vert_list1;
    	is->l2 = &vert_list2;

    	if (rt_g.NMG_debug & (DEBUG_POLYSECT|DEBUG_FCUT|DEBUG_MESH)
    	    && rt_g.NMG_debug & DEBUG_PLOTEM) {
    		static int fno=1;
    	    	nmg_pl_2fu( "Isect_faces%d.pl", fno++, fu1, fu2, 0 );
    	}

	nmg_isect_face3p_face3p(is, fu1, fu2);

	nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
	nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_two_generic_faces(fu1=x%x, fu2=x%x) vert_lists A:\n", fu1, fu2);
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

    	is->l2 = &vert_list1;
    	is->l1 = &vert_list2;
	nmg_isect_face3p_face3p(is, fu2, fu1);

	nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
	nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );

	nmg_purge_unwanted_intersection_points(&vert_list1, fu2);
	nmg_purge_unwanted_intersection_points(&vert_list2, fu1);

    	if (rt_g.NMG_debug & DEBUG_FCUT) {
	    	rt_log("nmg_isect_two_generic_faces(fu1=x%x, fu2=x%x) vert_lists B:\n", fu1, fu2);
    		nmg_pr_ptbl_vert_list( "vert_list1", &vert_list1 );
    		nmg_pr_ptbl_vert_list( "vert_list2", &vert_list2 );
    	}

    	if (vert_list1.end == 0) {
    		/* there were no intersections */
    		goto out;
    	}

	nmg_face_cutjoin(&vert_list1, &vert_list2, fu1, fu2, is->pt, is->dir, &is->tol);

	nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
	nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );

	nmg_mesh_faces(fu1, fu2);

#if 0
	show_broken_stuff((long *)fu1, (long **)NULL, 1, 0);
	show_broken_stuff((long *)fu2, (long **)NULL, 1, 0);
#endif

out:
	(void)nmg_tbl(&vert_list1, TBL_FREE, (long *)NULL);
	(void)nmg_tbl(&vert_list2, TBL_FREE, (long *)NULL);

	nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
	nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
}

/*
 *			N M G _ I S E C T _ T W O _ G E N E R I C _ F A C E S
 *
 *	Intersect a pair of faces
 */
static void
nmg_isect_two_generic_faces(fu1, fu2, tol)
struct faceuse		*fu1, *fu2;
CONST struct rt_tol	*tol;
{
	struct nmg_inter_struct	bs;
	int		i;
	fastf_t		*pl1, *pl2;
	struct face	*f1;
	struct face	*f2;
	point_t		min_pt;
	int		status;

	RT_CK_TOL(tol);
	bs.magic = NMG_INTER_STRUCT_MAGIC;
	bs.vert2d = (fastf_t *)NULL;
	bs.tol = *tol;		/* struct copy */

	NMG_CK_FACEUSE(fu1);
	f1 = fu1->f_p;
	NMG_CK_FACE(f1);
	NMG_CK_FACE_G(f1->fg_p);

	NMG_CK_FACEUSE(fu2);
	f2 = fu2->f_p;
	NMG_CK_FACE(f2);
	NMG_CK_FACE_G(f2->fg_p);

	if (rt_g.NMG_debug & DEBUG_POLYSECT) {
		rt_log("\nnmg_isect_two_generic_faces(fu1=x%x, fu2=x%x)\n", fu1, fu2);
		pl1 = f1->fg_p->N;
		pl2 = f2->fg_p->N;

		rt_log("Planes\t%gx + %gy + %gz = %g\n\t%gx + %gy + %gz = %g\n",
			pl1[0], pl1[1], pl1[2], pl1[3],
			pl2[0], pl2[1], pl2[2], pl2[3]);
	}
	nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
	nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );

	if ( !V3RPP_OVERLAP(f2->fg_p->min_pt, f2->fg_p->max_pt,
	    f1->fg_p->min_pt, f1->fg_p->max_pt) )  return;

	/* Extents of face1 overlap face2 */
	VMOVE(min_pt, f1->fg_p->min_pt);
	VMIN(min_pt, f2->fg_p->min_pt);
	status = rt_isect_2planes( bs.pt, bs.dir, f1->fg_p->N, f2->fg_p->N,
		min_pt, tol );

	switch( status )  {
	case 0:
		/* All is well */
		bs.coplanar = 0;
		nmg_isect_two_face3p( &bs, fu1, fu2 );
		break;
	case -1:
		/* co-planar faces */
rt_log("co-planar faces.\n");
		bs.coplanar = 1;
		nmg_isect_two_face2p( &bs, fu1, fu2 );
		break;
	case -2:
		/* parallel and distinct, no intersection */
		break;
	default:
		/* internal error */
		rt_log("ERROR nmg_isect_two_generic_faces() unable to find plane intersection\n");
		break;
	}

	if(bs.vert2d)  rt_free( (char *)bs.vert2d, "vert2d" );

	nmg_vfu( &fu1->s_p->fu_hd, fu1->s_p );
	nmg_vfu( &fu2->s_p->fu_hd, fu2->s_p );
}

/*
 *			N M G _ C R A C K S H E L L S
 *
 *	Split the components of two shells wherever they may intersect,
 *	in preparation for performing boolean operations on the shells.
 */
void
nmg_crackshells(s1, s2, tol)
struct shell		*s1;
struct shell		*s2;
CONST struct rt_tol	*tol;
{
	struct face	*f1;
	struct faceuse	*fu1, *fu2;
	struct shell_a	*sa1, *sa2;
	long		*flags;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_crackshells(s1=x%x, s2=x%x)\n", s1, s2);

	RT_CK_TOL(tol);
	NMG_CK_SHELL(s1);
	sa1 = s1->sa_p;
	NMG_CK_SHELL_A(sa1);

	NMG_CK_SHELL(s2);
	sa2 = s2->sa_p;
	NMG_CK_SHELL_A(sa2);

	nmg_vshell( &s1->r_p->s_hd, s1->r_p );
	nmg_vshell( &s2->r_p->s_hd, s2->r_p );

	/* XXX this isn't true for non-3-manifold geometry! */
	if( RT_LIST_IS_EMPTY( &s1->fu_hd ) ||
	    RT_LIST_IS_EMPTY( &s2->fu_hd ) )  {
		rt_log("ERROR:shells must contain faces for boolean operations.");
		return;
	}

	/* See if shells overlap */
	if ( ! V3RPP_OVERLAP(sa1->min_pt, sa1->max_pt,
	    sa2->min_pt, sa2->max_pt) )
		return;

	flags = (long *)rt_calloc( s1->r_p->m_p->maxindex, sizeof(long),
		"nmg_crackshells flags[]" );

	/*
	 *  Check each of the faces in shell 1 to see
	 *  if they overlap the extent of shell 2
	 */
	for( RT_LIST_FOR( fu1, faceuse, &s1->fu_hd ) )  {
		NMG_CK_FACEUSE(fu1);
		f1 = fu1->f_p;
		NMG_CK_FACE(f1);

		if( NMG_INDEX_IS_SET(flags, f1) )  continue;
		NMG_CK_FACE_G(f1->fg_p);

		/* See if face f1 overlaps shell2 */
		if( ! V3RPP_OVERLAP(sa2->min_pt, sa2->max_pt,
		    f1->fg_p->min_pt, f1->fg_p->max_pt) )
			continue;

		/*
		 *  Now, check the face f1 from shell 1
		 *  against each of the faces of shell 2
		 */
	    	for( RT_LIST_FOR( fu2, faceuse, &s2->fu_hd ) )  {
	    		NMG_CK_FACEUSE(fu2);
	    		NMG_CK_FACE(fu2->f_p);

			nmg_isect_two_generic_faces(fu1, fu2, tol);

			/* try not to process redundant faceuses (mates) */
			if( RT_LIST_NOT_HEAD( fu2, &s2->fu_hd ) )  {
				register struct faceuse	*nextfu;
				nextfu = RT_LIST_PNEXT(faceuse, fu2 );
				if( nextfu->f_p == fu2->f_p )
					fu2 = nextfu;
			}
	    	}

		/* Check f1 from s1 against wire loops and edges of s2 */

		/* Check f1 from s1 against lone vert of s2 */

	    	NMG_INDEX_SET(flags, f1);

		/* XXX expensive, but very conservative, for now */
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );
		nmg_vshell( &s2->r_p->s_hd, s2->r_p );
	}

	/*
	 *  Check each wire loop and wire edge of shell 1 against shell 2.
	 */
#if 0
	for( RT_LIST_FOR( lu1, loopuse, &s1->lu_hd ) )  {
	}
	for( RT_LIST_FOR( eu1, edgeuse, &s1->eu_hd ) )  {
	}
#endif

	/* Check each lone vert of s1 against shell 2 */

	rt_free( (char *)flags, "nmg_crackshells flags[]" );
}
