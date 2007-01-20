/*               G E T _ C N U R B _ C U R V E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file get_cnurb_curve.c
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	SLAD/BVLD/VMB
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "./iges_struct.h"
#include "./iges_extern.h"

struct edge_g_cnurb *
Get_cnurb_curve( curve_de, linear )
int curve_de;
int *linear;
{
	int i;
	int curve;
	struct edge_g_cnurb *crv;

	*linear = 0;

	curve = (curve_de - 1)/2;
	if( curve >= dirarraylen )
	{
		bu_log( "Get_cnurb_curve: DE=%d is too large, dirarraylen = %d\n", curve_de, dirarraylen );
		return( (struct edge_g_cnurb *)NULL );
	}

	switch( dir[curve]->type )
	{
		case 110:	/* line */
		{
			int pt_type;
			int type;
			point_t pt1;
			point_t start_pt, end_pt;

			Readrec( dir[curve]->param );
			Readint( &type , "" );
			if( type != dir[curve]->type )
			{
				bu_log( "Error in Get_cnurb_curve, looking for curve type %d, found %d\n" ,
					dir[curve]->type , type );
				return( (struct edge_g_cnurb *)NULL );

			}
			/* Read first point */
			for( i=0 ; i<3 ; i++ )
				Readcnv( &pt1[i] , "" );
			MAT4X3PNT( start_pt , *dir[curve]->rot , pt1 );

			/* Read second point */
			for( i=0 ; i<3 ; i++ )
				Readcnv( &pt1[i] , "" );
			MAT4X3PNT( end_pt , *dir[curve]->rot , pt1 );

			/* pt_type for rational UVW coords */
			pt_type = RT_NURB_MAKE_PT_TYPE( 3, 3, 1 );

			/* make a linear edge_g_cnurb (order=2) */
			crv = rt_nurb_new_cnurb( 2, 4, 2, pt_type );

			/* insert control mesh */
			VMOVE( crv->ctl_points, start_pt );
			VMOVE( &crv->ctl_points[3], end_pt );

			/* insert knot values */
			crv->k.knots[0] = 0.0;
			crv->k.knots[1] = 0.0;
			crv->k.knots[2] = 1.0;
			crv->k.knots[3] = 1.0;

			*linear = 1;

			return( crv );
		}
		case 126:	/* B-spline */
			crv = Get_cnurb( curve );
			if( crv->order < 3 )
				*linear = 1;
			return( crv );
		default:
			bu_log( "Not yet handling curves of type: %s\n", iges_type( dir[curve]->type ) );
			break;
	}

	return( (struct edge_g_cnurb *)NULL );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
