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

sphere( entityno )
int entityno;
{
	fastf_t		radius=0.0;
	point_t		center;
	fastf_t		x;
	fastf_t		y;
	fastf_t		z;
	int		sol_num;		/* IGES solid type number */

	/* Set Defaults */

	x = 0.0;
	y = 0.0;
	z = 0.0;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}
	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readcnv( &radius , "" );
	Readcnv( &x , "" );
	Readcnv( &y , "" );
	Readcnv( &z , "" );

	if( radius <= 0.0 )
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

	VSET(center, x, y, z);
	mk_sph(fdout, dir[entityno]->name, center, radius);

	return( 1 );
}
