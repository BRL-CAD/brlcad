/*
 *  Authors -
 *	Phil Dykstra
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

#define	PATCH_COUNT	1

spline( entityno )
int entityno;
{
	int	k1;	/* upper index of first sum */
	int	k2;	/* upper index of second sum */
	int	m1;	/* degree of 1st set of basis functions */
	int	m2;	/* degree of 2nd set of basis functions */
	int	prop1;	/* !0 if closed in first direction */
	int	prop2;	/* !0 if closed in second direction */
	int	prop3;	/* !0 if polynomial (else rational) */
	int	prop4;	/* !0 if periodic in first direction */
	int	prop5;	/* !0 if periodic in second direction */
	int	sol_num; /* IGES solid type number */
	int	n1, n2;
	int	i, j, k;
	int	count=0;
	fastf_t	min_knot;
	fastf_t max_wt;
	struct face_g_snurb *b_patch;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &k1 , "" );
	Readint( &k2 , "" );
	Readint( &m1 , "" );
	Readint( &m2 , "" );
	Readint( &prop1 , "" );
	Readint( &prop2 , "" );
	Readint( &prop3 , "" );
	Readint( &prop4 , "" );
	Readint( &prop5 , "" );

	n1 = k1 - m1 + 1;
	n2 = k2 - m2 + 1;

	/*  spl_new: Creates a spline surface data structure
	 *	u_order		(e.g. cubic = order 4)
	 *	v_order
	 *	num_u	(e.g. num control points + order)
	 *	num_v
	 *	num_rows	num control points in V direction
	 *	num_cols	num control points in U direction
	 *	point_size	number of values in a point (e.g. 3 or 4)
	 */
	b_patch = rt_nurb_new_snurb(
		m1+1, m2+1,
		n1+2*m1+1, n2+2*m2+1,
		k2+1, k1+1, RT_NURB_MAKE_PT_TYPE( 4 , 2 , 0 ), (struct resource *)NULL );

	/* U knot vector */
	min_knot = 0.0;
	for (i = 0; i <= n1+2*m1; i++)
	{
		Readdbl( &b_patch->u.knots[i] , "" );
		if( b_patch->u.knots[i] < min_knot )
			min_knot = b_patch->u.knots[i];
	}

	if( min_knot < 0.0 )
	{
		for (i = 0; i <= n1+2*m1; i++)
		{
			b_patch->u.knots[i] -= min_knot;
		}
	}

	min_knot = 0.0;
	/* V knot vector */
	for (i = 0; i <= n2+2*m2; i++)
	{
		Readdbl( &b_patch->v.knots[i] , "" );
		if( b_patch->v.knots[i] < min_knot )
			min_knot = b_patch->v.knots[i];
	}
	if( min_knot < 0.0 )
	{
		for (i = 0; i <= n1+2*m1; i++)
		{
			b_patch->v.knots[i] -= min_knot;
		}
	}


	/* weights */
	max_wt = 0.0;
	count = 0;
	for( i=0 ; i<=k2 ; i++ )
	{
		for( j=0 ; j<= k1 ; j++ )
		{
			Readdbl( &b_patch->ctl_points[ count*4 + 3 ] , "" );
			if( b_patch->ctl_points[ count*4 + 3 ] > max_wt )
				max_wt = b_patch->ctl_points[ count*4 + 3 ];
			count++;
		}
	}

	/* control points */
	count = 0;
	for (i = 0; i <= k2; i++)
	{
		for (j = 0; j <= k1; j++)
		{
			Readcnv( &b_patch->ctl_points[ count*4 ] , "" );
			Readcnv( &b_patch->ctl_points[ count*4 + 1 ] , "" );
			Readcnv( &b_patch->ctl_points[ count*4 + 2 ] , "" );
			count++;
		}
	}

	/* apply weights */
	count = 0;
	for (i = 0; i <= k2; i++)
	{
		for (j = 0; j <= k1; j++)
		{
			for( k=0 ; k<3 ; k++ )
				b_patch->ctl_points[ count*4 + k ] *= b_patch->ctl_points[ count*4 + 3];
			count++;
		}
	}



	/* Output the the b_spline through the libwdb interface */
	mk_bsurf(fdout, b_patch);

	return( 1 );
}
