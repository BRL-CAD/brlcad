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

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include <math.h>
#include "machine.h"		/* BRL-CAD specific machine data types */
#include "vmath.h"		/* BRL-CAD Vector macros */
#include "nurb.h"
#include "./iges_struct.h"
#include "./iges_extern.h"
#include "wdb.h"

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
	fastf_t *params; /* Surface parameters */
	int	sol_num; /* IGES solid type number */
	int	n1, n2, a, b, c;
	int	i, j;
	fastf_t	*dp;
	struct snurb *b_patch;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
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
	a = n1 + 2 * m1;
	b = n2 + 2 * m2;
	c = (k1 + 1)*(k2 + 1);

	/* Allocate space for the parameters */
	params =(fastf_t *)calloc( 15+a+b+4*c , sizeof(fastf_t ) );

	for( i=10 ; i<15+a+b+4*c ; i++ )
		Readcnv( &params[i] , "" );

	/*  spl_new: Creates a spline surface data structure
	 *	u_order		(e.g. cubic = order 4)
	 *	v_order
	 *	num_u_knots	(e.g. num control points + order)
	 *	num_v_knots
	 *	num_rows	num control points in V direction
	 *	num_cols	num control points in U direction
	 *	point_size	number of values in a point (e.g. 3 or 4)
	 */
	b_patch = rt_nurb_new_snurb(
		m1+1, m2+1,
		n1+2*m1+1, n2+2*m2+1,
		k2+1, k1+1, RT_NURB_MAKE_PT_TYPE( 4 , 2 , 0 ) );

	/* U knot vector */
	for (i = 0; i <= n1+2*m1; i++) {
		b_patch->u_knots.knots[i] = params[10+i];
	}

	/* V knot vector */
	for (i = 0; i <= n2+2*m2; i++) {
		b_patch->v_knots.knots[i] = params[11+a+i];
	}

	/* control points */
	dp = b_patch->ctl_points;
	for (i = 0; i <= k2; i++) {
		for (j = 0; j <= k1; j++) {
			fastf_t	x, y, z, w;
			w = params[12+a+b + i*(k1+1) + j];
			x = params[12+a+b+c + (i*(k1+1) + j)*3];
			y = params[13+a+b+c + (i*(k1+1) + j)*3];
			z = params[14+a+b+c + (i*(k1+1) + j)*3];
			x *= 1000 * w;
			y *= 1000 * w;
			z *= 1000 * w;
			*dp++ = x;
			*dp++ = y;
			*dp++ = z;
			*dp++ = w;
		}
	}

	/* Output the the b_spline through the libwdb interface */
	mk_bsurf(fdout, b_patch);

	/* Free some memory */
	free( params );
	return( 1 );
}
