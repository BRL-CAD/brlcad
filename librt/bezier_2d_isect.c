/* 	B E Z I E R _ 2 D _ I S E C T . C
 *
 *	The following routines are for 2D Bezier curves
 *
 *	The following routines are borrowed from Graphics Gems I,
 *	Academic Press, Inc, 1990, Andrew S. Glassner (editor),
 *	"A Bezier Curve-based Root-finder", Philip J. Schneider.
 *
 *	JRA 4/2001:
 *		Modifications have been made for inclusion in BRL-CAD and
 *		to generalize the codes for finding intersections with any 2D line
 *		rather than just the X-axis.
 */


#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "../librt/debug.h"

#define SGN(_x) (((_x)<0) ? -1 : 1)
#define MAXDEPTH	64

/*
 * CrossingCount :
 *      Count the number of times a Bezier control polygon 
 *      crosses the ray. This number is >= the number of roots.
 *
 */
int CrossingCount(
    point2d_t      *V,		/*  2D Control pts of Bezier curve */
    int         degree,         /*  Degreee of Bezier curve     */
    point2d_t	ray_start,	/*  starting point for ray	*/
    point2d_t	ray_dir,	/*  unit ray direction (actually a vector, not a point) */
    point2d_t	ray_perp)	/*  unit vector perpendicular to ray direction */
{
    int         i;      
    int         n_crossings = 0;        /*  Number of crossings    */
    int         sign, old_sign;         /*  Sign of coefficients        */
    point2d_t	to_pt;			/* vector from ray start to a control point */

    V2SUB2( to_pt, ray_start, V[0] );
    sign = old_sign = SGN( V2DOT( to_pt, ray_perp ) );
    for (i = 1; i <= degree; i++) {
	    V2SUB2( to_pt, ray_start, V[i] );
	    sign = SGN( V2DOT( to_pt, ray_perp ) );
	    if (sign != old_sign) n_crossings++;
	    old_sign = sign;
    }

    return n_crossings;
}


/*
 *  ControlPolygonFlatEnough :
 *	Check if the control polygon of a Bezier curve is flat enough
 *	for recursive subdivision to bottom out.
 *
 */
int
ControlPolygonFlatEnough(
    point2d_t	*V,		/* Control points	*/
    int 	degree,		/* Degree of polynomial	*/
    fastf_t	epsilon)	/* Maximum allowable error */
{
    int 	i;			/* Index variable		*/
    double 	*distance;		/* Distances from pts to line	*/
    double 	max_distance_above;	/* maximum of these		*/
    double 	max_distance_below;
    double 	error;			/* Precision of root		*/
    double 	intercept_1,
    	   	intercept_2,
	   		left_intercept,
		   	right_intercept;
    double 	a, b, c;		/* Coefficients of implicit	*/
    					/* eqn for line from V[0]-V[deg]*/

    /* Find the  perpendicular distance		*/
    /* from each interior control point to 	*/
    /* line connecting V[0] and V[degree]	*/
    distance = (double *)bu_malloc((unsigned)(degree + 1) * sizeof(double), "ControlPolygonFlatEnough");
    {
	double	abSquared;

	/* Derive the implicit equation for line connecting first */
	/*  and last control points */
	a = V[0][Y] - V[degree][Y];
	b = V[degree][X] - V[0][X];
	c = V[0][X] * V[degree][Y] - V[degree][X] * V[0][Y];

	abSquared = (a * a) + (b * b);

        for (i = 1; i < degree; i++) {
	    /* Compute distance from each of the points to that line	*/
	    	distance[i] = a * V[i][X] + b * V[i][Y] + c;
	    	if (distance[i] > 0.0) {
				distance[i] = (distance[i] * distance[i]) / abSquared;
	    	}
	    	if (distance[i] < 0.0) {
				distance[i] = -((distance[i] * distance[i]) / abSquared);
	    	}
	}
    }


    /* Find the largest distance	*/
    max_distance_above = 0.0;
    max_distance_below = 0.0;
    for (i = 1; i < degree; i++) {
		if (distance[i] < 0.0) {
			max_distance_below = MIN(max_distance_below, distance[i]);
		};
		if (distance[i] > 0.0) {
			max_distance_above = MAX(max_distance_above, distance[i]);
		}
    }
    bu_free((char *)distance,"ControlPolygonFlatEnough" );

    {
	double	det, dInv;
	double	a1, b1, c1, a2, b2, c2;

	/*  Implicit equation for zero line */
	a1 = 0.0;
	b1 = 1.0;
	c1 = 0.0;

	/*  Implicit equation for "above" line */
	a2 = a;
	b2 = b;
	c2 = c + max_distance_above;

	det = a1 * b2 - a2 * b1;
	dInv = 1.0/det;
	
	intercept_1 = (b1 * c2 - b2 * c1) * dInv;

	/*  Implicit equation for "below" line */
	a2 = a;
	b2 = b;
	c2 = c + max_distance_below;
	
	det = a1 * b2 - a2 * b1;
	dInv = 1.0/det;
	
	intercept_2 = (b1 * c2 - b2 * c1) * dInv;
    }

    /* Compute intercepts of bounding box	*/
    left_intercept = MIN(intercept_1, intercept_2);
    right_intercept = MAX(intercept_1, intercept_2);

    error = 0.5 * (right_intercept-left_intercept);    

    if (error < epsilon) {
		return 1;
    }
    else {
		return 0;
    }
}


/*
 *  Bezier : 
 *	Evaluate a Bezier curve at a particular parameter value
 *      Fill in control points for resulting sub-curves if "Left" and
 *	"Right" are non-null.
 * 
 */
void
Bezier(
    point2d_t 	*V,			/* Control pts			*/
    int 	degree,			/* Degree of bezier curve	*/
    double 	t,			/* Parameter value [0..1]	*/
    point2d_t 	*Left,			/* RETURN left half ctl pts	*/
    point2d_t 	*Right,			/* RETURN right half ctl pts	*/
    point2d_t	eval_pt)		/* RETURN evaluated point	*/
{
    int 	i, j;		/* Index variables	*/
    point2d_t 	**Vtemp;


    Vtemp = (point2d_t **)bu_calloc( degree+1, sizeof(point2d_t *), "Bezier: Vtemp array" );
    for( i=0 ; i<=degree ; i++ )
	    Vtemp[i] = (point2d_t *)bu_calloc( degree+1, sizeof( point2d_t ),
					       "Bezier: Vtemp[i] array" );
    /* Copy control points	*/
    for (j =0; j <= degree; j++) {
		V2MOVE( Vtemp[0][j], V[j]);
    }

    /* Triangle computation	*/
    for (i = 1; i <= degree; i++) {	
		for (j =0 ; j <= degree - i; j++) {
	    	Vtemp[i][j][X] =
	      		(1.0 - t) * Vtemp[i-1][j][X] + t * Vtemp[i-1][j+1][X];
	    	Vtemp[i][j][Y] =
	      		(1.0 - t) * Vtemp[i-1][j][Y] + t * Vtemp[i-1][j+1][Y];
		}
    }
    
    if (Left != NULL) {
		for (j = 0; j <= degree; j++) {
	    	V2MOVE( Left[j], Vtemp[j][0]);
		}
    }
    if (Right != NULL) {
		for (j = 0; j <= degree; j++) {
	    	V2MOVE( Right[j], Vtemp[degree-j][j]);
		}
    }

    V2MOVE( eval_pt, Vtemp[degree][0] );
    for( i=0 ; i<=degree ; i++ )
	    bu_free( (char *)Vtemp[i], "Bezier: Vtemp[i]" );
    bu_free( (char *)Vtemp, "Bezier: Vtemp" );

    return;
}

/*
 *  ComputeXIntercept :
 *      Compute intersection of chord from first control point to last
 *      with ray.
 *
 *  Returns :
 *
 *	0 - no intersection
 *	1 - found an intersection
 *
 *	intercept - contains calculated intercept
 * 
 */
static int
ComputeXIntercept(
    point2d_t   *V,                     /*  Control points      */
    int         degree,                 /*  Degree of curve     */
    point2d_t	ray_start,		/*  starting point of ray */
    point2d_t	ray_dir,		/* unit ray direction	*/
    double	epsilon,		/* maximum allowable error */
    point2d_t	intercept )		/* calculated intercept point */
{
	point_t a;
	vect_t	c;
	point_t p;
	vect_t d;
	fastf_t dist[2];
	struct bn_tol     tol;
	int	ret;

	tol.magic = BN_TOL_MAGIC;
	tol.dist = epsilon;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1.0 - tol.perp;

	V2MOVE( p, ray_start );
	p[Z] = 0.0;
	V2MOVE( d, ray_dir );
	c[Z] = 0.0;
	V2MOVE( a, V[0] );
	a[Z] = 0.0;
	V2SUB2( c, V[degree], V[0] );
	c[Z] = 0.0;

	ret = bn_isect_line2_lseg2( dist, p, d, a, c, &tol );

	if( ret <= 0 )
		return 0;

	switch( ret ) {
	        case 1:
			/* intercept at V[0] */
			V2MOVE( intercept, V[0] );
			break;
	        case 2:
			/* intercept at V[degree] */
			V2MOVE( intercept, V[degree] );
			break;
	        case 3:
			/* intercept between endpoints */
			V2JOIN1( intercept, ray_start, dist[0], ray_dir );
			break;
	}

	return 1;
}


/*
 *  FindRoots :
 *      Given an equation in Bernstein-Bezier form, find
 *      all of the roots in the interval [0, 1].  Return the number
 *      of roots found.
 */
int
FindRoots(
    point2d_t      *w,                     /* The control points           */
    int         degree,         /* The degree of the polynomial */
    point2d_t	**intercept,	/* list of intersections found	*/
    point2d_t	ray_start,	/* starting point of ray */
    point2d_t	ray_dir,	/* Unit direction for ray */
    point2d_t	ray_perp,	/* Unit vector normal to ray_dir */
    int         depth,          /* The depth of the recursion   */
    fastf_t	epsilon)	/* maximum allowable error */
{  
    int         i;
    point2d_t   *Left,                  /* New left and right           */
                *Right;                 /* control polygons             */
    int         left_count,             /* Solution count from          */
                right_count;            /* children                     */
    point2d_t   *left_t,                /* Solutions from kids          */
                *right_t;
    int		total_count;
    point2d_t	eval_pt;

    switch (CrossingCount(w, degree, ray_start, ray_dir, ray_perp)) {
        case 0 : {      /* No solutions here    */
             return 0;
        }
        case 1 : {      /* Unique solution      */
            /* Stop recursion when the tree is deep enough      */
            /* if deep enough, return 1 solution at midpoint    */
            if (depth >= MAXDEPTH) {
		    *intercept = (point2d_t *)bu_malloc( sizeof( point2d_t ), "FindRoots: unique solution" );
		    Bezier( w, degree, 0.5, NULL, NULL, *intercept[0] );
                    return 1;
            }
            if (ControlPolygonFlatEnough(w, degree, epsilon)) {
		    *intercept = (point2d_t *)bu_malloc( sizeof( point2d_t ), "FindRoots: unique solution" );
		    if( !ComputeXIntercept( w, degree, ray_start, ray_dir, epsilon, *intercept[0] ) ){
			    bu_free( (char *)(*intercept), "FindRoots: no solution" );
			    return 0;
		    }
                    return 1;
            }
            break;
        }
    }

    /* Otherwise, solve recursively after       */
    /* subdividing control polygon              */
    Left = (point2d_t *)bu_calloc( degree+1, sizeof( point2d_t ), "FindRoots: Left" );
    Right = (point2d_t *)bu_calloc( degree+1, sizeof( point2d_t ), "FindRoots: Right" );
    Bezier(w, degree, 0.5, Left, Right, eval_pt);

    left_count  = FindRoots(Left,  degree, &left_t, ray_start, ray_dir, ray_perp, depth+1, epsilon);
    right_count = FindRoots(Right, degree, &right_t, ray_start, ray_dir, ray_perp, depth+1, epsilon);

    total_count = left_count + right_count;

    bu_free( (char *)Left, "FindRoots: Left" );
    bu_free( (char *)Right, "FindRoots: Right" );
    if( total_count )
	    *intercept = (point2d_t *)bu_calloc( total_count, sizeof( point2d_t ),
				       "FindRoots: roots compilation" );

    /* Gather solutions together        */
    for (i = 0; i < left_count; i++) {
	    V2MOVE( (*intercept)[i], left_t[i] );
    }
    for (i = 0; i < right_count; i++) {
	    V2MOVE( (*intercept)[i+left_count], right_t[i] );
    }

    if( left_count )
	    bu_free( (char *)left_t, "Left roots" );
    if( right_count )
	    bu_free( (char *)right_t, "Right roots" );

    /* Send back total number of solutions      */
    return (total_count);
}

struct bezier_2d_list *
subdivide_bezier( struct bezier_2d_list *bezier_in, int degree, fastf_t epsilon, int depth )
{
	struct bezier_2d_list *bz_l, *bz_r, *new_head;
	struct bezier_2d_list *left_rtrn, *rt_rtrn;
	point2d_t pt;

	/* create a new head */
	new_head = (struct bezier_2d_list *)bu_malloc( sizeof( struct bezier_2d_list ),
						       "subdivide_bezier: new_head" );
	BU_LIST_INIT( &new_head->l );
	if( depth >= MAXDEPTH ){
		BU_LIST_APPEND( &new_head->l, &bezier_in->l );
		return( new_head );
	}

	if( ControlPolygonFlatEnough( bezier_in->ctl, degree, epsilon ) ) {
		BU_LIST_APPEND( &new_head->l, &bezier_in->l );
		return( new_head );
	}

	/* allocate memory for left and right curves */
	bz_l = (struct bezier_2d_list *)bu_malloc( sizeof( struct bezier_2d_list ), "subdivide_bezier: bz_l" );
	BU_LIST_INIT( &bz_l->l );
	bz_r = (struct bezier_2d_list *)bu_malloc( sizeof( struct bezier_2d_list ), "subdivide_bezier: bz_r" );
	BU_LIST_INIT( &bz_r->l );
	bz_l->ctl = (point2d_t *)bu_calloc( degree + 1, sizeof( point2d_t ),
					    "subdivide_bezier: bz_l->ctl" );
	bz_r->ctl = (point2d_t *)bu_calloc( degree + 1, sizeof( point2d_t ),
					    "subdivide_bezier: bz_r->ctl" );

	/* subdivide at t = 0.5 */
	Bezier( bezier_in->ctl, degree, 0.5, bz_l->ctl, bz_r->ctl, pt );

	/* eliminate original */
	BU_LIST_DEQUEUE( &bezier_in->l );
	bu_free( (char *)bezier_in->ctl, "subdivide_bezier: bezier_in->ctl" );
	bu_free( (char *)bezier_in, "subdivide_bezier: bezier_in" );

	/* recurse on left curve */
	left_rtrn = subdivide_bezier( bz_l, degree, epsilon, depth+1 );

	/* recurse on right curve */
	rt_rtrn = subdivide_bezier( bz_r, degree, epsilon, depth+1 );

	BU_LIST_APPEND_LIST( &new_head->l, &left_rtrn->l );
	BU_LIST_APPEND_LIST( &new_head->l, &rt_rtrn->l );
	bu_free( (char *)left_rtrn, "subdivide_bezier: left_rtrn (head)" );
	bu_free( (char *)rt_rtrn, "subdivide_bezier: rt_rtrn (head)" );

	return( new_head );
}
