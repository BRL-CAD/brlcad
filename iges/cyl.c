/*
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

cyl( entityno )
int entityno;
{ 
	fastf_t		radius=0.0;
	point_t		base;		/* center point of base */
	vect_t		height;
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
	Readcnv( &radius , "" );
	Readcnv( &x1 , "" );
	Readcnv( &y1 , "" );
	Readcnv( &z1 , "" );
	Readcnv( &x2 , "" );
	Readcnv( &y2 , "" );
	Readcnv( &z2 , "" );

	if( radius <= 0.0 || scale_height <= 0.0 )
	{
		bu_log( "Illegal parameters for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		if( radius == 0.0 )
		{
			bu_log( "\tradius of cylinder is zero!!!\n" );
			return( 0 );
		}
		if( scale_height == 0.0 )
		{
			bu_log( "\theight of cylinder is zero!!!\n" );
			return( 0 );
		}

		if( radius < 0.0 )
		{
			bu_log( "\tUsing the absolute value of a negative radius\n" );
			radius = (-radius);
		}

		if( scale_height < 0.0 )
		{
			bu_log( "\tUsing absolute value of a negative height and reversing axis direction\n" );
			scale_height = (-scale_height);
			x2 = (-x2);
			y2 = (-y2);
			z2 = (-z2);
		}
		
	}


	/*
	 * Making the necessaries. First an id is made for the new entity, then
	 * the x, y, z coordinates for its vertices are converted to vectors with
	 * VSET(), and finally the libwdb routine that makes an analogous BRL-CAD
	 * solid is called.
	 */

	VSET(base, x1, y1, z1);
	VSET(hdir, x2, y2, z2);
	VUNITIZE(hdir);

	/* Multiply the hdir * scale_height to obtain height. */

	VSCALE(height, hdir, scale_height);

	if( mk_rcc(fdout, dir[entityno]->name, base, height, radius) < 0 )  {
		bu_log("Unable to write entity D%07d (%s)\n" ,
			dir[entityno]->direct , dir[entityno]->name );
		return( 0 );
	}
	return( 1 );

}
