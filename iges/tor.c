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

torus( entityno )
int entityno;
{ 
	fastf_t		rad=0.0;
	fastf_t		rad2=0.0;	/* radius of disc */
	point_t		center;		/* center point of torus */
	vect_t		hdir;		/* direction in which to grow height */
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
	Readcnv( &rad , "" );
	Readcnv( &rad2 , "" );
	Readcnv( &x1 , "" );
	Readcnv( &y1 , "" );
	Readcnv( &z1 , "" );
	Readcnv( &x2 , "" );
	Readcnv( &y2 , "" );
	Readcnv( &z2 , "" );

	if( rad <= 0.0 || rad2 <= 0.0 )
	{
		bu_log( "Illegal parameters for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}


	/*
	 * Making the necessaries. First an id is made for the new entity, then
	 * the x, y, z coordinates for its vertices are converted to vectors with
	 * VSET(), and finally the libwdb routine that makes an analogous BRL-CAD
	 * solid is called.
	 */

	VSET(center, x1, y1, z1);
	VSET(hdir, x2, y2, z2);
	VUNITIZE(hdir);

	mk_tor(fdout, dir[entityno]->name, center, hdir, rad, rad2);

	return( 1 );
} 
