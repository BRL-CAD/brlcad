/*	N U R B  _ E V A L . C
 *
 *  Function -
 *	Evaluate a Non Uniform Rational B-spline curve or at the 
 *	given (u,v) values.
 *  Author -
 *	Paul Randal Stay
 * 
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

/* Algorithm -
 *
 * The algorithm uses the traditional COX-deBoor approach 
 * found in the book "Pratical Guide to Splines" Carl de Boor, pg 147 
 * to evaluate a parametric value on a curve. This is expanded to the surface.
 */

void
rt_nurb_s_eval( srf, u, v, final_value )
CONST struct face_g_snurb *srf;
fastf_t	u;
fastf_t v;
fastf_t * final_value;
{
	fastf_t * mesh_ptr = srf->ctl_points;
	fastf_t * curves;
	int	i, j, k;
	int	row_size = srf->s_size[RT_NURB_SPLIT_ROW];
	int	col_size = srf->s_size[RT_NURB_SPLIT_COL];
	fastf_t * c_ptr;
	fastf_t * diff_curve, *ev_pt;
	int	k_index;
	int	coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

	NMG_CK_SNURB(srf);

	/* Because the algorithm is destructive in nature, the
	 * rows of the control mesh are copied. The evaluation
	 * is then done on each row curve and then evaluation
	 * is then performed on the resulting curve.
	 */

	diff_curve = (fastf_t * )
	rt_malloc(row_size * sizeof(fastf_t) * coords,
	    "rt_nurb_s__eval: diff_curve");

	c_ptr = diff_curve;

	k_index = rt_nurb_knot_index( &srf->u, u, srf->order[RT_NURB_SPLIT_ROW] );
	if( k_index < 0 )
	{
		bu_log( "rt_nurb_s_eval: u value outside parameter range\n");
		bu_log( "\tUV = (%g %g )\n", u,v );
		rt_nurb_s_print( "", srf );
		rt_bomb( "rt_nurb_s_eval: u value outside parameter range\n");
	}

	curves = (fastf_t * ) rt_malloc( col_size * sizeof(fastf_t) * coords,
	    "rt_nurb_s_eval:crv_ptr");

	for ( i = 0; i < row_size; i++) {
		fastf_t * rtr_pt;
		fastf_t * crv_ptr;

		crv_ptr = curves;

		for ( j = 0; j < (col_size * coords ); j++) {
			*crv_ptr++ = *mesh_ptr++;
		}

		rtr_pt =  (fastf_t * ) rt_nurb_eval_crv( curves, srf->order[RT_NURB_SPLIT_ROW], u, 
		    &srf->u, k_index, coords );

		for (k = 0; k < coords; k++)
			c_ptr[k] = rtr_pt[k];
		c_ptr += coords;
	}

	rt_free( (char *)curves, "rt_nurb_s_eval: curves" );

	k_index = rt_nurb_knot_index( &srf->v, v, srf->order[RT_NURB_SPLIT_COL] );

	ev_pt = (fastf_t * ) rt_nurb_eval_crv( diff_curve, srf->order[RT_NURB_SPLIT_COL], 
		v, &srf->v, k_index, coords);

	for ( k = 0; k < coords; k++)
		final_value[k] = ev_pt[k];

	rt_free ( (char *)diff_curve, "rt_nurb_s_eval: diff curve" );
}


void
rt_nurb_c_eval( crv, param, final_value)
CONST struct edge_g_cnurb *crv;
fastf_t param;
fastf_t * final_value;
{
	fastf_t * pnts;
	fastf_t * ev_pt;
	int	coords;
	int	i, k_index;

	NMG_CK_CNURB(crv);

	coords = RT_NURB_EXTRACT_COORDS( crv->pt_type);

	k_index = rt_nurb_knot_index( &crv->k, param, crv->order);

	pnts = (fastf_t * ) rt_malloc( coords * sizeof( fastf_t) * 
	    crv->c_size, "diff: rt_nurb_c_eval");

	for ( i = 0; i < coords * crv->c_size; i++)
		pnts[i] = crv->ctl_points[i];

	ev_pt = (fastf_t * ) rt_nurb_eval_crv(
	    pnts, crv->order, param, &crv->k, k_index, coords);

	for ( i = 0; i < coords; i++)
		final_value[i] = ev_pt[i];

	rt_free( (char *) pnts, "rt_nurb_c_eval");
}


fastf_t *
rt_nurb_eval_crv( crv, order, param, k_vec, k_index, coords )
register fastf_t *crv;
int	order;
fastf_t	param;
CONST struct knot_vector *k_vec;
int	k_index;
int	coords;
{
	int	i, j;

	if ( order <= 1 ) 
		return
		    (crv + ((k_index) * coords));

	j = k_index;

	while ( j > (k_index - order + 1)) {
		register fastf_t  k1, k2;

		k1 =  k_vec->knots[ (j + order - 1)];

		k2 =  k_vec->knots[ ( j ) ];

		if ((k1 - k2) != 0.0 ) { 		
			for ( i= 0; i < coords; i++) 
			{
 				*((crv + ((j) * coords)) + i) =
					((k1 - param) *
					*((crv + ((j - 1) * coords)) + i) 
					+ (param - k2 ) * *((crv + ((j) * 
					coords)) + i)) / (k1 - k2);
 			} 		
		} 		
		j--;
	} 	
	return rt_nurb_eval_crv( crv, order - 1, param, k_vec, 
		k_index, coords ); 
}


void
rt_nurb_pr_crv( crv, c_size, coords )
fastf_t *crv;
int	c_size;
int	coords;
{
	int	i;

	fprintf(stderr, "\n");
	if (coords == 3)
		for (i = 0; i < c_size; i++)
			fprintf(stderr, "p%d   %f   %f   %f\n", i, *((crv + ((i) * coords))),
			    *((crv + ((i) * coords)) + 1),
			    *((crv + ((i) * coords)) + 2));

	else if (coords == 4)
		for (i = 0; i < c_size; i++)
			fprintf(stderr, "p%d   %f   %f   %f   %f\n", i,
			    *((crv + ((i) * coords))),
			    *((crv + ((i) * coords)) + 1),
			    *((crv + ((i) * coords)) + 2),
			    *((crv + ((i) * coords)) + 3));
}
