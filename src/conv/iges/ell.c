/*                           E L L . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
/** @file ell.c
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
ell( entityno )
int entityno;
{

	fastf_t		xscale=0.0;
	fastf_t		yscale=0.0;
	fastf_t		zscale=0.0;
	point_t		v;			/* the vertex */
	vect_t		xdir;			/* a unit vector */
	vect_t		xvec;			/* vector along x-axis */
	vect_t		ydir;			/* a unit vector */
	vect_t		yvec;			/* vector along y-axis */
	vect_t		zdir;			/* a unit vector */
	vect_t		zvec;			/* vector along z-axis */
	int		sol_num;		/* IGES solid type number */

	/* Default values */
	VSET( v , 0.0 , 0.0 , 0.0 );
	VSET( xdir , 1.0 , 0.0 , 0.0 );
	VSET( zdir , 0.0 , 0.0 , 1.0 );


	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}
	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readcnv( &xscale , "" );
	Readcnv( &yscale , "" );
	Readcnv( &zscale , "" );
	Readcnv( &v[X] , "" );
	Readcnv( &v[Y] , "" );
	Readcnv( &v[Z] , "" );
	Readflt( &xdir[X] , "" );
	Readflt( &xdir[Y] , "" );
	Readflt( &xdir[Z] , "" );
	Readflt( &zdir[X] , "" );
	Readflt( &zdir[Y] , "" );
	Readflt( &zdir[Z] , "" );

	if( xscale <= 0.0 || yscale <= 0.0 || zscale <= 0.0 )
	{
		bu_log( "Illegal parameters for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	/*
	 * Unitize local axis vectors, make a local y-axis vector,
	 * make semi-axis vectors,
	 * and call mk_ell.
	 */

	VUNITIZE(xdir);
	VUNITIZE(zdir);
	VCROSS(ydir, zdir, xdir);		/* Make y-dir vector */

	/* Scale all vectors */

	VSCALE(xvec, xdir, xscale);
	VSCALE(zvec, zdir, zscale);
	VSCALE(yvec, ydir, yscale);


	/* Now the information is handed off to mk_ell(). */

	mk_ell(fdout, dir[entityno]->name, v , xvec , yvec , zvec );

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
