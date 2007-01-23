/*                          C O N E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file cone.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

int
cone( entityno )
int entityno;
{
	fastf_t		rad1=0.0;
	fastf_t		rad2;
	point_t		base;		/* center point of base */
	vect_t		hdir;		/* direction in which to grow height */
	fastf_t		scale_height=0.0;
	fastf_t		x1;
	fastf_t		y1;
	fastf_t		z1;
	fastf_t		x2;
	fastf_t		y2;
	fastf_t		z2;
	int		sol_num;		/* IGES solid type number */

	/* Default values */
	x1 = 0.0;
	y1 = 0.0;
	z1 = 0.0;
	x2 = 0.0;
	y2 = 0.0;
	z2 = 1.0;
	rad2 = 0.0;

	/* Acquiring Data */
	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}
	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readcnv( &scale_height , "" );
	Readcnv( &rad1 , "" );
	Readcnv( &rad2 , "" );
	Readcnv( &x1 , "" );
	Readcnv( &y1 , "" );
	Readcnv( &z1 , "" );
	Readcnv( &x2 , "" );
	Readcnv( &y2 , "" );
	Readcnv( &z2 , "" );

	if( scale_height <= 0.0 || rad1 < rad2 || rad2 < 0.0 )
	{
		bu_log( "Illegal parameters for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		if( scale_height == 0.0 )
		{
			bu_log( "\tCone height is zero!!\n" );
			return( 0 );
		}
		if( rad1 == 0.0 && rad2 == 0.0 )
		{
			bu_log( "\tBoth radii for cone are zero!!!\n" );
			return( 0 );
		}
		if( rad1 < 0.0 )
		{
			bu_log( "\tUsing absloute value of a negative face radius (%f)\n" , rad1 );
			rad1 = (-rad1);
		}
		else if( rad1 == 0.0 )
			rad1 = SMALL_FASTF;

		if( rad2 < 0.0 )
		{
			bu_log( "\tUsing absloute value of a negative face radius (%f)\n" , rad2 );
			rad2 = (-rad2);
		}
		else if( rad2 == 0.0 )
			rad2 = SMALL_FASTF;

		if(scale_height < 0.0 )
		{
			bu_log( "\tUsing absloute value of a negative height (%f)\n" , scale_height );
			bu_log( "\t\tand reversing height direction\n" );
			x2 = (-x2);
			y2 = (-y2);
			z2 = (-z2);
		}
	}


	/*
	 * Making the necessaries. First an id is made for the new entity, then
	 * the x, y, z coordinates for its vertices are converted to a point with
	 * VSET(), and x2, y2, z2 are combined with the scalar height to make a
	 * direction vector.  Now it is necessary to use this information to
	 * construct a, b, c, and d vectors.  These represent the two radius
	 * vectors for the base and the nose respectively.
	 * Finally the libwdb routine that makes an analogous BRL-CAD
	 * solid is called.
	 */

	VSET(base, x1, y1, z1);		/* the center pt of base plate */
	VSET(hdir, x2, y2, z2);
	if( MAGNITUDE(hdir) <= SQRT_SMALL_FASTF )  {
		bu_log("Illegal height vector %g,%g,%g for entity D%07d (%s)\n",
			V3ARGS(hdir),
			dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}
	VUNITIZE(hdir);

	if( mk_cone( fdout, dir[entityno]->name, base, hdir, scale_height, rad1, rad2 ) < 0 )  {
		bu_log("Unable to write entity D%07d (%s)\n",
			dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}
	return( 1 );
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
