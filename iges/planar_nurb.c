/*
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	SLAD/BVLD/VMB
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 *
 *  Checks if nurb surface is planar, returns 1 if so, 0 otherwise
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

int
planar_nurb( entityno )
int entityno;
{
	int		sol_num=0;		/* IGES solid type number */
	int		k1=0,k2=0;		/* Upper index of sums */
	int		m1=0,m2=0;		/* degree */

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	if( sol_num != 128 )
	{
		bu_log( "entity at D%07d is not a B-spline surface\n" , entityno*2 + 1 );
		return( 0 );
	}
	Readint( &k1 , "" );
	Readint( &k2 , "" );
	Readint( &m1 , "" );
	Readint( &m2 , "" );

	if( m1 == 1 && m2 == 1 )
		return( 1 );
	else
		return( 0 );
}

